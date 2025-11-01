// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeduplicationManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "DeduplicateObjects/EqualNameDeduplication.h"
#include "DeduplicateObjects/EqualSizeDeduplication.h"
#include "DeduplicationFunctionLibrary.h"

UDeduplicationManager::UDeduplicationManager()
{
	DeduplicationAlgorithms.Add(NewObject<UEqualNameDeduplication>());
	DeduplicationAlgorithms.Add(NewObject<UEqualSizeDeduplication>());
}

bool UDeduplicationManager::FindDuplicateClusterByAssetData(const FAssetData& AssetData, TArray<FDuplicateCluster>& DuplicateClasters, FDuplicateCluster& ResultClaster)
{
	for (FDuplicateCluster& DuplicateClaster : DuplicateClasters)
	{
		if (DuplicateClaster.AssetData == AssetData)
		{
			ResultClaster = DuplicateClaster;
			return true;
		}
	}
	return false;
}

void UDeduplicationManager::EndDeduplicateAssetsAsync(TArray<FDuplicateGroup> NewDeduplicateGroups, UDeduplicateObject* DeduplicationAlgorithm)
{
	FScopeLock Lock(&EndDeduplicationLock);
	DeduplicateGroups.Append(NewDeduplicateGroups);
	DeduplicationAlgorithmsInWork.Remove(DeduplicationAlgorithm);
	DeduplicationAlgorithm->OnDeduplicationCompleted.RemoveAll(this);
	CompleteProgress += DeduplicationAlgorithm->AlgorithmComplexity;
	if (DeduplicationAlgorithmsInWork.Num() <= 0)
	{
		StartCreateClasters();
	}
	Lock.Unlock();
}
void UDeduplicationManager::EndEarlyDeduplicateAssetsAsync(TArray<FDuplicateGroup> NewDeduplicateGroups, UDeduplicateObject* EarlyCheckAlgorithm)
{
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
	bIsAnalyze = true;
	DeduplicationAlgorithmsInWork.Empty();
	EarlyCheckDeduplicationAlgorithmsInWork.Empty();
	EarlyDeduplicateGroups.Empty();
	DeduplicateGroups.Empty();
	SummaryComplexity = 0.0f;
	CompleteProgress = 0.0f;

	Async(EAsyncExecution::ThreadPool, [this, AssetsCopy = MoveTemp(AssetsCopy)]()
		{
			SetProgress(0.0);
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
					if (EarlyCheckDeduplicationAlgorithms.Num() > 0)
					{
						for (UDeduplicateObject* EarlyCheckPrototype : EarlyCheckDeduplicationAlgorithms)
						{
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
							if (!EarlyCheckPrototype) continue;
							for (TPair<UClass*, TArray<FAssetData>>& ClassGroup : ClassGroups)
							{
								if (!EarlyCheckPrototype->IsAssetClassAllowed(ClassGroup.Key))
								{
									continue;
								}

								TSharedRef<TArray<FAssetData>, ESPMode::ThreadSafe> SharedClassAssets = MakeShared<TArray<FAssetData>, ESPMode::ThreadSafe>(ClassGroup.Value);

								UDeduplicateObject* NewEarlyCheckAlgorithm = DuplicateObject(EarlyCheckPrototype, this);
								NewEarlyCheckAlgorithm->OnDeduplicationProgressCompleted.AddUObject(this, &UDeduplicationManager::BindUpdateDeduplicationProgressCompleted);
								NewEarlyCheckAlgorithm->OnDeduplicationCompleted.AddUObject(this, &UDeduplicationManager::EndEarlyDeduplicateAssetsAsync);
								EarlyCheckDeduplicationAlgorithmsInWork.Add(NewEarlyCheckAlgorithm);

								Async(EAsyncExecution::ThreadPool, [this, SharedClassAssets, NewEarlyCheckAlgorithm]()
									{
										NewEarlyCheckAlgorithm->FindDuplicates(SharedClassAssets.Get());
									});
							}
						}
					}
					else
					{
						for (UDeduplicateObject* DeduplicationAlgorithm : DeduplicationAlgorithms)
						{
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
							if (!AlgorithmPrototype) continue;
							for (TPair<UClass*, TArray<FAssetData>>& ClassGroup : ClassGroups)
							{
								if (!AlgorithmPrototype->IsAssetClassAllowed(ClassGroup.Key))
								{
									continue;
								}

								TSharedRef<TArray<FAssetData>, ESPMode::ThreadSafe> SharedClassAssets = MakeShared<TArray<FAssetData>, ESPMode::ThreadSafe>(ClassGroup.Value);

								UDeduplicateObject* NewAlgorithm = DuplicateObject(AlgorithmPrototype, this);

								NewAlgorithm->OnDeduplicationProgressCompleted.AddUObject(this, &UDeduplicationManager::BindUpdateDeduplicationProgressCompleted);
								NewAlgorithm->OnDeduplicationCompleted.AddUObject(this, &UDeduplicationManager::EndDeduplicateAssetsAsync);
								DeduplicationAlgorithmsInWork.Add(NewAlgorithm);

								Async(EAsyncExecution::ThreadPool, [this, SharedClassAssets, NewAlgorithm]()
									{
										NewAlgorithm->FindDuplicates(SharedClassAssets.Get());
									});
							}
						}
					}
				});
		});
}


void UDeduplicationManager::StartDeduplicationAsyncAfterEarlyCheck()
{
	for (UDeduplicateObject* Algorithm : DeduplicationAlgorithms)
	{
		if (!Algorithm) continue;

		AsyncTask(ENamedThreads::GameThread, [this, Algorithm]() mutable
			{
				for (const FDuplicateGroup& DuplicateGroup : EarlyDeduplicateGroups)
				{
					TArray<FAssetData> FilteredAssets;
					Algorithm->FilterAssetsByIncludeExclude(DuplicateGroup.DuplicateAssets, FilteredAssets);

					if (FilteredAssets.Num() != 0)
					{
						UDeduplicateObject* NewAlgorithm = DuplicateObject(Algorithm, this);
						NewAlgorithm->OnDeduplicationProgressCompleted.AddUObject(this, &UDeduplicationManager::BindUpdateDeduplicationProgressCompleted);
						NewAlgorithm->OnDeduplicationCompleted.AddUObject(this, &UDeduplicationManager::EndDeduplicateAssetsAsync);
						DeduplicationAlgorithmsInWork.Add(NewAlgorithm);

						Async(EAsyncExecution::ThreadPool, [Algorithm, DuplicateGroup, NewAlgorithm, this]()
							{
								NewAlgorithm->FindDuplicates(DuplicateGroup.DuplicateAssets);
							});
					}
				}
			});

	}
}


void UDeduplicationManager::StartCreateClasters()
{
	TArray<FDuplicateCluster> ResultClusters;
	SetProgress(0.99);

	int Counter = 0;
	for (const FDuplicateGroup& DuplicateGroup : DeduplicateGroups)
	{
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
	int Count = 0;
	while (ResultClusters.IsValidIndex(Count))
	{
		if (ResultClusters[Count].ClusterScore < ConfidenceThreshold)
		{
			ResultClusters.RemoveAt(Count);
			continue;
		}
		else
		{
			
			int Index = 0;
			while(ResultClusters[Count].DuplicateAssets.IsValidIndex(Index))
			{
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
	}

	SetProgress(1.0);

	AsyncTask(ENamedThreads::GameThread, [this, ResultClusters = MoveTemp(ResultClusters)]() mutable
		{
			Clasters = ResultClusters;
			bCompleteAnalyze = true;
			bIsAnalyze = false;
			OnDeduplicationAnalyzeCompleted.Broadcast(Clasters);
			UE_LOG(LogTemp, Log, TEXT("Deduplication completed: %d clusters"), Clasters.Num());
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

	for (const FDuplicateCluster& Cluster : Clasters)
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


TArray<FDuplicateCluster> UDeduplicationManager::GetClastersByFolder(FString Folder)
{
	TArray<FDuplicateCluster> ResultClasters;
	for (FDuplicateCluster Claster : Clasters)
	{
		if (Claster.AssetData.PackagePath.ToString().Contains(Folder))
		{
			ResultClasters.Add(Claster);
		}
	}
	return ResultClasters;
}
