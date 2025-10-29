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
	if (DeduplicationAlgorithmsInWork.Num() <= 0)
	{
		StartCreateClasters();
	}
	DeduplicationAlgorithmProgressCompleteCount += 1;
	Lock.Unlock();
}
void UDeduplicationManager::EndEarlyDeduplicateAssetsAsync(TArray<FDuplicateGroup> NewDeduplicateGroups, UDeduplicateObject* EarlyCheckAlgorithm)
{
	FScopeLock Lock(&EndEarlyDeduplicationLock);
	EarlyDeduplicateGroups.Append(NewDeduplicateGroups);
	EarlyCheckAlgorithm->OnDeduplicationCompleted.RemoveAll(this);
	EarlyCheckDeduplicationAlgorithmsInWork.Remove(EarlyCheckAlgorithm);
	DeduplicationAlgorithmProgressCompleteCount += 1;
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
	TArray<UDeduplicateObject*> AlgorithmsCopy = DeduplicationAlgorithms;
	DeduplicationAlgorithmsInWork.Empty();
	EarlyCheckDeduplicationAlgorithmsInWork.Empty();
	EarlyDeduplicateGroups.Empty();
	DeduplicateGroups.Empty();

	Async(EAsyncExecution::ThreadPool, [this, AssetsCopy = MoveTemp(AssetsCopy), AlgorithmsCopy = MoveTemp(AlgorithmsCopy)]()
		{
			SetProgress(0.0);
			TArray<FDuplicateGroup> AllGroups;

			DeduplicationAlgorithmProgressCompleteCount = 0;

			TSharedRef<TArray<FAssetData>, ESPMode::ThreadSafe> SharedAssets = MakeShared<TArray<FAssetData>, ESPMode::ThreadSafe>(AssetsCopy);

			TArray<FDuplicateGroup> EarlyRegisteringGroups;
			if (EarlyCheckDeduplicationAlgorithms.Num() > 0)
			{
				for (UDeduplicateObject* EarlyCheckAlgorithm : EarlyCheckDeduplicationAlgorithms)
				{
					if (!EarlyCheckAlgorithm)
					{
						DeduplicationAlgorithmProgressCompleteCount += 1;
						continue;
					}

					EarlyCheckAlgorithm->OnDeduplicationProgressCompleted.AddUObject(this, &UDeduplicationManager::BindUpdateDeduplicationProgressCompleted);

					EarlyCheckDeduplicationAlgorithmsInWork.Add(EarlyCheckAlgorithm);
					Async(EAsyncExecution::ThreadPool, [this, SharedAssets, EarlyCheckAlgorithm]()
						{
							EarlyCheckAlgorithm->OnDeduplicationCompleted.AddUObject(this, &UDeduplicationManager::EndEarlyDeduplicateAssetsAsync);
							EarlyCheckAlgorithm->FindDuplicates(SharedAssets.Get());
						});
				}

				DeduplicationAlgorithmProgressCompleteCount = 0;
			}
			else
			{

				for (UDeduplicateObject* Algorithm : DeduplicationAlgorithms)
				{
					if (!Algorithm)
					{
						DeduplicationAlgorithmProgressCompleteCount += 1;
						continue;
					}

					Algorithm->OnDeduplicationProgressCompleted.AddUObject(this, &UDeduplicationManager::BindUpdateDeduplicationProgressCompleted);
					DeduplicationAlgorithmsInWork.Add(Algorithm);
			
					Async(EAsyncExecution::ThreadPool, [this, SharedAssets, Algorithm]()
						{
							Algorithm->OnDeduplicationCompleted.AddUObject(this, &UDeduplicationManager::EndDeduplicateAssetsAsync);
							Algorithm->FindDuplicates(*SharedAssets);
						});
				}
			}
		});
}

void UDeduplicationManager::StartDeduplicationAsyncAfterEarlyCheck()
{
	for (UDeduplicateObject* Algorithm : DeduplicationAlgorithms)
	{
		if (!Algorithm)
		{
			DeduplicationAlgorithmProgressCompleteCount += 1;
			continue;
		}

		Algorithm->OnDeduplicationProgressCompleted.AddUObject(this, &UDeduplicationManager::BindUpdateDeduplicationProgressCompleted);

		for (const FDuplicateGroup& DuplicateGroup : EarlyDeduplicateGroups)
		{
			TArray<FAssetData> LocalAssets = DuplicateGroup.DuplicateAssets;

			Async(EAsyncExecution::ThreadPool, [LocalAssets = MoveTemp(LocalAssets), Algorithm, this]()
				{
					Algorithm->OnDeduplicationCompleted.AddUObject(this, &UDeduplicationManager::EndDeduplicateAssetsAsync);
					Algorithm->FindDuplicates(LocalAssets);
				});
		}
	}
}

void UDeduplicationManager::StartDeduplicationAsync(TArray<FAssetData> AssetsToAnalyze)
{
	for (UDeduplicateObject* Algorithm : DeduplicationAlgorithms)
	{
		if (!Algorithm)
		{
			DeduplicationAlgorithmProgressCompleteCount += 1;
			continue;
		}

		Algorithm->OnDeduplicationProgressCompleted.AddUObject(this, &UDeduplicationManager::BindUpdateDeduplicationProgressCompleted);

		Async(EAsyncExecution::ThreadPool, [LocalAssets = MoveTemp(AssetsToAnalyze), Algorithm, this]()
			{
				Algorithm->OnDeduplicationCompleted.AddUObject(this, &UDeduplicationManager::EndDeduplicateAssetsAsync);
				Algorithm->FindDuplicates(LocalAssets);
			});
	}
}

void UDeduplicationManager::StartCreateClasters()
{
	TArray<FDuplicateCluster> ResultClusters;

	for (const FDuplicateGroup& DuplicateGroup : DeduplicateGroups)
	{
		const float GroupScore = DuplicateGroup.ConfidenceScore;

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
					if (FMath::IsNearlyZero(Cluster.ClusterScore))
					{
						Cluster.ClusterScore = GroupScore;
					}
					else
					{
						Cluster.ClusterScore *= GroupScore;
					}
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
							if (FMath::IsNearlyZero(ExistingScore))
							{
								ExistingScore = GroupScore;
							}
							else
							{
								ExistingScore *= GroupScore;
							}
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

	int Count = 0;
	while (ResultClusters.IsValidIndex(Count))
	{
		if (ResultClusters[Count].ClusterScore < ConfidenceThreshold)
		{
			ResultClusters.RemoveAt(Count);
			continue;
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


void UDeduplicationManager::BindUpdateDeduplicationProgressCompleted(float Progress)
{
	SetProgress((DeduplicationAlgorithmProgressCompleteCount + Progress) / DeduplicationAlgorithms.Num() - 0.01);
}

void UDeduplicationManager::SetProgress(float Progress)
{
	AsyncTask(ENamedThreads::GameThread, [this, Progress]() mutable
		{
			ProgressValue = Progress;
			OnDeduplicationProgressCompleted.Broadcast(ProgressValue);
		});
}

TArray<FDuplicateCluster> UDeduplicationManager::FindMostPriorityDuplicateClusterByPath(FString Path)
{
	TArray<FDuplicateCluster> Results;
	FString Pattern = FString::Format(TEXT("{0}"), { Path });

	for (const FDuplicateCluster& Cluster : Clasters)
	{
		const FString PackagePath = Cluster.AssetData.PackagePath.ToString();
		const FString ObjectPath = Cluster.AssetData.ObjectPath.ToString();
		const FString PackageName = Cluster.AssetData.PackageName.ToString();

		if (PackagePath.StartsWith(Pattern) || ObjectPath.StartsWith(Pattern) || PackageName.StartsWith(Pattern))
		{
			Results.Add(Cluster);
			continue;
		}

		for (const FDeduplicationAssetStruct& DuplicateStruct : Cluster.DuplicateAssets)
		{
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
