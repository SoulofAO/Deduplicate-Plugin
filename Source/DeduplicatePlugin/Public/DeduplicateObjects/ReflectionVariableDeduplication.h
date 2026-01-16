/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "DeduplicateObject.h"
#include "ReflectionVariableDeduplication.generated.h"

/**
 * Deduplication algorithm that analyzes equivalence of all variables in objects and their default values,
 * accessible through reflection. Compares properties marked with UPROPERTY and their default values.
 */
UCLASS(BlueprintType, Blueprintable)
class DEDUPLICATEPLUGIN_API UReflectionVariableDeduplication : public UDeduplicateObject
{
	GENERATED_BODY()

public:
	UReflectionVariableDeduplication();

	virtual bool ShouldLoadAssets_Implementation() override;

	virtual TArray<FDuplicateGroup> Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze) override;

	virtual FString GetAlgorithmName_Implementation() const override;

protected:
	virtual float CalculateConfidenceScore_Implementation(const TArray<FAssetData>& Assets) const override;

	virtual float CalculateComplexity_Implementation(const TArray<FAssetData>& CheckAssets) override;

private:
	float CompareObjectsByReflection(UObject* ObjectA, UObject* ObjectB) const;

	FString GetPropertySignature(const FProperty* Property) const;

	bool ArePropertyValuesEqual(const FProperty* Property, const void* ValueA, const void* ValueB) const;

	UClass* GetRealClass(UObject* Object) const;

	UClass* GetCppParentClass(UObject* Object) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reflection Deduplication", meta = (AllowPrivateAccess = "true"))
	float PenaltyByPropertyDifference = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reflection Deduplication", meta = (AllowPrivateAccess = "true"))
	float PenaltyByStructureDifference = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reflection Deduplication", meta = (AllowPrivateAccess = "true"))
	bool bCompareOnlyVisibleProperties = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reflection Deduplication", meta = (AllowPrivateAccess = "true"))
	bool bIgnoreTransientProperties = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reflection Deduplication", meta = (AllowPrivateAccess = "true"))
	bool bIgnoreEditDefaultsOnlyProperties = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reflection Deduplication", meta = (AllowPrivateAccess = "true"))
	bool bRequireSameParentClass = true;
};
