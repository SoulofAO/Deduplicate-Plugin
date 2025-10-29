// Fill out your copyright notice in the Description page of Project Settings.


#include "DeduplicateObjects/EqualSizeDeduplication.h"

#include "DeduplicateObjects/EqualSizeDeduplication.h"
#include "Engine/AssetManager.h"
#include "DeduplicationFunctionLibrary.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Math/UnrealMathUtility.h"


TArray<FDuplicateGroup> UEqualSizeDeduplication::Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze)
{
	TArray<FDuplicateGroup> DuplicateGroups;
	TMap<int64, TArray<FAssetData>> SizeToAssetsMap;

	int32 TotalAssets = AssetsToAnalyze.Num();
	if (TotalAssets > 0)
	{
		int32 Counter = 0;
		for (const FAssetData& Asset : AssetsToAnalyze)
		{
			Counter++;
			int64 AssetSize = GetAssetFileSize(Asset);

			bool bAddedToExistingGroup = false;
			for (auto& SizePair : SizeToAssetsMap)
			{
				const int64 ExistingSize = SizePair.Key;
				int64 Difference = FMath::Abs(AssetSize - ExistingSize);
				int64 MaxSize = FMath::Max(AssetSize, ExistingSize);

				float Penalty = 0;
				if (bBlendPenaltyBySize)
				{
					Penalty = static_cast<float>(Difference) / static_cast<float>(MaxSize) * PenaltyByDifferenceDistance;
				}
				else
				{
					Penalty = static_cast<float>(Difference) * PenaltyByDifferenceDistance;
				}

				if ((1.0f - Penalty) > SimilarityThreshold)
				{
					SizePair.Value.Add(Asset);
					bAddedToExistingGroup = true;
					break;
				}
			}

			if (!bAddedToExistingGroup)
			{
				SizeToAssetsMap.Add(AssetSize, TArray<FAssetData>{ Asset });
			}

			float Progress = (float)Counter / (float)TotalAssets * 0.5f;
			OnDeduplicationProgressCompleted.Broadcast(Progress);
		}
	}
	else
	{
		OnDeduplicationProgressCompleted.Broadcast(1.0f);
	}

	{
		int32 NumSizes = SizeToAssetsMap.Num();
		int32 Counter = 0;

		for (auto& SizePair : SizeToAssetsMap)
		{
			Counter++;
			const TArray<FAssetData>& AssetsWithSameSize = SizePair.Value;

			if (AssetsWithSameSize.Num() > 1)
			{
				float ConfidenceScore = CalculateConfidenceScore(AssetsWithSameSize);
				FDuplicateGroup DuplicateGroup = CreateDuplicateGroup(AssetsWithSameSize, ConfidenceScore);
				DuplicateGroups.Add(DuplicateGroup);
			}

			float Progress = NumSizes > 0 ? (float)Counter / (float)NumSizes * 0.5f + 0.5f : 1.0f;
			OnDeduplicationProgressCompleted.Broadcast(Progress);
		}
	}

	return DuplicateGroups;
}


float UEqualSizeDeduplication::CalculateConfidenceScore_Implementation(const TArray<FAssetData>& Assets) const
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
		const int64 SizeA = GetAssetFileSize(Assets[IndexA]);

		for (int32 IndexB = IndexA + 1; IndexB < NumAssets; ++IndexB)
		{
			const int64 SizeB = GetAssetFileSize(Assets[IndexB]);

			const int64 Difference = FMath::Abs(SizeA - SizeB);
			const int64 MaxSize = FMath::Max(SizeA, SizeB);

			float Penalty = 0.0f;
			if (bBlendPenaltyBySize)
			{
				Penalty = (MaxSize == 0)
					? 0.0f
					: static_cast<float>(Difference) / static_cast<float>(MaxSize) * PenaltyByDifferenceDistance;
			}
			else
			{
				Penalty = static_cast<float>(Difference) * PenaltyByDifferenceDistance;
			}

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


int64 UEqualSizeDeduplication::GetAssetFileSize(const FAssetData& Asset) const
{
	if (UseLoadingSize)
	{
		return Asset.GetAsset()->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal);
	}
	else
	{
		FString PackageName = Asset.PackageName.ToString();
		FString Filename;

		if (FPackageName::DoesPackageExist(PackageName, &Filename))
		{
			int64 Size = IFileManager::Get().FileSize(*Filename);
			if (Size >= 0)
			{
				return Size;
			}
		}

		FString CandidateFilename = FPackageName::LongPackageNameToFilename(PackageName, TEXT(".uasset"));
		int64 CandidateSize = IFileManager::Get().FileSize(*CandidateFilename);
		if (CandidateSize >= 0)
		{
			return CandidateSize;
		}

		return -1;
	}
}

bool UEqualSizeDeduplication::ShouldLoadAssets_Implementation()
{
	return UseLoadingSize;
}
