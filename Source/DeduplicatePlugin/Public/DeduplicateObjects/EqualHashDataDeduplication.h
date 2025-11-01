// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeduplicateObjects/EqualDataBaseDeduplication.h"
#include "EqualHashDataDeduplication.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class DEDUPLICATEPLUGIN_API UEqualHashDataDeduplication : public UEqualBaseDataDeduplication
{
	GENERATED_BODY()

public:
	virtual float CalculateBinarySimilarity(const TArray<uint8>& Data1, const TArray<uint8>& Data2) const override;
};