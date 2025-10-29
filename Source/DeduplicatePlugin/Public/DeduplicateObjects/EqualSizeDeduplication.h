// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeduplicateObjects/DeduplicateObject.h"
#include "EqualSizeDeduplication.generated.h"

/**
 * 
 */
UCLASS()
class DEDUPLICATEPLUGIN_API UEqualSizeDeduplication : public UDeduplicateObject
{
	GENERATED_BODY()

public:
	virtual TArray<FDuplicateGroup> Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze) override;

	virtual bool ShouldLoadAssets_Implementation() override;

protected:
	virtual float CalculateConfidenceScore_Implementation(const TArray<FAssetData>& Assets) const override;

	int64 GetAssetFileSize(const FAssetData& Asset) const;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size Deduplication")
	bool CheckSameType = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size Deduplication")
	float PenaltyByDifferenceDistance = 0.01;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size Deduplication")
	bool bBlendPenaltyBySize = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size Deduplication")
	bool UseLoadingSize = true;
};
