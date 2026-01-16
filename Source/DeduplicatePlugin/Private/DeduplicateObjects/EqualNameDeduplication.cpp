/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#include "DeduplicateObjects/EqualNameDeduplication.h"
#include "Engine/AssetManager.h"
#include "DeduplicationFunctionLibrary.h"

UEqualNameDeduplication::UEqualNameDeduplication()
{
	bIgnoreCase = true;
	bIgnoreExtensions = false;
	bIgnoreCommonPrefixes = false;

	CommonPrefixesToIgnore.Add(TEXT("SM_"));        // Static Mesh
	CommonPrefixesToIgnore.Add(TEXT("SK_"));        // Skeletal Mesh
	CommonPrefixesToIgnore.Add(TEXT("SKEL_"));      // Skeleton / Skeleton asset
	CommonPrefixesToIgnore.Add(TEXT("BP_"));        // Blueprint (Actor / Blueprint class)
	CommonPrefixesToIgnore.Add(TEXT("WBP_"));       // Widget Blueprint (UI)
	CommonPrefixesToIgnore.Add(TEXT("M_"));         // Material
	CommonPrefixesToIgnore.Add(TEXT("MAT_"));       // Material 
	CommonPrefixesToIgnore.Add(TEXT("MI_"));        // Material Instance
	CommonPrefixesToIgnore.Add(TEXT("T_"));         // Texture
	CommonPrefixesToIgnore.Add(TEXT("TX_"));        // Texture (alternative)
	CommonPrefixesToIgnore.Add(TEXT("A_"));         // Animation asset (AnimSequence / Montage)
	CommonPrefixesToIgnore.Add(TEXT("ANIM_"));      // Animation (��������������)
	CommonPrefixesToIgnore.Add(TEXT("P_"));         // Particle system (Cascade/Niagara)
	CommonPrefixesToIgnore.Add(TEXT("FX_"));        // VFX / ������
	CommonPrefixesToIgnore.Add(TEXT("NS_"));   // Niagara system / emitter (���� ������������)
	CommonPrefixesToIgnore.Add(TEXT("S_"));         // Sound (SoundWave / raw sound)
	CommonPrefixesToIgnore.Add(TEXT("SC_"));        // SoundCue
	CommonPrefixesToIgnore.Add(TEXT("UI_"));        // UI / user interface assets
}


TArray<FDuplicateGroup> UEqualNameDeduplication::Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze)
{
	TArray<FDuplicateGroup> DuplicateGroups;
	TMap<FString, TArray<FAssetData>> NameToAssetsMap;

	int32 TotalAssetsNumber = AssetsToAnalyze.Num();
	if (TotalAssetsNumber > 0)
	{
		int32 Counter = 0;
		for (const FAssetData& Asset : AssetsToAnalyze)
		{
			if (ShouldStop())
			{
				break;
			}
			
			Counter++;
			FString NormalizedName = NormalizeAssetName(Asset.AssetName.ToString());

			bool bAddedToExistingGroup = false;
			for (auto& NamePair : NameToAssetsMap)
			{
				const FString& ExistingName = NamePair.Key;

				int32 Distance = UDeduplicationFunctionLibrary::ComputeLevenshteinDistance(NormalizedName, ExistingName);
				float Penalty;

				if (bBlendPenaltyBySize)
				{
					int32 MaxLength = FMath::Max(ExistingName.Len(), NormalizedName.Len());
					Penalty = Distance / static_cast<float>(MaxLength) * PenaltyByDistance;
				}
				else
				{
					Penalty = Distance * PenaltyByDistance;
				}

				if ((1-Distance*PenaltyByDistance) > SimilarityThreshold)
				{
					NamePair.Value.Add(Asset);
					bAddedToExistingGroup = true;
					break;
				}
			}

			if (!bAddedToExistingGroup)
			{
				NameToAssetsMap.Add(NormalizedName, TArray<FAssetData>{ Asset });
			}

			SetProgress(Counter);
			OnDeduplicationProgressCompleted.Broadcast();
		}
	}
	else
	{
		SetProgress(TotalAssetsNumber);
	}

	{
		int32 NumNames = NameToAssetsMap.Num();
		int32 Counter = 0;

		for (auto& NamePair : NameToAssetsMap)
		{
			if (ShouldStop())
			{
				break;
			}
			
			Counter++;
			const TArray<FAssetData>& AssetsWithSameName = NamePair.Value;

			if (AssetsWithSameName.Num() > 1)
			{
				float ConfidenceScore = CalculateConfidenceScore(AssetsWithSameName);
				FDuplicateGroup DuplicateGroup = CreateDuplicateGroup(AssetsWithSameName, ConfidenceScore);
				DuplicateGroups.Add(DuplicateGroup);
			}

			SetProgress(Counter + TotalAssetsNumber);
		}
	}

	return DuplicateGroups;
}


float UEqualNameDeduplication::CalculateConfidenceScore_Implementation(const TArray<FAssetData>& Assets) const
{
	const int32 NumAssets = Assets.Num();
	if (NumAssets <= 1)
	{
		return 1.0f;
	}

	double TotalSimilarity = 0.0;
	int64 PairCount = 0;

	for (int32 IndexA = 0; IndexA < NumAssets - 1; ++IndexA)
	{
		if (ShouldStop())
		{
			break;
		}
		
		const FString NameA = NormalizeAssetName(Assets[IndexA].AssetName.ToString());

		for (int32 IndexB = IndexA + 1; IndexB < NumAssets; ++IndexB)
		{
			const FString NameB = NormalizeAssetName(Assets[IndexB].AssetName.ToString());

			const int32 Distance = UDeduplicationFunctionLibrary::ComputeLevenshteinDistance(NameA, NameB);
			const float Penalty = static_cast<float>(Distance) * PenaltyByDistance;

			const float Similarity = FMath::Clamp(1.0f - Penalty, 0.0f, 1.0f);
			TotalSimilarity += static_cast<double>(Similarity);
			++PairCount;
		}
	}

	if (PairCount == 0)
	{
		return 1.0f;
	}

	const float AverageSimilarity = static_cast<float>(TotalSimilarity / static_cast<double>(PairCount));
	return FMath::Clamp(AverageSimilarity, 0.0f, 1.0f);
}

float UEqualNameDeduplication::CalculateComplexity_Implementation(const TArray<FAssetData>& CheckAssets)
{
	AlgorithmComplexity = CheckAssets.Num() * 2;
	return CheckAssets.Num();
}


bool UEqualNameDeduplication::AreNamesEqual(const FString& Name1, const FString& Name2) const
{
	FString NormalizedName1 = NormalizeAssetName(Name1);
	FString NormalizedName2 = NormalizeAssetName(Name2);

	int Distance = UDeduplicationFunctionLibrary::ComputeLevenshteinDistance(NormalizedName1, NormalizedName2);

	return (1 - Distance * PenaltyByDistance) > SimilarityThreshold;
}


FString UEqualNameDeduplication::NormalizeAssetName(const FString& AssetName) const
{
	FString NormalizedName = AssetName;
	
	if (bIgnoreExtensions)
	{
		NormalizedName = FPaths::GetBaseFilename(NormalizedName);
	}
	
	if (bIgnoreCommonPrefixes)
	{
		for (const FString& Prefix : CommonPrefixesToIgnore)
		{
			if (NormalizedName.StartsWith(Prefix, bIgnoreCase ? ESearchCase::IgnoreCase : ESearchCase::CaseSensitive))
			{
				NormalizedName = NormalizedName.Mid(Prefix.Len());
				break; 
			}
		}
	}
	
	if (bIgnoreCase)
	{
		NormalizedName = NormalizedName.ToLower();
	}
	
	return NormalizedName;
}

