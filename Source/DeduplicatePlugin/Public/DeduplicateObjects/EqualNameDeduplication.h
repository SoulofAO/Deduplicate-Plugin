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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Name Deduplication", meta = (AllowPrivateAccess = "true"))
	float PenaltyByDistance = 0.25;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Name Deduplication", meta = (AllowPrivateAccess = "true"))
	bool bBlendPenaltyBySize = false;
};
