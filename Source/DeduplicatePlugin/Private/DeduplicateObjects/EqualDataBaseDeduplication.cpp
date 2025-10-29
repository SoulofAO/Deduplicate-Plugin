// Fill out your copyright notice in the Description page of Project Settings.


#include "DeduplicateObjects/EqualDataBaseDeduplication.h"


UEqualBaseDataDeduplication::UEqualBaseDataDeduplication()
{
}

TArray<FDuplicateGroup> UEqualBaseDataDeduplication::Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze)
{
	TArray<FDuplicateGroup> DuplicateGroups;
	TArray<FAssetData> SupportedAssets;

	for (const FAssetData& Asset : AssetsToAnalyze)
	{
		SupportedAssets.Add(Asset);
	}


	for (int32 i = 0; i < SupportedAssets.Num(); i++)
	{
		TArray<FAssetData> CurrentGroup;
		CurrentGroup.Add(SupportedAssets[i]);

		for (int32 j = i + 1; j < SupportedAssets.Num(); j++)
		{
			TArray<uint8> Data1, Data2;

			if (LoadAssetData(SupportedAssets[i], Data1) && LoadAssetData(SupportedAssets[j], Data2))
			{
				float Similarity = CalculateSimilarity(Data1, Data2);

				if (Similarity >= SimilarityThreshold)
				{
					CurrentGroup.Add(SupportedAssets[j]);
					SupportedAssets.RemoveAtSwap(j);
					j--;
				}
			}

			OnDeduplicationProgressCompleted.Broadcast(i / SupportedAssets.Num() + j / SupportedAssets.Num() * 1 / SupportedAssets.Num());
		}

		OnDeduplicationProgressCompleted.Broadcast(i / SupportedAssets.Num());

		if (CurrentGroup.Num() > 1)
		{
			float ConfidenceScore = CalculateConfidenceScore(CurrentGroup);
			FDuplicateGroup DuplicateGroup = CreateDuplicateGroup(CurrentGroup, ConfidenceScore);
			DuplicateGroups.Add(DuplicateGroup);
		}

	}

	return DuplicateGroups;
}

float UEqualBaseDataDeduplication::CalculateConfidenceScore_Implementation(const TArray<FAssetData>& Assets) const
{
	if (Assets.Num() < 2)
	{
		return 0.0f;
	}

	float TotalSimilarity = 0.0f;
	int32 ComparisonCount = 0;

	for (int32 i = 0; i < Assets.Num(); i++)
	{
		for (int32 j = i + 1; j < Assets.Num(); j++)
		{
			TArray<uint8> Data1, Data2;
			if (LoadAssetData(Assets[i], Data1) && LoadAssetData(Assets[j], Data2))
			{
				TotalSimilarity += CalculateSimilarity(Data1, Data2);
				ComparisonCount++;
			}
		}
	}

	if (ComparisonCount > 0)
	{
		float AverageSimilarity = TotalSimilarity / ComparisonCount;
		float GroupSizeBonus = FMath::Min(0.2f, (Assets.Num() - 2) * 0.05f);
		return FMath::Clamp(AverageSimilarity + GroupSizeBonus, 0.0f, 1.0f);
	}

	return 0.0f;
}

float UEqualBaseDataDeduplication::CalculateSimilarity(const TArray<uint8>& Data1, const TArray<uint8>& Data2) const
{
	if (Data1.Num() == 0 || Data2.Num() == 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(CalculateBinarySimilarity(Data1, Data2), 0.0f, 1.0f);
}

bool UEqualBaseDataDeduplication::LoadAssetData(const FAssetData& Asset, TArray<uint8>& OutData) const
{
	FString AssetPath = Asset.ObjectPath.ToString();
	int Index;
	AssetPath.FindLastChar(*TEXT("."), Index);
	AssetPath = AssetPath.Left(Index);
	FString PackagePath = FPackageName::LongPackageNameToFilename(AssetPath);

	FString UAssetPath = PackagePath + TEXT(".uasset");

	if (FPaths::FileExists(UAssetPath))
	{
		return FFileHelper::LoadFileToArray(OutData, *UAssetPath);
	}

	return false;
}

float UEqualBaseDataDeduplication::CalculateBinarySimilarity(const TArray<uint8>& Data1, const TArray<uint8>& Data2) const
{
	return 0.0f;
}
