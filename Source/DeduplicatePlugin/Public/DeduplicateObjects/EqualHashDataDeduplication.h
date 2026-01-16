/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

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