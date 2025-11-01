// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DeduplicateObject.h"
#include "EqualNameDeduplication.generated.h"

/**
 * Deduplication algorithm that compares asset names
 * Groups assets with identical names as potential duplicates
 */
UCLASS(BlueprintType, Blueprintable)
class DEDUPLICATEPLUGIN_API UEqualNameDeduplication : public UDeduplicateObject
{
	GENERATED_BODY()

public:
	UEqualNameDeduplication();

	virtual TArray<FDuplicateGroup> Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze) override;

protected:
	virtual float CalculateConfidenceScore_Implementation(const TArray<FAssetData>& Assets) const override;

	virtual float CalculateComplexity_Implementation(const TArray<FAssetData>& CheckAssets) override;

private:
	bool AreNamesEqual(const FString& Name1, const FString& Name2) const;
	FString NormalizeAssetName(const FString& AssetName) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Name Deduplication", meta = (AllowPrivateAccess = "true"))
	bool bIgnoreCase = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Name Deduplication", meta = (AllowPrivateAccess = "true"))
	bool bIgnoreExtensions = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Name Deduplication", meta = (AllowPrivateAccess = "true"))
	bool bIgnoreCommonPrefixes = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Name Deduplication", meta = (AllowPrivateAccess = "true"))
	TArray<FString> CommonPrefixesToIgnore;

	// Penalty applied for the difference between two names. It can be based on the number of mismatched characters 
	// or the relative difference between names, depending on bBlendPenaltyBySize.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Name Deduplication", meta = (AllowPrivateAccess = "true"))
	float PenaltyByDistance = 0.25;

	// If True — the penalty is calculated as a relative difference between names (ranging from 0 to 1),
	// i.e. Penalty = RelativeDifference * PenaltyByDistance. 
	// If False — the penalty is calculated based on the absolute number of differing characters.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Name Deduplication", meta = (AllowPrivateAccess = "true"))
	bool bBlendPenaltyBySize = false;
};
