// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DeduplicateObjects/EqualDataBaseDeduplication.h"
#include "EqualSimpleDataDeduplication.generated.h"

/**
	Deduplicator, that simply compares bitwise with a short proximity search. A very stupid algorithm.
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
};
