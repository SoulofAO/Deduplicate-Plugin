// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeduplicateObjects/DeduplicateObject.h"
#include "EqualDataBaseDeduplication.generated.h"

/**
 * 
 */

UCLASS(BlueprintType, Blueprintable, Abstract)
class DEDUPLICATEPLUGIN_API UEqualBaseDataDeduplication : public UDeduplicateObject
{
	GENERATED_BODY()

public:
	UEqualBaseDataDeduplication();

	virtual TArray<FDuplicateGroup> Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze) override final;

	virtual float CalculateComplexity_Implementation(const TArray<FAssetData>& CheckAssets) override;

protected:
	virtual float CalculateConfidenceScore_Implementation(const TArray<FAssetData>& Assets) const override;
	virtual float CalculateSimilarity(const TArray<uint8>& Data1, const TArray<uint8>& Data2) const;
	bool LoadAssetData(const FAssetData& Asset, TArray<uint8>& OutData) const;
	virtual float CalculateBinarySimilarity(const TArray<uint8>& Data1, const TArray<uint8>& Data2) const;
	virtual bool ShouldLoadAssets_Implementation();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deduplication")
	bool ShouldUseSerialization = true;
};
