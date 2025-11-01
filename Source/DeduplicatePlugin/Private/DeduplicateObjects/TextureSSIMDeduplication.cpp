// Fill out your copyright notice in the Description page of Project Settings.


#include "DeduplicateObjects/TextureSSIMDeduplication.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Texture.h"
#include "Rendering/Texture2DResource.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformProcess.h"
#include "Engine/Texture.h"
UTextureSSIMDeduplication::UTextureSSIMDeduplication()
{
    SimilarityThreshold = 0.7f;
    GaussianRadius = 5;
    MSSSIMLevels = 5;
    Weight = 1.0f;

    IncludeClasses = {UTexture2D::StaticClass()};
}

bool UTextureSSIMDeduplication::ShouldLoadAssets_Implementation()
{
    return true;
}

float UTextureSSIMDeduplication::CalculateComplexity_Implementation(const TArray<FAssetData>& CheckAssets)
{
    AlgorithmComplexity = CheckAssets.Num() * 3;
    return CheckAssets.Num();
}

bool UTextureSSIMDeduplication::LoadTextureGrayFromAsset(const FAssetData& Asset, FLoadedTexture& OutTexture)
{
    UObject* Loaded = Asset.GetAsset();
    UTexture2D* Texture = Cast<UTexture2D>(Loaded);
    if (!Texture)
    {
        return false;
    }

    return ExtractGrayFromTexture(Texture, OutTexture);
}

bool UTextureSSIMDeduplication::ExtractGrayFromTexture(UTexture2D* Texture, FLoadedTexture& OutTexture)
{
    FEvent* WaitEvent = FPlatformProcess::GetSynchEventFromPool(true);

    AsyncTask(ENamedThreads::GameThread, [Texture, &OutTexture, WaitEvent]()
        {
            bool bSucceeded = false;

            if (!Texture)
            {
                WaitEvent->Trigger();
                return;
            }

            Texture->UpdateResourceWithParams(UTexture::EUpdateResourceFlags::Synchronous);

            FTexturePlatformData* PlatformData = Texture->GetPlatformData();
            if (!PlatformData || PlatformData->Mips.Num() == 0)
            {
                WaitEvent->Trigger();
                return;
            }

            const int32 Width = Texture->Source.GetSizeX();
            const int32 Height = Texture->Source.GetSizeY();
            if (Width <= 0 || Height <= 0)
            {
                WaitEvent->Trigger();
                return;
            }

            const int32 PixelCount = Width * Height;

            OutTexture.Gray.Reset();
            OutTexture.Gray.SetNumUninitialized(PixelCount);
            OutTexture.Width = Width;
            OutTexture.Height = Height;
            OutTexture.bValid = false;

            const int32 BytesPerPixel = Texture->Source.GetBytesPerPixel();

            FTexture2DMipMap& Mip = PlatformData->Mips[0];
            const int64 BulkSize = Mip.BulkData.GetBulkDataSize();
            if (BulkSize <= 0)
            {
                WaitEvent->Trigger();
                return;
            }

            const void* RawData = Mip.BulkData.LockReadOnly();
            if (!RawData)
            {
                // не вызываем Unlock, если LockReadOnly вернул nullptr
                WaitEvent->Trigger();
                return;
            }

            const uint8* BytePtr = static_cast<const uint8*>(RawData);

            if (BytesPerPixel == 1)
            {
                for (int32 Index = 0; Index < PixelCount; ++Index)
                {
                    const uint8 GrayByte = BytePtr[Index];
                    const float L = static_cast<float>(GrayByte) / 255.0f;
                    OutTexture.Gray[Index] = FMath::Clamp(L, 0.0f, 1.0f);
                }
            }
            else
            {
                for (int32 Index = 0, Offset = 0; Index < PixelCount; ++Index, Offset += BytesPerPixel)
                {
                    uint8 R = BytePtr[Offset + 0];
                    uint8 G = (BytesPerPixel >= 2) ? BytePtr[Offset + 1] : R;
                    uint8 B = (BytesPerPixel >= 3) ? BytePtr[Offset + 2] : R;

                    const float FR = static_cast<float>(R) / 255.0f;
                    const float FG = static_cast<float>(G) / 255.0f;
                    const float FB = static_cast<float>(B) / 255.0f;

                    const float L = 0.299f * FR + 0.587f * FG + 0.114f * FB;
                    OutTexture.Gray[Index] = FMath::Clamp(L, 0.0f, 1.0f);
                }
            }

            Mip.BulkData.Unlock();

            OutTexture.bValid = true;
            WaitEvent->Trigger();
        });

    WaitEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(WaitEvent);
    return OutTexture.bValid;
}



void UTextureSSIMDeduplication::BuildGaussianKernel(int32 Radius, TArray<float>& OutKernel) const
{
    int32 Size = Radius * 2 + 1;
    OutKernel.SetNumUninitialized(Size);
    const float Sigma = FMath::Max(0.0001f, Radius / 2.0f);
    const float TwoSigmaSq = 2.0f * Sigma * Sigma;
    float Sum = 0.0f;
    for (int32 i = -Radius, idx = 0; i <= Radius; ++i, ++idx)
    {
        float Val = FMath::Exp(-(i * i) / TwoSigmaSq);
        OutKernel[idx] = Val;
        Sum += Val;
    }
    for (int32 i = 0; i < Size; ++i)
    {
        OutKernel[i] /= Sum;
    }
}

void UTextureSSIMDeduplication::SeparableGaussianBlur(const TArray<float>& In, TArray<float>& Out, int32 Width, int32 Height, const TArray<float>& Kernel) const
{
    const int32 Radius = (Kernel.Num() - 1) / 2;
    Out.SetNumUninitialized(Width * Height);

    TArray<float> Temp;
    Temp.SetNumUninitialized(Width * Height);

    for (int32 Y = 0; Y < Height; ++Y)
    {
        int32 RowOffset = Y * Width;
        for (int32 X = 0; X < Width; ++X)
        {
            float Sum = 0.0f;
            for (int32 K = -Radius; K <= Radius; ++K)
            {
                int32 SX = FMath::Clamp(X + K, 0, Width - 1);
                Sum += Kernel[K + Radius] * In[RowOffset + SX];
            }
            Temp[RowOffset + X] = Sum;
        }
    }

    for (int32 X = 0; X < Width; ++X)
    {
        for (int32 Y = 0; Y < Height; ++Y)
        {
            float Sum = 0.0f;
            for (int32 K = -Radius; K <= Radius; ++K)
            {
                int32 SY = FMath::Clamp(Y + K, 0, Height - 1);
                Sum += Kernel[K + Radius] * Temp[SY * Width + X];
            }
            Out[Y * Width + X] = Sum;
        }
    }
}

void UTextureSSIMDeduplication::Downsample2x(const TArray<float>& In, TArray<float>& Out, int32 InWidth, int32 InHeight, int32& OutWidth, int32& OutHeight) const
{
    OutWidth = FMath::Max(1, InWidth / 2);
    OutHeight = FMath::Max(1, InHeight / 2);
    Out.SetNumUninitialized(OutWidth * OutHeight);

    for (int32 Y = 0; Y < OutHeight; ++Y)
    {
        for (int32 X = 0; X < OutWidth; ++X)
        {
            int32 SX = X * 2;
            int32 SY = Y * 2;
            int32 Count = 0;
            float Sum = 0.0f;
            for (int32 j = 0; j < 2; ++j)
            {
                for (int32 i = 0; i < 2; ++i)
                {
                    int32 CX = FMath::Min(SX + i, InWidth - 1);
                    int32 CY = FMath::Min(SY + j, InHeight - 1);
                    Sum += In[CY * InWidth + CX];
                    ++Count;
                }
            }
            Out[Y * OutWidth + X] = Sum / float(Count);
        }
    }
}

float UTextureSSIMDeduplication::ComputeSSIM(const TArray<float>& A, const TArray<float>& B, int32 Width, int32 Height) const
{
    if (A.Num() != B.Num() || A.Num() == 0)
    {
        return 0.0f;
    }

    TArray<float> Kernel;
    BuildGaussianKernel(GaussianRadius, Kernel);

    TArray<float> MuA, MuB;
    SeparableGaussianBlur(A, MuA, Width, Height, Kernel);
    SeparableGaussianBlur(B, MuB, Width, Height, Kernel);

    TArray<float> A2, B2, AB;
    int32 PixelCount = Width * Height;
    A2.SetNumUninitialized(PixelCount);
    B2.SetNumUninitialized(PixelCount);
    AB.SetNumUninitialized(PixelCount);

    for (int32 i = 0; i < PixelCount; ++i)
    {
        A2[i] = A[i] * A[i];
        B2[i] = B[i] * B[i];
        AB[i] = A[i] * B[i];
    }

    TArray<float> MuA2, MuB2, MuAB;
    SeparableGaussianBlur(A2, MuA2, Width, Height, Kernel);
    SeparableGaussianBlur(B2, MuB2, Width, Height, Kernel);
    SeparableGaussianBlur(AB, MuAB, Width, Height, Kernel);

    TArray<float> SigmaA2, SigmaB2, SigmaAB;
    SigmaA2.SetNumUninitialized(PixelCount);
    SigmaB2.SetNumUninitialized(PixelCount);
    SigmaAB.SetNumUninitialized(PixelCount);

    for (int32 i = 0; i < PixelCount; ++i)
    {
        SigmaA2[i] = MuA2[i] - MuA[i] * MuA[i];
        SigmaB2[i] = MuB2[i] - MuB[i] * MuB[i];
        SigmaAB[i] = MuAB[i] - MuA[i] * MuB[i];
    }

    const float K1 = 0.01f;
    const float K2 = 0.03f;
    const float L = 1.0f;
    const float C1 = (K1 * L) * (K1 * L);
    const float C2 = (K2 * L) * (K2 * L);

    double SumSSIM = 0.0;
    for (int32 i = 0; i < PixelCount; ++i)
    {
        float MuX = MuA[i];
        float MuY = MuB[i];
        float SigmaX = SigmaA2[i];
        float SigmaY = SigmaB2[i];
        float SigmaXY = SigmaAB[i];

        float NumL = 2.0f * MuX * MuY + C1;
        float NumC = 2.0f * SigmaXY + C2;
        float DenL = (MuX * MuX + MuY * MuY + C1);
        float DenC = (SigmaX + SigmaY + C2);

        float Val = (NumL * NumC) / (DenL * DenC);
        SumSSIM += FMath::Clamp<double>(Val, 0.0, 1.0);
    }

    double MeanSSIM = SumSSIM / double(PixelCount);
    return float(MeanSSIM);
}

float UTextureSSIMDeduplication::ComputeMSSSIM(const TArray<float>& A, const TArray<float>& B, int32 Width, int32 Height) const
{
    if (A.Num() != B.Num() || A.Num() == 0)
    {
        return 0.0f;
    }

    TArray<float> Weights;
    Weights.SetNumZeroed(MSSSIMLevels);
    if (MSSSIMLevels == 5)
    {
        Weights[0] = 0.0448f;
        Weights[1] = 0.2856f;
        Weights[2] = 0.3001f;
        Weights[3] = 0.2363f;
        Weights[4] = 0.1333f;
    }
    else
    {
        float Equal = 1.0f / float(MSSSIMLevels);
        for (int32 i = 0; i < MSSSIMLevels; ++i) Weights[i] = Equal;
    }

    TArray<float> ImageA = A;
    TArray<float> ImageB = B;
    int32 CurW = Width;
    int32 CurH = Height;

    double Product = 1.0;

    for (int32 Level = 0; Level < MSSSIMLevels; ++Level)
    {
        float SsimVal = ComputeSSIM(ImageA, ImageB, CurW, CurH);
        Product *= FMath::Pow(FMath::Clamp(SsimVal, 0.0f, 1.0f), Weights[Level]);

        if (Level != MSSSIMLevels - 1)
        {
            TArray<float> DownA, DownB;
            int32 OutW = 0, OutH = 0;
            Downsample2x(ImageA, DownA, CurW, CurH, OutW, OutH);
            Downsample2x(ImageB, DownB, CurW, CurH, OutW, OutH);
            ImageA = MoveTemp(DownA);
            ImageB = MoveTemp(DownB);
            CurW = OutW;
            CurH = OutH;
            if (CurW <= 1 || CurH <= 1)
            {
                break;
            }
        }
    }

    return float(Product);
}

TArray<FDuplicateGroup> UTextureSSIMDeduplication::Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze)
{
    TArray<FDuplicateGroup> DuplicateGroups;
    const int32 TotalAssetsNumber = AssetsToAnalyze.Num();
    if (TotalAssetsNumber == 0)
    {
        SetProgress(0.0f);
        return DuplicateGroups;
    }

    TArray<FLoadedTexture> LoadedTextures;
    LoadedTextures.Reserve(TotalAssetsNumber);

    for (int32 Index = 0; Index < TotalAssetsNumber; ++Index)
    {
        SetProgress(float(Index) / float(TotalAssetsNumber));
        OnDeduplicationProgressCompleted.Broadcast();

        FLoadedTexture Loaded;
        Loaded.AssetData = AssetsToAnalyze[Index];
        bool bLoaded = LoadTextureGrayFromAsset(AssetsToAnalyze[Index], Loaded);
        if (bLoaded && Loaded.bValid && Loaded.Gray.Num() > 0)
        {
            LoadedTextures.Add(MoveTemp(Loaded));
        }
    }

    const int32 NumLoaded = LoadedTextures.Num();
    TArray<bool> Assigned;
    Assigned.Init(false, NumLoaded);

    for (int32 i = 0; i < NumLoaded; ++i)
    {
        if (Assigned[i])
        {
            continue;
        }

        TArray<FAssetData> GroupAssets;
        GroupAssets.Add(LoadedTextures[i].AssetData);
        Assigned[i] = true;

        for (int32 j = i + 1; j < NumLoaded; ++j)
        {
            if (Assigned[j])
            {
                continue;
            }

            const FLoadedTexture& A = LoadedTextures[i];
            const FLoadedTexture& B = LoadedTextures[j];

            if (A.Width != B.Width || A.Height != B.Height)
            {
                continue;
            }

            float Similarity = ComputeMSSSIM(A.Gray, B.Gray, A.Width, A.Height);

            if (Similarity >= SimilarityThreshold)
            {
                GroupAssets.Add(B.AssetData);
                Assigned[j] = true;
            }
        }

        if (GroupAssets.Num() > 1)
        {
            float ConfidenceScore = CalculateConfidenceScore(GroupAssets);
            FDuplicateGroup DuplicateGroup = CreateDuplicateGroup(GroupAssets, ConfidenceScore);
            DuplicateGroups.Add(DuplicateGroup);
        }
    }

    SetProgress(1.0f);
    OnDeduplicationProgressCompleted.Broadcast();

    return DuplicateGroups;
}

float UTextureSSIMDeduplication::CalculateConfidenceScore_Implementation(const TArray<FAssetData>& CheckAssets) const
{
    const int32 NumAssets = CheckAssets.Num();
    if (NumAssets <= 1)
    {
        return 1.0f;
    }

    TArray<FLoadedTexture> Loaded;
    Loaded.Reserve(NumAssets);

    for (const FAssetData& Asset : CheckAssets)
    {
        FLoadedTexture LT;
        const_cast<UTextureSSIMDeduplication*>(this)->LoadTextureGrayFromAsset(Asset, LT);
        if (LT.bValid)
        {
            Loaded.Add(MoveTemp(LT));
        }
    }

    const int32 N = Loaded.Num();
    if (N <= 1)
    {
        return 1.0f;
    }

    double TotalSimilarity = 0.0;
    int64 PairCount = 0;

    for (int32 A = 0; A < N - 1; ++A)
    {
        for (int32 B = A + 1; B < N; ++B)
        {
            if (Loaded[A].Width != Loaded[B].Width || Loaded[A].Height != Loaded[B].Height)
            {
                continue;
            }

            float Sim = ComputeMSSSIM(Loaded[A].Gray, Loaded[B].Gray, Loaded[A].Width, Loaded[A].Height);
            TotalSimilarity += double(Sim);
            ++PairCount;
        }
    }

    if (PairCount == 0)
    {
        return 1.0f;
    }

    float Average = float(TotalSimilarity / double(PairCount));
    return FMath::Clamp(Average, 0.0f, 1.0f);
}
