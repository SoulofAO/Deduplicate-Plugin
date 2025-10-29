// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DeduplicateObjects/EqualDataBaseDeduplication.h"
#include "EqualSimpleDataDeduplication.generated.h"

/**
 * Deduplication algorithm that analyzes binary UAsset content
 * Compares asset data to find similar or identical content
 */


UCLASS(BlueprintType, Blueprintable)
class DEDUPLICATEPLUGIN_API UEqualSimpleUassetDataDeduplication : public UEqualBaseDataDeduplication
{
	GENERATED_BODY()

public:
	UEqualSimpleUassetDataDeduplication();

	virtual float CalculateBinarySimilarity(const TArray<uint8>& Data1, const TArray<uint8>& Data2) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content Deduplication", meta = (AllowPrivateAccess = "true"))
	int32 MinCommonSubstringLength = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content Deduplication", meta = (AllowPrivateAccess = "true"))
	bool bAnalyzeBinaryContent = true;
};
