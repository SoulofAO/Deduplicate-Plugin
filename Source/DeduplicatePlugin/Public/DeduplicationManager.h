// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DeduplicateObjects/DeduplicateObject.h"
#include "DeduplicationManager.generated.h"


USTRUCT(BlueprintType)
struct DEDUPLICATEPLUGIN_API FDeduplicationAssetStruct
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Deduplication")
	FAssetData DuplicateAsset;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Deduplication")
	float DeduplicationAssetScore;

	bool operator==(const FAssetData& OtherDuplicateAsset) const
	{
		return DuplicateAsset == OtherDuplicateAsset;
	}

	bool operator==(const FDeduplicationAssetStruct& Other) const
	{
		return DuplicateAsset == Other.DuplicateAsset;
	}

};

USTRUCT(BlueprintType)
struct DEDUPLICATEPLUGIN_API FDuplicateCluster
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Deduplication")
	FAssetData AssetData;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Deduplication")
	TArray<FDeduplicationAssetStruct> DuplicateAssets;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Deduplication")
	float ClusterScore = 0.0f;

	FDuplicateCluster()
	{
		DuplicateAssets.Empty();
		ClusterScore = 0.0f;
	}

	FDuplicateCluster(FAssetData NewAssetData, TArray<FAssetData> NewDuplicateAssets, float MewClusterScore) : AssetData(NewAssetData), DuplicateAssets(NewDuplicateAssets), ClusterScore(MewClusterScore)
	{
	}

	bool operator==(const FDuplicateCluster& Other) const
	{
		return AssetData == Other.AssetData
			&& DuplicateAssets == Other.DuplicateAssets
			&& FMath::IsNearlyEqual(ClusterScore, Other.ClusterScore);
	}

	bool operator==(const FAssetData& OtherAssetData) const
	{
		return AssetData == OtherAssetData;
	}

	bool AvailibleByConfidenceThreshold(float ConfidenceThreshold, float GroupConfidenceThreshold)
	{
		if (ClusterScore > ConfidenceThreshold)
		{
			for (FDeduplicationAssetStruct DuplicateAsset : DuplicateAssets)
			{
				if (DuplicateAsset.DeduplicationAssetScore > GroupConfidenceThreshold)
				{
					return true;
				}
			}
		}
		return false;
	}
};


UENUM(Blueprintable)
enum class ECombinationScoreMethod : uint8
{
	Add UMETA(DisplayName = "Add"),
	Multiply UMETA(DisplayName = "Multiply")
};

/**
 * Main manager class for asset deduplication
 * Coordinates multiple deduplication algorithms and manages results
 */

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDeduplicationAnalyzeCompleted, const TArray<FDuplicateCluster>&);
DECLARE_MULTICAST_DELEGATE(FOnDeduplicationProgressCompleted);

UCLASS(BlueprintType, Blueprintable)
class DEDUPLICATEPLUGIN_API UDeduplicationManager : public UObject
{
	GENERATED_BODY()

public:
	UDeduplicationManager();

	bool FindDuplicateClusterByAssetData(const FAssetData& AssetData, TArray<FDuplicateCluster>& DuplicateClasters, FDuplicateCluster& ResultClaster);

	void EndDeduplicateAssetsAsync(TArray<FDuplicateGroup> NewDeduplicateGroups, UDeduplicateObject* EarlyCheckAlgorithm);

	FCriticalSection EndDeduplicationLock;

	void EndEarlyDeduplicateAssetsAsync(TArray<FDuplicateGroup> DeduplicateGroups, UDeduplicateObject* EarlyCheckAlgorithm);

	FCriticalSection EndEarlyDeduplicationLock;

	FCriticalSection UpdateDeduplicationProgressCompletedMutex;

	TArray<FDuplicateGroup> DeduplicateGroups;
	TArray<FDuplicateGroup> EarlyDeduplicateGroups;

	void StartAnalyzeAssetsAsync(const TArray<FAssetData>& AssetsToAnalyze);

	void StartDeduplicationAsyncAfterEarlyCheck();

	void StartCreateClasters();

	void BindUpdateDeduplicationProgressCompleted();

	void SetProgress(float Progress);
	
	TArray<FDuplicateCluster> FindMostPriorityDuplicateClusterByPath(FString Path);

	UPROPERTY()
	TArray<UDeduplicateObject*> EarlyCheckDeduplicationAlgorithmsInWork;

	UPROPERTY()
	TArray<UDeduplicateObject*> DeduplicationAlgorithmsInWork;

	// Many DeduplicateObjects require significant processing time. To speed up the process, EarlyCheckDeduplicationAlgorithms are used.
	// Their purpose is to filter out all Assets that are definitely not duplicates of each other and then further check their Group DeduplicateObject.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Settings")
	TArray<UDeduplicateObject*> EarlyCheckDeduplicationAlgorithms;

	//The basic deduplication algorithm. When there are multiple objects to be deduplicated, the priorities of the objects to be deduplicated are summed and then filtered.
	//When merging, priority is given to the highest-priority classifiers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Settings")
	TArray<UDeduplicateObject*> DeduplicationAlgorithms;

	//Specifies the method by which group proximity scores will be summed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	ECombinationScoreMethod CombinationScoreMethod;
	
	float CompleteProgress = 0;
	float SummaryComplexity = 0;

	//Trims all classifiers below a certain score level. This is most often used to isolate random matches.
	UPROPERTY()
	float ConfidenceThreshold = 1.2f;

	UPROPERTY()
	float GroupConfidenceThreshold = 0.7f;

	float ProgressValue{ 0.0f };

	TArray<FDuplicateCluster> Clasters;

	TArray<FDuplicateCluster> GetClastersByFolder(FString Folder);

	UPROPERTY()
	bool bIsAnalyze = false;

	UPROPERTY()
	bool bCompleteAnalyze = false;

	FOnDeduplicationAnalyzeCompleted OnDeduplicationAnalyzeCompleted;
};
