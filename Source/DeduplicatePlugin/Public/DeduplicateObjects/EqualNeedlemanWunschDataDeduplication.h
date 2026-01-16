/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "DeduplicateObject.h"
#include "DeduplicateObjects/EqualDataBaseDeduplication.h"
#include "EqualNeedlemanWunschDataDeduplication.generated.h"

/**
	Deduplication that uses Needleman Wunsch everistics to compare two strings.
	It takes a very long time to execute.
 */

UCLASS(BlueprintType, Blueprintable)
class DEDUPLICATEPLUGIN_API UEqualNeedlemanWunschDataDeduplication : public UEqualBaseDataDeduplication
{
	GENERATED_BODY()

public:
	UEqualNeedlemanWunschDataDeduplication();

	float CalculateBinarySimilarity(const TArray<uint8>& Data1, const TArray<uint8>& Data2) const;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Needleman Wunsch Data Deduplication")
	int32 MatchScore = 2;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Needleman Wunsch Data Deduplication")
	int32 MismatchScore = -1;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Needleman Wunsch Data Deduplication")
	int32 GapOpen = -5;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Needleman Wunsch Data Deduplication")
	int32 GapExtend = -1;
};
