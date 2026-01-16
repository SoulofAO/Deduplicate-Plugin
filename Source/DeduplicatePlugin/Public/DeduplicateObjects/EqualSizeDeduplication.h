/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "DeduplicateObjects/DeduplicateObject.h"
#include "EqualSizeDeduplication.generated.h"

/**
 * Deduplication algorithm that compares asset sizes
 * Groups assets with identical or near sizes as potential duplicates
 */
UCLASS()
class DEDUPLICATEPLUGIN_API UEqualSizeDeduplication : public UDeduplicateObject
{
	GENERATED_BODY()

public:
	virtual TArray<FDuplicateGroup> Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze) override;

	virtual bool ShouldLoadAssets_Implementation() override;

	virtual float CalculateComplexity_Implementation(const TArray<FAssetData>& CheckAssets) override;
protected:
	virtual float CalculateConfidenceScore_Implementation(const TArray<FAssetData>& Assets) const override;

	int64 GetAssetFileSize(const FAssetData& Asset) const;

	// Penalty applied for the difference between two sizes. It can be based on the number of mismatched size bytes
	// or the relative difference between names, depending on bBlendPenaltyBySize.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size Deduplication")
	float PenaltyByDifferenceDistance = 0.0001;

	//	// If True � the penalty is calculated as a relative difference between suze (ranging from 0 to 1),
	// i.e. Penalty = RelativeDifference * PenaltyByDistance. 
	// If False � the penalty is calculated based on the absolute number of differing size.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size Deduplication")
	bool bBlendPenaltyBySize = false;

	/*
	As far as I can tell, assets like Blueprints and the like contain information not only about their data but also about Blueprint development. 
	This makes the size of their UASSET files and the size I get from loading them, even for a copy, quite different. 
	Therefore, loading them into memory and analyzing them directly is much more accurate.
	If you plan to read their data directly from files, disable it and set the penalty for this difference much lower.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size Deduplication")
	bool UseLoadingSize = false;
};
