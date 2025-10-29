// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/AssetManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DeduplicateObject.generated.h"


USTRUCT(BlueprintType)
struct DEDUPLICATEPLUGIN_API FDuplicateGroup
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Deduplication")
	TArray<FAssetData> DuplicateAssets;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Deduplication")
	float ConfidenceScore = 0.0f;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Deduplication")
	FName AlghoritmName;

	FDuplicateGroup()
	{
		DuplicateAssets.Empty();
		ConfidenceScore = 0.0f;
	}

	FDuplicateGroup(const TArray<FAssetData>& InAssets, float InScore, const FString& InAlgorithmName)
		: DuplicateAssets(InAssets)
		, ConfidenceScore(InScore)
	{
	}

	bool operator==(const FDuplicateGroup& Other) const
	{
		if (DuplicateAssets.Num() != Other.DuplicateAssets.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < DuplicateAssets.Num(); ++Index)
		{
			if (DuplicateAssets[Index] != Other.DuplicateAssets[Index])
			{
				return false;
			}
		}

		return FMath::IsNearlyEqual(ConfidenceScore, Other.ConfidenceScore) && AlghoritmName == Other.AlghoritmName;
	}


};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDeduplicationProgressCompleted, const float);
DECLARE_MULTICAST_DELEGATE(FOnLoadingAssetsCompleted);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDeduplicationCompleted, TArray<FDuplicateGroup>, UDeduplicateObject*);

UCLASS(BlueprintType, Abstract, DefaultToInstanced, EditInlineNew)
class DEDUPLICATEPLUGIN_API UDeduplicateObject : public UObject
{
	GENERATED_BODY()

public:
	UDeduplicateObject();

	TArray<FAssetData> DeduplicationAssets;

	void Load(const TArray<FAssetData>& AssetsToLoad);

	TSharedPtr<FStreamableHandle> Handle;

	virtual void FindDuplicates(const TArray<FAssetData>& AssetsToAnalyzee);

	virtual void Iternal_StartFindDeduplicatesAfterLoad();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Deduplication")
	TArray<FDuplicateGroup> Internal_FindDuplicates(const TArray<FAssetData>& AssetsToAnalyze);
	virtual TArray<FDuplicateGroup> Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Deduplication")
	FString GetAlgorithmName();

	virtual FString GetAlgorithmName_Implementation() const { return GetClass()->GetName(); }

	UFUNCTION(BlueprintCallable, Category = "Deduplication")
	float GetWeight() const { return Weight; }

	UFUNCTION(BlueprintCallable, Category = "Deduplication")
	void SetWeight(float InWeight) { Weight = FMath::Clamp(InWeight, 0.0f, 1.0f); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deduplication")
	float SimilarityThreshold = 0.7;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Deduplication")
	bool ShouldLoadAssets();
	virtual bool ShouldLoadAssets_Implementation();

	FOnDeduplicationProgressCompleted OnDeduplicationProgressCompleted;

	FOnDeduplicationCompleted OnDeduplicationCompleted;

	FOnLoadingAssetsCompleted OnLoadingAssetsCompleted;

	UFUNCTION(BlueprintCallable, Category = "Deduplication")
	void BroadcastDeduplicationProgressCompleted(const float Progress);

protected:
	UFUNCTION(BlueprintCallable, Category = "Deduplication")
	FDuplicateGroup CreateDuplicateGroup(const TArray<FAssetData>& NewAssets, float Score);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Deduplication")
	float CalculateConfidenceScore(const TArray<FAssetData>& CheckAssets) const;
	virtual float CalculateConfidenceScore_Implementation(const TArray<FAssetData>& CheckAssets) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (AllowPrivateAccess = "true"))
	float Weight = 1.0f;


};
