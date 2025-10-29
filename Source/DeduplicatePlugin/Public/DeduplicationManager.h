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
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDeduplicationProgressCompleted, const float);

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

	TArray<FDuplicateGroup> DeduplicateGroups;

	TArray<FDuplicateGroup> EarlyDeduplicateGroups;

	void StartAnalyzeAssetsAsync(const TArray<FAssetData>& AssetsToAnalyze);

	void StartDeduplicationAsyncAfterEarlyCheck();

	void StartDeduplicationAsync(TArray<FAssetData> AssetsToAnalyze);

	void StartCreateClasters();

	void BindUpdateDeduplicationProgressCompleted(float Progress);

	void SetProgress(float Progress);
	
	TArray<FDuplicateCluster> FindMostPriorityDuplicateClusterByPath(FString Path);

	TArray<UDeduplicateObject*> EarlyCheckDeduplicationAlgorithmsInWork;

	TArray<UDeduplicateObject*> DeduplicationAlgorithmsInWork;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Settings")
	TArray<UDeduplicateObject*> EarlyCheckDeduplicationAlgorithms;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Settings")
	TArray<UDeduplicateObject*> DeduplicationAlgorithms;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	ECombinationScoreMethod CombinationScoreMethod;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float ConfidenceThreshold = 0.6f;

	int DeduplicationAlgorithmProgressCompleteCount = 0;
	float ProgressValue = 0.0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Settings")
	TArray<FDuplicateCluster> Clasters;

	TArray<FDuplicateCluster> GetClastersByFolder(FString Folder);

	UPROPERTY()
	bool bIsAnalyze = false;

	UPROPERTY()
	bool bCompleteAnalyze = false;

	FOnDeduplicationAnalyzeCompleted OnDeduplicationAnalyzeCompleted;

	FOnDeduplicationProgressCompleted OnDeduplicationProgressCompleted;
};
