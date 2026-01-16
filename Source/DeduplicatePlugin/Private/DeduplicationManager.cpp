/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#include "DeduplicationManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "DeduplicateObjects/EqualNameDeduplication.h"
#include "DeduplicateObjects/EqualSizeDeduplication.h"
#include "DeduplicationFunctionLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/SoftObjectPath.h"

UDeduplicationManager::UDeduplicationManager()
{
	DeduplicationAlgorithms.Add(NewObject<UEqualNameDeduplication>());
	DeduplicationAlgorithms.Add(NewObject<UEqualSizeDeduplication>());
}

bool UDeduplicationManager::FindDuplicateClusterByAssetData(const FAssetData& AssetData, TArray<FDuplicateCluster>& DuplicateClusters, FDuplicateCluster& ResultCluster)
{
	for (FDuplicateCluster& DuplicateCluster : DuplicateClusters)
	{
		if (DuplicateCluster.AssetData == AssetData)
		{
			ResultCluster = DuplicateCluster;
			return true;
		}
	}
	return false;
}

void UDeduplicationManager::EndDeduplicateAssetsAsync(TArray<FDuplicateGroup> NewDeduplicateGroups, UDeduplicateObject* DeduplicationAlgorithm)
{
	if (bShouldStop.GetValue() != 0)
	{
		return;
	}
	
	FScopeLock Lock(&EndDeduplicationLock);
	DeduplicateGroups.Append(NewDeduplicateGroups);
	DeduplicationAlgorithmsInWork.Remove(DeduplicationAlgorithm);
	DeduplicationAlgorithm->OnDeduplicationCompleted.RemoveAll(this);
	CompleteProgress += DeduplicationAlgorithm->AlgorithmComplexity;
	if (DeduplicationAlgorithmsInWork.Num() <= 0)
	{
		StartCreateClusters();
	}
	Lock.Unlock();
}
void UDeduplicationManager::EndEarlyDeduplicateAssetsAsync(TArray<FDuplicateGroup> NewDeduplicateGroups, UDeduplicateObject* EarlyCheckAlgorithm)
{
	if (bShouldStop.GetValue() != 0)
	{
		return;
	}
	
	FScopeLock Lock(&EndEarlyDeduplicationLock);
	EarlyDeduplicateGroups.Append(NewDeduplicateGroups);
	EarlyCheckAlgorithm->OnDeduplicationCompleted.RemoveAll(this);
	CompleteProgress += EarlyCheckAlgorithm->AlgorithmComplexity;
	EarlyCheckDeduplicationAlgorithmsInWork.Remove(EarlyCheckAlgorithm);
	if (EarlyCheckDeduplicationAlgorithmsInWork.Num() <= 0)
	{
		StartDeduplicationAsyncAfterEarlyCheck();
	}
	Lock.Unlock();
}
void UDeduplicationManager::StartAnalyzeAssetsAsync(const TArray<FAssetData>& AssetsToAnalyze)
{
	TArray<FAssetData> AssetsCopy = UDeduplicationFunctionLibrary::FilterRedirects(AssetsToAnalyze);
	bShouldStop.Reset();
	bIsAnalyze = true;
	bCompleteAnalyze = false;
	AnalyzedClusters.Empty();
	DeduplicationAlgorithmsInWork.Empty();
	EarlyCheckDeduplicationAlgorithmsInWork.Empty();
	EarlyDeduplicateGroups.Empty();
	DeduplicateGroups.Empty();
	SummaryComplexity = 0.0f;
	CompleteProgress = 0.0f;

	Async(EAsyncExecution::ThreadPool, [this, AssetsCopy = MoveTemp(AssetsCopy)]()
		{
			if (bShouldStop.GetValue() != 0)
			{
				return;
			}

			SetProgress(0.0);
			
			if (AssetsCopy.Num() == 0)
			{
				AsyncTask(ENamedThreads::GameThread, [this]()
					{
						AnalyzedClusters.Empty();
						bCompleteAnalyze = true;
						bIsAnalyze = false;
						TArray<FDuplicateCluster> AllClusters = GetAllClusters();
						OnDeduplicationAnalyzeCompleted.Broadcast(AllClusters);
						UE_LOG(LogTemp, Log, TEXT("Deduplication completed: no assets to analyze"));
					});
				return;
			}
			
			TArray<FDuplicateGroup> AllGroups;

			TMap<UClass*, TArray<FAssetData>> AssetsByClass;
			for (const FAssetData& Asset : AssetsCopy)
			{
				UClass* AssetClass = Asset.GetClass();
				AssetsByClass.FindOrAdd(AssetClass).Add(Asset);
			}

			TArray<TPair<UClass*, TArray<FAssetData>>> ClassGroups;
			ClassGroups.Reserve(AssetsByClass.Num());
			for (auto& Pair : AssetsByClass)
			{
				ClassGroups.Add(TPair<UClass*, TArray<FAssetData>>(Pair.Key, MoveTemp(Pair.Value)));
			}

			SummaryComplexity = 0.0f;
			AsyncTask(ENamedThreads::GameThread, [this, ClassGroups]() mutable
				{
					if (bShouldStop.GetValue() != 0)
					{
						return;
					}
					
					if (ClassGroups.Num() == 0)
					{
						AnalyzedClusters.Empty();
						bCompleteAnalyze = true;
						bIsAnalyze = false;
						TArray<FDuplicateCluster> AllClusters = GetAllClusters();
						OnDeduplicationAnalyzeCompleted.Broadcast(AllClusters);
						UE_LOG(LogTemp, Log, TEXT("Deduplication completed: no valid asset classes"));
						return;
					}
					
					if (EarlyCheckDeduplicationAlgorithms.Num() > 0)
					{
						for (UDeduplicateObject* EarlyCheckPrototype : EarlyCheckDeduplicationAlgorithms)
						{
							if (bShouldStop.GetValue() != 0)
							{
								break;
							}
							if (!EarlyCheckPrototype) continue;
							for (TPair<UClass*, TArray<FAssetData>>& ClassGroup : ClassGroups)
							{
								if (EarlyCheckPrototype->IsAssetClassAllowed(ClassGroup.Key))
								{
									SummaryComplexity += EarlyCheckPrototype->CalculateComplexity(ClassGroup.Value);
								}
							}
						}
						for (UDeduplicateObject* EarlyCheckPrototype : EarlyCheckDeduplicationAlgorithms)
						{
							if (bShouldStop.GetValue() != 0)
							{
								break;
							}
							if (!EarlyCheckPrototype) continue;
							for (TPair<UClass*, TArray<FAssetData>>& ClassGroup : ClassGroups)
							{
								if (bShouldStop.GetValue() != 0)
								{
									break;
								}
								if (!EarlyCheckPrototype->IsAssetClassAllowed(ClassGroup.Key))
								{
									continue;
								}

								TSharedRef<TArray<FAssetData>, ESPMode::ThreadSafe> SharedClassAssets = MakeShared<TArray<FAssetData>, ESPMode::ThreadSafe>(ClassGroup.Value);

								UDeduplicateObject* NewEarlyCheckAlgorithm = DuplicateObject(EarlyCheckPrototype, this);
								NewEarlyCheckAlgorithm->OwnerManager = this;
								NewEarlyCheckAlgorithm->OnDeduplicationProgressCompleted.AddUObject(this, &UDeduplicationManager::BindUpdateDeduplicationProgressCompleted);
								NewEarlyCheckAlgorithm->OnDeduplicationCompleted.AddUObject(this, &UDeduplicationManager::EndEarlyDeduplicateAssetsAsync);
								EarlyCheckDeduplicationAlgorithmsInWork.Add(NewEarlyCheckAlgorithm);

								Async(EAsyncExecution::ThreadPool, [this, SharedClassAssets, NewEarlyCheckAlgorithm]()
									{
										if (bShouldStop.GetValue() == 0)
										{
											NewEarlyCheckAlgorithm->FindDuplicates(SharedClassAssets.Get());
										}
									});
							}
						}
						
						if (EarlyCheckDeduplicationAlgorithmsInWork.Num() == 0)
						{
							StartDeduplicationAsyncAfterEarlyCheck();
						}
					}
					else
					{
						for (UDeduplicateObject* DeduplicationAlgorithm : DeduplicationAlgorithms)
						{
							if (bShouldStop.GetValue() != 0)
							{
								break;
							}
							if (!DeduplicationAlgorithm) continue;
							for (TPair<UClass*, TArray<FAssetData>>& ClassGroup : ClassGroups)
							{
								if (DeduplicationAlgorithm->IsAssetClassAllowed(ClassGroup.Key))
								{
									SummaryComplexity += DeduplicationAlgorithm->CalculateComplexity(ClassGroup.Value);
								}
							}
						}
						for (UDeduplicateObject* AlgorithmPrototype : DeduplicationAlgorithms)
						{
							if (bShouldStop.GetValue() != 0)
							{
								break;
							}
							if (!AlgorithmPrototype) continue;
							for (TPair<UClass*, TArray<FAssetData>>& ClassGroup : ClassGroups)
							{
								if (bShouldStop.GetValue() != 0)
								{
									break;
								}
								if (!AlgorithmPrototype->IsAssetClassAllowed(ClassGroup.Key))
								{
									continue;
								}

								TSharedRef<TArray<FAssetData>, ESPMode::ThreadSafe> SharedClassAssets = MakeShared<TArray<FAssetData>, ESPMode::ThreadSafe>(ClassGroup.Value);

								UDeduplicateObject* NewAlgorithm = DuplicateObject(AlgorithmPrototype, this);
								NewAlgorithm->OwnerManager = this;
								NewAlgorithm->OnDeduplicationProgressCompleted.AddUObject(this, &UDeduplicationManager::BindUpdateDeduplicationProgressCompleted);
								NewAlgorithm->OnDeduplicationCompleted.AddUObject(this, &UDeduplicationManager::EndDeduplicateAssetsAsync);
								DeduplicationAlgorithmsInWork.Add(NewAlgorithm);

								Async(EAsyncExecution::ThreadPool, [this, SharedClassAssets, NewAlgorithm]()
									{
										if (bShouldStop.GetValue() == 0)
										{
											NewAlgorithm->FindDuplicates(SharedClassAssets.Get());
										}
									});
							}
						}
						
						if (DeduplicationAlgorithmsInWork.Num() == 0)
						{
							StartCreateClusters();
						}
					}
				});
		});
}


void UDeduplicationManager::StartDeduplicationAsyncAfterEarlyCheck()
{
	if (bShouldStop.GetValue() != 0)
	{
		return;
	}
	
	for (UDeduplicateObject* Algorithm : DeduplicationAlgorithms)
	{
		if (bShouldStop.GetValue() != 0)
		{
			break;
		}
		if (!Algorithm) continue;

		AsyncTask(ENamedThreads::GameThread, [this, Algorithm]() mutable
			{
			if (bShouldStop.GetValue() != 0)
			{
				return;
			}
			
			for (const FDuplicateGroup& DuplicateGroup : EarlyDeduplicateGroups)
			{
				if (bShouldStop.GetValue() != 0)
				{
					break;
				}
				UDeduplicateObject* NewAlgorithm = DuplicateObject(Algorithm, this);
				NewAlgorithm->OwnerManager = this;
				NewAlgorithm->OnDeduplicationProgressCompleted.AddUObject(this, &UDeduplicationManager::BindUpdateDeduplicationProgressCompleted);
				NewAlgorithm->OnDeduplicationCompleted.AddUObject(this, &UDeduplicationManager::EndDeduplicateAssetsAsync);
				DeduplicationAlgorithmsInWork.Add(NewAlgorithm);

				Async(EAsyncExecution::ThreadPool, [Algorithm, DuplicateGroup, NewAlgorithm, this]()
					{
						if (bShouldStop.GetValue() == 0)
						{
							NewAlgorithm->FindDuplicates(DuplicateGroup.DuplicateAssets);
						}
					});
			}
			});

	}
}


void UDeduplicationManager::StartCreateClusters()
{
	if (bShouldStop.GetValue() != 0)
	{
		return;
	}
	
	TArray<FDuplicateCluster> ResultClusters;
	SetProgress(0.99);

	int Counter = 0;
	for (const FDuplicateGroup& DuplicateGroup : DeduplicateGroups)
	{
		if (bShouldStop.GetValue() != 0)
		{
			break;
		}
		
		const float GroupScore = DuplicateGroup.ConfidenceScore;
		Counter++;
		SetProgress(0.99 + static_cast<float>(Counter)/ static_cast<float>(DeduplicateGroups.Num()) * 0.01);
		for (const FAssetData& CenterAsset : DuplicateGroup.DuplicateAssets)
		{
			FDuplicateCluster* FoundCluster = ResultClusters.FindByKey(CenterAsset);

			if (FoundCluster == nullptr)
			{
				FDuplicateCluster NewCluster;
				NewCluster.AssetData = CenterAsset;
				NewCluster.ClusterScore = GroupScore;

				for (const FAssetData& OtherAsset : DuplicateGroup.DuplicateAssets)
				{
					if (OtherAsset == CenterAsset)
						continue;

					FDeduplicationAssetStruct Entry;
					Entry.DuplicateAsset = OtherAsset;
					Entry.DeduplicationAssetScore = GroupScore;
					NewCluster.DuplicateAssets.Add(MoveTemp(Entry));
				}

				ResultClusters.Add(MoveTemp(NewCluster));
			}
			else
			{
				FDuplicateCluster& Cluster = *FoundCluster;

				if (CombinationScoreMethod == ECombinationScoreMethod::Add)
				{
					Cluster.ClusterScore += GroupScore;
				}
				else
				{
					Cluster.ClusterScore *= GroupScore;
				}

				for (const FAssetData& OtherAsset : DuplicateGroup.DuplicateAssets)
				{
					if (OtherAsset == CenterAsset)
						continue;

					int32 FoundIndex = INDEX_NONE;
					for (int32 i = 0; i < Cluster.DuplicateAssets.Num(); ++i)
					{
						if (Cluster.DuplicateAssets[i].DuplicateAsset == OtherAsset)
						{
							FoundIndex = i;
							break;
						}
					}

					if (FoundIndex != INDEX_NONE)
					{
						float& ExistingScore = Cluster.DuplicateAssets[FoundIndex].DeduplicationAssetScore;

						if (CombinationScoreMethod == ECombinationScoreMethod::Add)
						{
							ExistingScore += GroupScore;
						}
						else
						{
							ExistingScore *= GroupScore;
						}
					}
					else
					{
						FDeduplicationAssetStruct NewEntry;
						NewEntry.DuplicateAsset = OtherAsset;
						NewEntry.DeduplicationAssetScore = GroupScore;
						Cluster.DuplicateAssets.Add(MoveTemp(NewEntry));
					}
				}
			}
		}
	}
	SetProgress(0.9999);

	/*
	int Count = 0;
	while (ResultClusters.IsValidIndex(Count))
	{
		if (bShouldStop.GetValue() != 0)
		{
			break;
		}
		
		if (ResultClusters[Count].ClusterScore)
		{
			ResultClusters.RemoveAt(Count);
			continue;
		}
		else
		{
			
			int Index = 0;
			while(ResultClusters[Count].DuplicateAssets.IsValidIndex(Index))
			{
				if (bShouldStop.GetValue() != 0)
				{
					break;
				}
				
				if (ResultClusters[Count].DuplicateAssets[Index].DeduplicationAssetScore < GroupConfidenceThreshold)
				{
					ResultClusters[Count].DuplicateAssets.RemoveAt(Index);
				}
				else
				{
					Index += 1;
				}
			}
			if (ResultClusters[Count].DuplicateAssets.Num() <= 0)
			{
				ResultClusters.RemoveAt(Count);
				continue;
			}
		}
		Count++;
	}*/

	if (bShouldStop.GetValue() != 0)
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
			{
				bCompleteAnalyze = false;
				bIsAnalyze = false;
				TArray<FDuplicateCluster> EmptyClusters;
				OnDeduplicationAnalyzeCompleted.Broadcast(EmptyClusters);
				UE_LOG(LogTemp, Log, TEXT("Deduplication stopped by user"));
			});
		return;
	}

	SetProgress(1.0);

	AsyncTask(ENamedThreads::GameThread, [this, ResultClusters = MoveTemp(ResultClusters)]() mutable
		{
			if (bShouldStop.GetValue() != 0)
			{
				bCompleteAnalyze = false;
				bIsAnalyze = false;
				return;
			}
			
			AnalyzedClusters = MoveTemp(ResultClusters);
			bCompleteAnalyze = true;
			bIsAnalyze = false;
			TArray<FDuplicateCluster> AllClusters = GetAllClusters();
			OnDeduplicationAnalyzeCompleted.Broadcast(AllClusters);
			UE_LOG(LogTemp, Log, TEXT("Deduplication completed: %d analyzed clusters, %d total clusters"), AnalyzedClusters.Num(), AllClusters.Num());
		});
}



void UDeduplicationManager::BindUpdateDeduplicationProgressCompleted()
{
	TArray<UDeduplicateObject*> EarlyCheckCopy;
	TArray<UDeduplicateObject*> EarlyInWorkCopy;
	TArray<UDeduplicateObject*> InWorkCopy;

	{
		FScopeLock Lock(&UpdateDeduplicationProgressCompletedMutex);
		EarlyCheckCopy = EarlyCheckDeduplicationAlgorithms;
		EarlyInWorkCopy = EarlyCheckDeduplicationAlgorithmsInWork;
		InWorkCopy = DeduplicationAlgorithmsInWork;
	} 

	auto SumProgressAndComplexity = [](const TArray<UDeduplicateObject*>& List, float& OutProgress, float& OutComplexity)
		{
			OutProgress = 0.0f;
			OutComplexity = 0.0f;
			for (UDeduplicateObject* DeduplicateObject : List)
			{
				OutProgress += DeduplicateObject->Progress;
				OutComplexity += DeduplicateObject->AlgorithmComplexity;
			}
		};

	const float Epsilon = KINDA_SMALL_NUMBER; 
	float Progress = 0.0f;
	float Complexity = 0.0f;

	if (EarlyCheckCopy.Num() > 0)
	{
		if (EarlyInWorkCopy.Num() > 0)
		{
			SumProgressAndComplexity(EarlyInWorkCopy, Progress, Complexity);
			float Ratio = (Complexity > Epsilon) ? (Progress / Complexity) : 0.0f;
			float Base = (SummaryComplexity > Epsilon) ? (CompleteProgress / SummaryComplexity) : 0.0f;
			SetProgress(FMath::Clamp(((Base + Ratio) * 0.5f) * 0.99, 0.0f, 0.99f));
		}
		else if (InWorkCopy.Num() > 0)
		{
			SumProgressAndComplexity(InWorkCopy, Progress, Complexity);
			float Ratio = (Complexity > Epsilon) ? (Progress / Complexity) : 0.0f;
			float Base = (SummaryComplexity > Epsilon) ? (CompleteProgress / SummaryComplexity) : 0.0f;
			SetProgress(FMath::Clamp(((Base + Ratio) * 0.5f + 0.5f) * 0.99, 0.0f, 0.99f));
		}
		else
		{
			float Base = (SummaryComplexity > Epsilon) ? (CompleteProgress / SummaryComplexity) : 0.0f;
			SetProgress(FMath::Clamp(Base * 0.99, 0.0f, 0.99f));
		}
	}
	else
	{
		SumProgressAndComplexity(InWorkCopy, Progress, Complexity);
		float Ratio = (Complexity > Epsilon) ? (Progress / Complexity) : 0.0f;
		float Base = (SummaryComplexity > Epsilon) ? (CompleteProgress / SummaryComplexity) : 0.0f;
		SetProgress(FMath::Clamp((Base + Ratio) * 0.99, 0.0f, 0.99f));
	}
}

void UDeduplicationManager::SetProgress(float Progress)
{
	AsyncTask(ENamedThreads::GameThread, [this, Progress]() mutable
		{
			ProgressValue = Progress;
		});
}

TArray<FDuplicateCluster> UDeduplicationManager::FindMostPriorityDuplicateClusterByPath(FString Path)
{
	TArray<FDuplicateCluster> Results;
	FString Pattern = FString::Format(TEXT("{0}"), { Path });
	TArray<FDuplicateCluster> AllClusters = GetAllClusters();

	for (const FDuplicateCluster& Cluster : AllClusters)
	{
		const FString PackagePath = Cluster.AssetData.PackagePath.ToString();
		const FString ObjectPath = Cluster.AssetData.GetSoftObjectPath().ToString();
		const FString PackageName = Cluster.AssetData.PackageName.ToString();

		if (PackagePath.StartsWith(Pattern) || ObjectPath.StartsWith(Pattern) || PackageName.StartsWith(Pattern))
		{
			Results.Add(Cluster);
			continue;
		}

		for (const FDeduplicationAssetStruct& DuplicateStruct : Cluster.DuplicateAssets)
		{
			if (DuplicateStruct.DeduplicationAssetScore < ConfidenceThreshold)
			{
				continue;
			}

			const FString DuplicatePackagePath = DuplicateStruct.DuplicateAsset.PackagePath.ToString();
			const FString DuplicateObjectPath = DuplicateStruct.DuplicateAsset.GetObjectPathString();
			const FString DuplicatePackageName = DuplicateStruct.DuplicateAsset.PackageName.ToString();

			if (DuplicatePackagePath.StartsWith(Pattern) || DuplicateObjectPath.StartsWith(Pattern) || DuplicatePackageName.StartsWith(Pattern))
			{
				Results.Add(Cluster);
				break;
			}
		}
	}

	Results.Sort([](const FDuplicateCluster& A, const FDuplicateCluster& B)
		{
			return A.ClusterScore > B.ClusterScore;
		});

	return Results;
}


TArray<FDuplicateCluster> UDeduplicationManager::GetAllClusters() const
{
	TArray<FDuplicateCluster> AllClusters = SavedClusters;
	AllClusters.Append(AnalyzedClusters);
	return AllClusters;
}

TArray<FDuplicateCluster> UDeduplicationManager::GetClustersByFolder(FString Folder)
{
	TArray<FDuplicateCluster> ResultClusters;
	TArray<FDuplicateCluster> AllClusters = GetAllClusters();
	for (FDuplicateCluster Cluster : AllClusters)
	{
		if (Cluster.AssetData.PackagePath.ToString().Contains(Folder))
		{
			ResultClusters.Add(Cluster);
		}
	}
	return ResultClusters;
}

FString UDeduplicationManager::GetSaveDirectory()
{
	return FPaths::ProjectSavedDir() / TEXT("DeduplicationResults");
}

FString UDeduplicationManager::GetSaveFilePath(const FString& SaveName) const
{
	FString SafeFileName = SaveName;
	SafeFileName.ReplaceInline(TEXT(" "), TEXT("_"));
	SafeFileName.ReplaceInline(TEXT("/"), TEXT("_"));
	SafeFileName.ReplaceInline(TEXT("\\"), TEXT("_"));
	SafeFileName.ReplaceInline(TEXT(":"), TEXT("_"));
	SafeFileName.ReplaceInline(TEXT("*"), TEXT("_"));
	SafeFileName.ReplaceInline(TEXT("?"), TEXT("_"));
	SafeFileName.ReplaceInline(TEXT("\""), TEXT("_"));
	SafeFileName.ReplaceInline(TEXT("<"), TEXT("_"));
	SafeFileName.ReplaceInline(TEXT(">"), TEXT("_"));
	SafeFileName.ReplaceInline(TEXT("|"), TEXT("_"));
	
	return GetSaveDirectory() / (SafeFileName + TEXT(".json"));
}

void UDeduplicationManager::SaveResults(const FString& SaveName, bool bIncludeCurrentClusters)
{
	if (SaveName.IsEmpty())
	{
		return;
	}

	FSavedDeduplicationResults SavedResults;
	SavedResults.SaveName = SaveName;
	SavedResults.SaveDate = FDateTime::Now();

	if (bIncludeCurrentClusters)
	{
		SavedResults.Clusters = GetAllClusters();
	}
	else
	{
		SavedResults.Clusters = SavedClusters;
	}

	FString SaveDirectory = GetSaveDirectory();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*SaveDirectory))
	{
		PlatformFile.CreateDirectoryTree(*SaveDirectory);
	}

	FString SaveFilePath = GetSaveFilePath(SaveName);

	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	RootObject->SetStringField(TEXT("SaveName"), SavedResults.SaveName);
	RootObject->SetStringField(TEXT("SaveDate"), SavedResults.SaveDate.ToString());

	TArray<TSharedPtr<FJsonValue>> ClustersArray;
	for (const FDuplicateCluster& Cluster : SavedResults.Clusters)
	{
		TSharedPtr<FJsonObject> ClusterObject = MakeShareable(new FJsonObject);
		ClusterObject->SetStringField(TEXT("AssetPath"), Cluster.AssetData.GetObjectPathString());
		ClusterObject->SetNumberField(TEXT("ClusterScore"), Cluster.ClusterScore);

		TArray<TSharedPtr<FJsonValue>> DuplicateAssetsArray;
		for (const FDeduplicationAssetStruct& DuplicateAsset : Cluster.DuplicateAssets)
		{
			TSharedPtr<FJsonObject> DuplicateAssetObject = MakeShareable(new FJsonObject);
			DuplicateAssetObject->SetStringField(TEXT("AssetPath"), DuplicateAsset.DuplicateAsset.GetObjectPathString());
			DuplicateAssetObject->SetNumberField(TEXT("Score"), DuplicateAsset.DeduplicationAssetScore);
			DuplicateAssetsArray.Add(MakeShareable(new FJsonValueObject(DuplicateAssetObject)));
		}
		ClusterObject->SetArrayField(TEXT("DuplicateAssets"), DuplicateAssetsArray);
		ClustersArray.Add(MakeShareable(new FJsonValueObject(ClusterObject)));
	}
	RootObject->SetArrayField(TEXT("Clusters"), ClustersArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	FFileHelper::SaveStringToFile(OutputString, *SaveFilePath);
}

bool UDeduplicationManager::LoadResults(const FString& SaveName)
{
	if (SaveName.IsEmpty())
	{
		return false;
	}

	FString SaveFilePath = GetSaveFilePath(SaveName);
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*SaveFilePath))
	{
		return false;
	}

	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *SaveFilePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContents);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	TArray<FDuplicateCluster> LoadedClusters;
	const TArray<TSharedPtr<FJsonValue>>* ClustersArrayPtr = nullptr;
	if (RootObject->TryGetArrayField(TEXT("Clusters"), ClustersArrayPtr))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		for (const TSharedPtr<FJsonValue>& ClusterValue : *ClustersArrayPtr)
		{
			TSharedPtr<FJsonObject> ClusterObject = ClusterValue->AsObject();
			if (!ClusterObject.IsValid())
			{
				continue;
			}

			FDuplicateCluster NewCluster;
			FString AssetPathString;
			if (ClusterObject->TryGetStringField(TEXT("AssetPath"), AssetPathString))
			{
				FSoftObjectPath SoftObjectPath(AssetPathString);
				FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(SoftObjectPath);
				if (AssetData.IsValid())
				{
					NewCluster.AssetData = AssetData;
				}
			}

			ClusterObject->TryGetNumberField(TEXT("ClusterScore"), NewCluster.ClusterScore);

			const TArray<TSharedPtr<FJsonValue>>* DuplicateAssetsArrayPtr = nullptr;
			if (ClusterObject->TryGetArrayField(TEXT("DuplicateAssets"), DuplicateAssetsArrayPtr))
			{
				for (const TSharedPtr<FJsonValue>& DuplicateAssetValue : *DuplicateAssetsArrayPtr)
				{
					TSharedPtr<FJsonObject> DuplicateAssetObject = DuplicateAssetValue->AsObject();
					if (!DuplicateAssetObject.IsValid())
					{
						continue;
					}

					FDeduplicationAssetStruct NewDuplicateAsset;
					FString DuplicateAssetPathString;
					if (DuplicateAssetObject->TryGetStringField(TEXT("AssetPath"), DuplicateAssetPathString))
					{
						FSoftObjectPath DuplicateSoftObjectPath(DuplicateAssetPathString);
						FAssetData DuplicateAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(DuplicateSoftObjectPath);
						if (DuplicateAssetData.IsValid())
						{
							if (NewDuplicateAsset.DuplicateAsset == NewCluster.AssetData)
							{
								continue;
							}
							NewDuplicateAsset.DuplicateAsset = DuplicateAssetData;
						}
					}
					DuplicateAssetObject->TryGetNumberField(TEXT("Score"), NewDuplicateAsset.DeduplicationAssetScore);
					NewCluster.DuplicateAssets.Add(NewDuplicateAsset);
				}
			}

			if (NewCluster.AssetData.IsValid())
			{
				LoadedClusters.Add(NewCluster);
			}
		}
	}

	for (const FDuplicateCluster& ClusterToAdd : LoadedClusters)
	{
		FDuplicateCluster* ExistingCluster = SavedClusters.FindByKey(ClusterToAdd.AssetData);

		if (ExistingCluster == nullptr)
		{
			SavedClusters.Add(ClusterToAdd);
		}
		else
		{
			if (CombinationScoreMethod == ECombinationScoreMethod::Add)
			{
				ExistingCluster->ClusterScore += ClusterToAdd.ClusterScore;
			}
			else
			{
				ExistingCluster->ClusterScore *= ClusterToAdd.ClusterScore;
			}

			for (const FDeduplicationAssetStruct& DuplicateAssetToAdd : ClusterToAdd.DuplicateAssets)
			{
				if (ExistingCluster->AssetData == DuplicateAssetToAdd.DuplicateAsset)
				{
					continue;
				}
				int32 FoundIndex = ExistingCluster->DuplicateAssets.IndexOfByKey(DuplicateAssetToAdd.DuplicateAsset);
				if (FoundIndex == INDEX_NONE)
				{
					ExistingCluster->DuplicateAssets.Add(DuplicateAssetToAdd);
				}
				else
				{
					if (CombinationScoreMethod == ECombinationScoreMethod::Add)
					{
						ExistingCluster->DuplicateAssets[FoundIndex].DeduplicationAssetScore += DuplicateAssetToAdd.DeduplicationAssetScore;
					}
					else
					{
						ExistingCluster->DuplicateAssets[FoundIndex].DeduplicationAssetScore *= DuplicateAssetToAdd.DeduplicationAssetScore;
					}
				}
			}
		}
	}

	TArray<FDuplicateCluster> AllClusters = GetAllClusters();
	OnDeduplicationAnalyzeCompleted.Broadcast(AllClusters);
	return true;
}

void UDeduplicationManager::DeleteSavedResults(const FString& SaveName)
{
	if (SaveName.IsEmpty())
	{
		return;
	}

	FString SaveFilePath = GetSaveFilePath(SaveName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*SaveFilePath))
	{
		PlatformFile.DeleteFile(*SaveFilePath);
	}
}

TArray<FString> UDeduplicationManager::GetSavedResultsList() const
{
	TArray<FString> ResultList;

	FString SaveDirectory = GetSaveDirectory();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*SaveDirectory))
	{
		return ResultList;
	}
	FString SearchPattern = SaveDirectory / TEXT("*.json");
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *SearchPattern, true, false);

	for (const FString& FilePath : FoundFiles)
	{
		FString FileName = FPaths::GetBaseFilename(FilePath);
		ResultList.Add(FileName);
	}

	return ResultList;
}

void UDeduplicationManager::AddClustersToCurrent(const TArray<FDuplicateCluster>& ClustersToAdd)
{
	for (const FDuplicateCluster& ClusterToAdd : ClustersToAdd)
	{
		FDuplicateCluster* ExistingCluster = AnalyzedClusters.FindByKey(ClusterToAdd.AssetData);

		if (ExistingCluster == nullptr)
		{
			AnalyzedClusters.Add(ClusterToAdd);
		}
		else
		{
			if (CombinationScoreMethod == ECombinationScoreMethod::Add)
			{
				ExistingCluster->ClusterScore += ClusterToAdd.ClusterScore;
			}
			else
			{
				ExistingCluster->ClusterScore *= ClusterToAdd.ClusterScore;
			}

			for (const FDeduplicationAssetStruct& DuplicateAssetToAdd : ClusterToAdd.DuplicateAssets)
			{
				int32 FoundIndex = ExistingCluster->DuplicateAssets.IndexOfByKey(DuplicateAssetToAdd.DuplicateAsset);
				if (FoundIndex == INDEX_NONE)
				{
					ExistingCluster->DuplicateAssets.Add(DuplicateAssetToAdd);
				}
				else
				{
					if (CombinationScoreMethod == ECombinationScoreMethod::Add)
					{
						ExistingCluster->DuplicateAssets[FoundIndex].DeduplicationAssetScore += DuplicateAssetToAdd.DeduplicationAssetScore;
					}
					else
					{
						ExistingCluster->DuplicateAssets[FoundIndex].DeduplicationAssetScore *= DuplicateAssetToAdd.DeduplicationAssetScore;
					}
				}
			}
		}
	}

	TArray<FDuplicateCluster> AllClusters = GetAllClusters();
	OnDeduplicationAnalyzeCompleted.Broadcast(AllClusters);
}

void UDeduplicationManager::StopAnalyze()
{
	bShouldStop.Increment();
	bIsAnalyze = false;
	
	TArray<UDeduplicateObject*> AlgorithmsToStop;
	{
		FScopeLock Lock(&EndDeduplicationLock);
		AlgorithmsToStop = DeduplicationAlgorithmsInWork;
	}
	
	{
		FScopeLock Lock(&EndEarlyDeduplicationLock);
		AlgorithmsToStop.Append(EarlyCheckDeduplicationAlgorithmsInWork);
	}
	
	for (UDeduplicateObject* Algorithm : AlgorithmsToStop)
	{
		if (Algorithm != nullptr && Algorithm->Handle.IsValid())
		{
			Algorithm->Handle->CancelHandle();
		}
	}
	
	{
		FScopeLock Lock(&EndDeduplicationLock);
		DeduplicationAlgorithmsInWork.Empty();
	}
	
	{
		FScopeLock Lock(&EndEarlyDeduplicationLock);
		EarlyCheckDeduplicationAlgorithmsInWork.Empty();
	}
	
	ProgressValue = 0.0f;
}