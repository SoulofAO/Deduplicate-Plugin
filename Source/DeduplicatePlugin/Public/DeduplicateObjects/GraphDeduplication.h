/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "DeduplicateObject.h"
#include "GraphDeduplication.generated.h"

class UEdGraph;
class UEdGraphNode;

USTRUCT()
struct FGraphNodeSignature
{
	GENERATED_BODY()

	UPROPERTY()
	FString NodeClassName;

	UPROPERTY()
	TArray<FString> InputPinNames;

	UPROPERTY()
	TArray<FString> OutputPinNames;

	UPROPERTY()
	TMap<FString, FString> PropertyValues;

	bool operator==(const FGraphNodeSignature& Other) const;
};

USTRUCT()
struct FGraphSignature
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGraphNodeSignature> NodeSignatures;

	UPROPERTY()
	int32 TotalNodeCount = 0;

	bool operator==(const FGraphSignature& Other) const;
};

/**
 * Deduplication algorithm that analyzes all graphs belonging to an object.
 * Reads their nodes and attempts to find similarity by categorizing them by input parameters,
 * node classes, and equivalence of their reflection (internal variables marked as Property).
 * Penalty is calculated relative to the size of all graphs.
 */
UCLASS(BlueprintType, Blueprintable)
class DEDUPLICATEPLUGIN_API UGraphDeduplication : public UDeduplicateObject
{
	GENERATED_BODY()

public:
	UGraphDeduplication();

	virtual bool ShouldLoadAssets_Implementation() override;

	virtual TArray<FDuplicateGroup> Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze) override;

	virtual FString GetAlgorithmName_Implementation() const override;

protected:
	virtual float CalculateConfidenceScore_Implementation(const TArray<FAssetData>& Assets) const override;

	virtual float CalculateComplexity_Implementation(const TArray<FAssetData>& CheckAssets) override;

private:
	TArray<FGraphSignature> ExtractGraphSignatures(UObject* Object) const;

	TArray<UEdGraph*> GetAllGraphsFromObject(UObject* Object) const;

	FGraphSignature ExtractGraphSignature(UEdGraph* Graph) const;

	FGraphNodeSignature ExtractNodeSignature(UEdGraphNode* Node) const;

	void ExtractNodePropertyValues(UEdGraphNode* Node, TMap<FString, FString>& OutPropertyValues) const;

	float CompareGraphSignatures(const TArray<FGraphSignature>& SignaturesA, const TArray<FGraphSignature>& SignaturesB) const;

	float CalculateGraphSize(const TArray<FGraphSignature>& Signatures) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graph Deduplication", meta = (AllowPrivateAccess = "true"))
	float PenaltyByNodeDifference = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graph Deduplication", meta = (AllowPrivateAccess = "true"))
	float PenaltyByPropertyDifference = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graph Deduplication", meta = (AllowPrivateAccess = "true"))
	bool bCompareNodeProperties = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graph Deduplication", meta = (AllowPrivateAccess = "true"))
	bool bComparePinNames = true;
};
