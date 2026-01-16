/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "DeduplicateObjects/DeduplicateObject.h"
#include "Engine/Texture2D.h"
#include "Async/Async.h"
#include "UObject/NoExportTypes.h"
#include "TextureSSIMDeduplication.generated.h"

/**
 * 
 */

UCLASS(BlueprintType, Blueprintable)
class DEDUPLICATEPLUGIN_API UTextureSSIMDeduplication : public UDeduplicateObject
{
    GENERATED_BODY()

public:
    UTextureSSIMDeduplication();

    virtual TArray<FDuplicateGroup> Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze) override;
    virtual float CalculateConfidenceScore_Implementation(const TArray<FAssetData>& CheckAssets) const override;
    virtual float CalculateComplexity_Implementation(const TArray<FAssetData>& CheckAssets) override;
    virtual bool ShouldLoadAssets_Implementation() override;
    virtual FString GetAlgorithmName_Implementation() const override { return GetClass()->GetName(); }

protected:
    struct FLoadedTexture
    {
        FAssetData AssetData;
        TArray<float> Gray; // luminance [0..1]
        int32 Width = 0;
        int32 Height = 0;
        bool bValid = false;
    };

    bool LoadTextureGrayFromAsset(const FAssetData& Asset, FLoadedTexture& OutTexture);
    bool ExtractGrayFromTexture(UTexture2D* Texture, FLoadedTexture& OutTexture);

    float ComputeSSIM(const TArray<float>& A, const TArray<float>& B, int32 Width, int32 Height) const;
    float ComputeMSSSIM(const TArray<float>& A, const TArray<float>& B, int32 Width, int32 Height) const;

    void BuildGaussianKernel(int32 Radius, TArray<float>& OutKernel) const;
    void SeparableGaussianBlur(const TArray<float>& In, TArray<float>& Out, int32 Width, int32 Height, const TArray<float>& Kernel) const;
    void Downsample2x(const TArray<float>& In, TArray<float>& Out, int32 InWidth, int32 InHeight, int32& OutWidth, int32& OutHeight) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deduplication")
    int32 GaussianRadius = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deduplication")
    int32 MSSSIMLevels = 5;
};
