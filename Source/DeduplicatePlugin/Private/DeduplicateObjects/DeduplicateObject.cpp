// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeduplicateObjects/DeduplicateObject.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include <cfloat>

UDeduplicateObject::UDeduplicateObject()
{
    Weight = 1.0f;
}

void UDeduplicateObject::Load(const TArray<FAssetData>& AssetsToAnalyze)
{
    TArray<FSoftObjectPath> Paths;
    Paths.Reserve(AssetsToAnalyze.Num());

    for (const FAssetData& AssetData : AssetsToAnalyze)
    {
        if (AssetData.IsValid())
        {
            Paths.Add(AssetData.ToSoftObjectPath());
        }
    }

    if (Paths.Num() == 0)
    {
        OnLoadingAssetsCompleted.Broadcast();
    }


    AsyncTask(ENamedThreads::GameThread, [this, Paths]() mutable
        {
            FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();

            Handle = StreamableManager.RequestAsyncLoad(
                Paths,
                FStreamableDelegate::CreateLambda([this]()
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Load Start End"));
                        Async(EAsyncExecution::ThreadPool, [this]()
                        {
                            OnLoadingAssetsCompleted.Broadcast();
                        });
                    }));

            if (Handle.IsValid())
            {
                Handle->BindCompleteDelegate(FStreamableDelegate::CreateLambda([this]()
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Load Start End"));
                        Async(EAsyncExecution::ThreadPool, [this]()
                            {
                                OnLoadingAssetsCompleted.Broadcast();
                            });
                    }));
            }
         });
}


void UDeduplicateObject::FindDuplicates(const TArray<FAssetData>& AssetsToAnalyze)
{
    DeduplicationAssets = AssetsToAnalyze;

    if (ShouldLoadAssets())
    {
		OnLoadingAssetsCompleted.AddUObject(this, &UDeduplicateObject::Iternal_StartFindDeduplicatesAfterLoad);
        Load(AssetsToAnalyze);
    }
    else
    {
        Iternal_StartFindDeduplicatesAfterLoad();
    }
}

void UDeduplicateObject::Iternal_StartFindDeduplicatesAfterLoad()
{
    OnLoadingAssetsCompleted.RemoveAll(this);

	TArray<FDuplicateGroup> Result = Internal_FindDuplicates(DeduplicationAssets);
	OnDeduplicationCompleted.Broadcast(Result, this);
}


TArray<FDuplicateGroup> UDeduplicateObject::Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze)
{
	return TArray<FDuplicateGroup>();
}

bool UDeduplicateObject::ShouldLoadAssets_Implementation()
{
	return false;
}

void UDeduplicateObject::BroadcastDeduplicationProgressCompleted(const float Progress)
{
}

FDuplicateGroup UDeduplicateObject::CreateDuplicateGroup(const TArray<FAssetData>& NewAssets, float Score)
{
	GetAlgorithmName();
	FString AlgorithmName = GetAlgorithmName();
	return FDuplicateGroup(NewAssets, Score, AlgorithmName);
}

float UDeduplicateObject::CalculateConfidenceScore_Implementation(const TArray<FAssetData>& CheckAssets) const
{
	return 0.5f;
}
