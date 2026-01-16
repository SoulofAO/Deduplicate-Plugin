/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "IAssetTools.h"
#include "DeduplicationFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class DEDUPLICATEPLUGIN_API UDeduplicationFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static void ExecuteFixUp_NoUI(TArray<TWeakObjectPtr<UObjectRedirector>> Objects);

	static TArray<FAssetData> FilterRedirects(const TArray<FAssetData>& Assets);

	static int32 FindCommonSubstrings(const TArray<uint8>& Data1, const TArray<uint8>& Data2, int32 MinLength);
	
	static int ComputeLevenshteinDistance(const FString& Str1, const FString& Str2);

};
