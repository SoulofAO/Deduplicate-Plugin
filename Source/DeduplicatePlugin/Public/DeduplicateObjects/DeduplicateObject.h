/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/AssetManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DeduplicateObject.generated.h"

class UDeduplicationManager;


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

DECLARE_MULTICAST_DELEGATE(FOnDeduplicationProgressCompleted);
DECLARE_MULTICAST_DELEGATE(FOnLoadingAssetsCompleted);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDeduplicationCompleted, TArray<FDuplicateGroup>, UDeduplicateObject*);

//The core deduplication processing element of the plugin. It detects groups of assets that are potentially duplicates of each other. 
//It's asynchronous via multithreading mechanisms. Hypothetically supported via Blueprints.
//All Deduplicators are executed within the "unconditional" rule environment. It is assumed that all deduplicated assets belong to the same class. 
//This rule is required for optimization purposes, as otherwise, the execution time of the Deduplicator Plugin on large, full-fledged projects, even for simple Deduplicator algorithms, is extremely long.

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

	//The main function to be rewritten for Deduplication implementation.
	//Finds groups of assets that can be deduplicated.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Deduplication")
	TArray<FDuplicateGroup> Internal_FindDuplicates(const TArray<FAssetData>& AssetsToAnalyze);
	virtual TArray<FDuplicateGroup> Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze);

	//Returns the name of the deduplication algorithm.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Deduplication")
	FString GetAlgorithmName();

	virtual FString GetAlgorithmName_Implementation() const { return GetClass()->GetName(); }

	//Group cutter. The same as at the Manager level, but works for specific DeduplicateObject generate duplicators groups.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deduplication")
	float SimilarityThreshold = 0.7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deduplication")
	float Weight = 1.0f;

	//Execute the deduplicator only for the specified classes. If it is empty, execute for all classes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deduplication")
	TArray<TSubclassOf<UObject>> IncludeClasses;

	//Exclude execution for the specified classes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deduplication")
	TArray<TSubclassOf<UObject>> ExcludeClasses;

	class UDeduplicationManager* OwnerManager;

	//If return true, it will preload the Assets that you are processing.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Deduplication")
	bool ShouldLoadAssets();
	virtual bool ShouldLoadAssets_Implementation();

	FOnDeduplicationProgressCompleted OnDeduplicationProgressCompleted;

	FOnDeduplicationCompleted OnDeduplicationCompleted;

	FOnLoadingAssetsCompleted OnLoadingAssetsCompleted;

	float AlgorithmComplexity;
	float Progress;

	//Support functions for implementing the operation progression slider.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Deduplication")
	float CalculateComplexity(const TArray<FAssetData>& Assets);
	virtual float CalculateComplexity_Implementation(const TArray<FAssetData>& CheckAssets);

	bool IsAssetClassAllowed(UClass* AssetClass) const;
	void FilterAssetsByIncludeExclude(const TArray<FAssetData>& InAssets, TArray<FAssetData>& OutFiltered) const;

protected:
	UFUNCTION(BlueprintCallable, Category = "Deduplication")
	FDuplicateGroup CreateDuplicateGroup(const TArray<FAssetData>& NewAssets, float Score);

	//In most cases, it calculates the group as the average of the proximity of each asset to each other in the group.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Deduplication")
	float CalculateConfidenceScore(const TArray<FAssetData>& CheckAssets) const;
	virtual float CalculateConfidenceScore_Implementation(const TArray<FAssetData>& CheckAssets) const;

	//Support functions for implementing the operation progression slider.
	UFUNCTION(BlueprintCallable, Category = "Deduplication")
	void SetProgress(float NewProgress);

	bool ShouldStop() const;
};
