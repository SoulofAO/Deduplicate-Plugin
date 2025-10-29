// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeduplicateObjects/EqualSimpleDataDeduplication.h"
#include "Engine/AssetManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DeduplicationFunctionLibrary.h"

UEqualSimpleUassetDataDeduplication::UEqualSimpleUassetDataDeduplication()
{
	MinCommonSubstringLength = 4;
	bAnalyzeBinaryContent = true;
}

float UEqualSimpleUassetDataDeduplication::CalculateBinarySimilarity(const TArray<uint8>& Data1, const TArray<uint8>& Data2) const
{
	int32 MinSize = FMath::Min(Data1.Num(), Data2.Num());
	int32 MatchingBytes = 0;

	for (int32 i = 0; i < MinSize; i++)
	{
		if (Data1[i] == Data2[i])
		{
			MatchingBytes++;
		}
	}

	float ByteSimilarity = (float)MatchingBytes / (float)MinSize;

	int32 CommonSubstrings = UDeduplicationFunctionLibrary::FindCommonSubstrings(Data1, Data2, MinCommonSubstringLength);
	float SubstringSimilarity = FMath::Min(1.0f, (float)CommonSubstrings / 10.0f);

	return (ByteSimilarity * 0.8f + SubstringSimilarity * 0.2f);
}

