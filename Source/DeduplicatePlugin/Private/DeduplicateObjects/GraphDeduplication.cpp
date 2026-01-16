/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#include "DeduplicateObjects/GraphDeduplication.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"
#include "UObject/Script.h"
#include "Engine/AssetManager.h"
#include "Blueprint/BlueprintSupport.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "Misc/DefaultValueHelper.h"

bool FGraphNodeSignature::operator==(const FGraphNodeSignature& Other) const
{
	if (NodeClassName != Other.NodeClassName)
	{
		return false;
	}

	if (InputPinNames.Num() != Other.InputPinNames.Num())
	{
		return false;
	}

	if (OutputPinNames.Num() != Other.OutputPinNames.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < InputPinNames.Num(); ++Index)
	{
		if (InputPinNames[Index] != Other.InputPinNames[Index])
		{
			return false;
		}
	}

	for (int32 Index = 0; Index < OutputPinNames.Num(); ++Index)
	{
		if (OutputPinNames[Index] != Other.OutputPinNames[Index])
		{
			return false;
		}
	}

	if (PropertyValues.Num() != Other.PropertyValues.Num())
	{
		return false;
	}

	for (const auto& PropertyPair : PropertyValues)
	{
		const FString* OtherValue = Other.PropertyValues.Find(PropertyPair.Key);
		if (OtherValue == nullptr || *OtherValue != PropertyPair.Value)
		{
			return false;
		}
	}

	return true;
}

bool FGraphSignature::operator==(const FGraphSignature& Other) const
{
	if (TotalNodeCount != Other.TotalNodeCount)
	{
		return false;
	}

	if (NodeSignatures.Num() != Other.NodeSignatures.Num())
	{
		return false;
	}

	TArray<bool> MatchedNodes;
	MatchedNodes.Init(false, NodeSignatures.Num());

	for (int32 IndexA = 0; IndexA < NodeSignatures.Num(); ++IndexA)
	{
		const FGraphNodeSignature& SignatureA = NodeSignatures[IndexA];
		bool bFoundMatch = false;

		for (int32 IndexB = 0; IndexB < Other.NodeSignatures.Num(); ++IndexB)
		{
			if (MatchedNodes[IndexB])
			{
				continue;
			}

			if (SignatureA == Other.NodeSignatures[IndexB])
			{
				MatchedNodes[IndexB] = true;
				bFoundMatch = true;
				break;
			}
		}

		if (!bFoundMatch)
		{
			return false;
		}
	}

	return true;
}

UGraphDeduplication::UGraphDeduplication()
{
	PenaltyByNodeDifference = 0.05f;
	PenaltyByPropertyDifference = 0.1f;
	bCompareNodeProperties = true;
	bComparePinNames = true;
}

bool UGraphDeduplication::ShouldLoadAssets_Implementation()
{
	return true;
}

TArray<FDuplicateGroup> UGraphDeduplication::Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze)
{
	TArray<FDuplicateGroup> DuplicateGroups;

	if (AssetsToAnalyze.Num() < 2)
	{
		return DuplicateGroups;
	}

	TArray<UObject*> LoadedObjects;
	TArray<TArray<FGraphSignature>> ObjectGraphSignatures;
	LoadedObjects.Reserve(AssetsToAnalyze.Num());
	ObjectGraphSignatures.Reserve(AssetsToAnalyze.Num());

	for (const FAssetData& AssetData : AssetsToAnalyze)
	{
		if (ShouldStop())
		{
			break;
		}
		
		UObject* LoadedObject = AssetData.GetAsset();
		if (LoadedObject != nullptr)
		{
			LoadedObjects.Add(LoadedObject);
			TArray<FGraphSignature> Signatures = ExtractGraphSignatures(LoadedObject);
			ObjectGraphSignatures.Add(Signatures);
		}
		else
		{
			ObjectGraphSignatures.Add(TArray<FGraphSignature>());
		}
	}

	if (LoadedObjects.Num() < 2)
	{
		return DuplicateGroups;
	}

	TMap<int32, TArray<FAssetData>> SimilarityGroups;
	int32 TotalComparisons = LoadedObjects.Num() * (LoadedObjects.Num() - 1) / 2;
	int32 CurrentComparison = 0;

	for (int32 IndexA = 0; IndexA < LoadedObjects.Num() - 1; ++IndexA)
	{
		if (ShouldStop())
		{
			break;
		}
		
		if (LoadedObjects[IndexA] == nullptr)
		{
			continue;
		}

		TArray<FAssetData> CurrentGroup;
		CurrentGroup.Add(AssetsToAnalyze[IndexA]);

		const TArray<FGraphSignature>& SignaturesA = ObjectGraphSignatures[IndexA];
		float GraphSizeA = CalculateGraphSize(SignaturesA);

		for (int32 IndexB = IndexA + 1; IndexB < LoadedObjects.Num(); ++IndexB)
		{
			if (ShouldStop())
			{
				break;
			}
			
			if (LoadedObjects[IndexB] == nullptr)
			{
				continue;
			}

			const TArray<FGraphSignature>& SignaturesB = ObjectGraphSignatures[IndexB];
			float GraphSizeB = CalculateGraphSize(SignaturesB);

			float Similarity = CompareGraphSignatures(SignaturesA, SignaturesB);
			
			float MaxGraphSize = FMath::Max(GraphSizeA, GraphSizeB);
			if (MaxGraphSize > 0.0f)
			{
				float SizePenalty = FMath::Abs(GraphSizeA - GraphSizeB) / MaxGraphSize;
				Similarity = FMath::Clamp(Similarity - SizePenalty * PenaltyByNodeDifference, 0.0f, 1.0f);
			}

			if (Similarity >= SimilarityThreshold)
			{
				bool bAlreadyInGroup = false;
				for (const FAssetData& ExistingAsset : CurrentGroup)
				{
					if (ExistingAsset == AssetsToAnalyze[IndexB])
					{
						bAlreadyInGroup = true;
						break;
					}
				}

				if (!bAlreadyInGroup)
				{
					CurrentGroup.Add(AssetsToAnalyze[IndexB]);
				}
			}

			CurrentComparison++;
			if (CurrentComparison % 10 == 0)
			{
				float ProgressValue = static_cast<float>(CurrentComparison) / static_cast<float>(TotalComparisons);
				SetProgress(ProgressValue);
				OnDeduplicationProgressCompleted.Broadcast();
			}
		}

		if (CurrentGroup.Num() > 1)
		{
			bool bGroupExists = false;
			for (auto& GroupPair : SimilarityGroups)
			{
				TArray<FAssetData>& ExistingGroup = GroupPair.Value;
				bool bHasCommonAsset = false;
				for (const FAssetData& NewAsset : CurrentGroup)
				{
					for (const FAssetData& ExistingAsset : ExistingGroup)
					{
						if (NewAsset == ExistingAsset)
						{
							bHasCommonAsset = true;
							break;
						}
					}
					if (bHasCommonAsset)
					{
						break;
					}
				}

				if (bHasCommonAsset)
				{
					for (const FAssetData& NewAsset : CurrentGroup)
					{
						bool bAlreadyExists = false;
						for (const FAssetData& ExistingAsset : ExistingGroup)
						{
							if (NewAsset == ExistingAsset)
							{
								bAlreadyExists = true;
								break;
							}
						}
						if (!bAlreadyExists)
						{
							ExistingGroup.Add(NewAsset);
						}
					}
					bGroupExists = true;
					break;
				}
			}

			if (!bGroupExists)
			{
				SimilarityGroups.Add(SimilarityGroups.Num(), CurrentGroup);
			}
		}
	}

	for (auto& GroupPair : SimilarityGroups)
	{
		const TArray<FAssetData>& GroupAssets = GroupPair.Value;
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

FString UGraphDeduplication::GetAlgorithmName_Implementation() const
{
	return TEXT("Graph Deduplication");
}

float UGraphDeduplication::CalculateConfidenceScore_Implementation(const TArray<FAssetData>& Assets) const
{
	const int32 NumAssets = Assets.Num();
	if (NumAssets <= 1)
	{
		return 1.0f;
	}

	TArray<UObject*> LoadedObjects;
	TArray<TArray<FGraphSignature>> ObjectGraphSignatures;

	for (const FAssetData& AssetData : Assets)
	{
		UObject* LoadedObject = AssetData.GetAsset();
		if (LoadedObject != nullptr)
		{
			LoadedObjects.Add(LoadedObject);
			TArray<FGraphSignature> Signatures = ExtractGraphSignatures(LoadedObject);
			ObjectGraphSignatures.Add(Signatures);
		}
		else
		{
			ObjectGraphSignatures.Add(TArray<FGraphSignature>());
		}
	}

	if (LoadedObjects.Num() < 2)
	{
		return 0.0f;
	}

	double TotalSimilarity = 0.0;
	int64 PairCount = 0;

	for (int32 IndexA = 0; IndexA < LoadedObjects.Num() - 1; ++IndexA)
	{
		if (ShouldStop())
		{
			break;
		}
		
		if (LoadedObjects[IndexA] == nullptr)
		{
			continue;
		}

		const TArray<FGraphSignature>& SignaturesA = ObjectGraphSignatures[IndexA];
		float GraphSizeA = CalculateGraphSize(SignaturesA);

		for (int32 IndexB = IndexA + 1; IndexB < LoadedObjects.Num(); ++IndexB)
		{
			if (ShouldStop())
			{
				break;
			}
			
			if (LoadedObjects[IndexB] == nullptr)
			{
				continue;
			}

			const TArray<FGraphSignature>& SignaturesB = ObjectGraphSignatures[IndexB];
			float GraphSizeB = CalculateGraphSize(SignaturesB);

			float Similarity = CompareGraphSignatures(SignaturesA, SignaturesB);

			float MaxGraphSize = FMath::Max(GraphSizeA, GraphSizeB);
			if (MaxGraphSize > 0.0f)
			{
				float SizePenalty = FMath::Abs(GraphSizeA - GraphSizeB) / MaxGraphSize;
				Similarity = FMath::Clamp(Similarity - SizePenalty * PenaltyByNodeDifference, 0.0f, 1.0f);
			}

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

float UGraphDeduplication::CalculateComplexity_Implementation(const TArray<FAssetData>& CheckAssets)
{
	int32 NumAssets = CheckAssets.Num();
	AlgorithmComplexity = static_cast<float>(NumAssets * (NumAssets - 1) / 2);
	return AlgorithmComplexity;
}

TArray<FGraphSignature> UGraphDeduplication::ExtractGraphSignatures(UObject* Object) const
{
	TArray<FGraphSignature> Signatures;

	if (Object == nullptr)
	{
		return Signatures;
	}

	TArray<UEdGraph*> AllGraphs = GetAllGraphsFromObject(Object);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph != nullptr)
		{
			FGraphSignature Signature = ExtractGraphSignature(Graph);
			Signatures.Add(Signature);
		}
	}

	return Signatures;
}

TArray<UEdGraph*> UGraphDeduplication::GetAllGraphsFromObject(UObject* Object) const
{
	TArray<UEdGraph*> Graphs;

	if (Object == nullptr)
	{
		return Graphs;
	}

	UClass* ObjectClass = Object->GetClass();
	if (ObjectClass == nullptr)
	{
		return Graphs;
	}

	if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
	{
		Graphs.Append(Blueprint->UbergraphPages);
		Graphs.Append(Blueprint->FunctionGraphs);
		Graphs.Append(Blueprint->MacroGraphs);
	}
	else
	{
		for (TFieldIterator<FProperty> PropertyIterator(ObjectClass); PropertyIterator; ++PropertyIterator)
		{
			FProperty* Property = *PropertyIterator;
			if (Property == nullptr)
			{
				continue;
			}

			if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				if (ObjectProperty->PropertyClass != nullptr && ObjectProperty->PropertyClass->IsChildOf(UEdGraph::StaticClass()))
				{
					UObject* GraphObject = ObjectProperty->GetObjectPropertyValue_InContainer(Object);
					if (UEdGraph* Graph = Cast<UEdGraph>(GraphObject))
					{
						if (Graph != nullptr)
						{
							Graphs.Add(Graph);
						}
					}
				}
			}
			else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				FObjectPropertyBase* InnerObjectProperty = CastField<FObjectPropertyBase>(ArrayProperty->Inner);
				if (InnerObjectProperty != nullptr && InnerObjectProperty->PropertyClass != nullptr && InnerObjectProperty->PropertyClass->IsChildOf(UEdGraph::StaticClass()))
				{
					FScriptArrayHelper ArrayHelper(ArrayProperty, Property->ContainerPtrToValuePtr<void>(Object));
					for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
					{
						UObject* GraphObject = InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Index));
						if (UEdGraph* Graph = Cast<UEdGraph>(GraphObject))
						{
							if (Graph != nullptr)
							{
								Graphs.Add(Graph);
							}
						}
					}
				}
			}
		}
	}

	return Graphs;
}

FGraphSignature UGraphDeduplication::ExtractGraphSignature(UEdGraph* Graph) const
{
	FGraphSignature Signature;

	if (Graph == nullptr)
	{
		return Signature;
	}

	TArray<UEdGraphNode*> Nodes = Graph->Nodes;
	Signature.TotalNodeCount = Nodes.Num();

	for (UEdGraphNode* Node : Nodes)
	{
		if (Node != nullptr)
		{
			FGraphNodeSignature NodeSignature = ExtractNodeSignature(Node);
			Signature.NodeSignatures.Add(NodeSignature);
		}
	}

	return Signature;
}

FGraphNodeSignature UGraphDeduplication::ExtractNodeSignature(UEdGraphNode* Node) const
{
	FGraphNodeSignature Signature;

	if (Node == nullptr)
	{
		return Signature;
	}

	Signature.NodeClassName = Node->GetClass()->GetName();

	if (bComparePinNames)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin != nullptr)
			{
				if (Pin->Direction == EGPD_Input)
				{
					Signature.InputPinNames.Add(Pin->PinName.ToString());
				}
				else if (Pin->Direction == EGPD_Output)
				{
					Signature.OutputPinNames.Add(Pin->PinName.ToString());
				}
			}
		}

		Signature.InputPinNames.Sort();
		Signature.OutputPinNames.Sort();
	}

	if (bCompareNodeProperties)
	{
		ExtractNodePropertyValues(Node, Signature.PropertyValues);
	}

	return Signature;
}

void UGraphDeduplication::ExtractNodePropertyValues(UEdGraphNode* Node, TMap<FString, FString>& OutPropertyValues) const
{
	if (Node == nullptr)
	{
		return;
	}

	UClass* NodeClass = Node->GetClass();
	if (NodeClass == nullptr)
	{
		return;
	}

	for (TFieldIterator<FProperty> PropertyIterator(NodeClass); PropertyIterator; ++PropertyIterator)
	{
		FProperty* Property = *PropertyIterator;
		if (Property == nullptr)
		{
			continue;
		}

		if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			continue;
		}

		if (Property->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		const void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Node);
		FString PropertyValueString;

		if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
		{
			if (NumericProperty->IsFloatingPoint())
			{
				double Value = NumericProperty->GetFloatingPointPropertyValue(PropertyValue);
				PropertyValueString = FString::Format(TEXT("{0}"), { Value });
			}
			else
			{
				int64 Value = NumericProperty->GetSignedIntPropertyValue(PropertyValue);
				PropertyValueString = FString::Format(TEXT("{0}"), { Value });
			}
		}
		else if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			bool Value = BoolProperty->GetPropertyValue(PropertyValue);
			PropertyValueString = Value ? TEXT("True") : TEXT("False");
		}
		else if (const FStrProperty* StrProperty = CastField<FStrProperty>(Property))
		{
			PropertyValueString = StrProperty->GetPropertyValue(PropertyValue);
		}
		else if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			PropertyValueString = NameProperty->GetPropertyValue(PropertyValue).ToString();
		}
		else if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			PropertyValueString = TextProperty->GetPropertyValue(PropertyValue).ToString();
		}
		else if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			UObject* ValueObject = ObjectProperty->GetObjectPropertyValue(PropertyValue);
			if (ValueObject != nullptr)
			{
				PropertyValueString = ValueObject->GetPathName();
			}
			else
			{
				PropertyValueString = TEXT("None");
			}
		}

		if (!PropertyValueString.IsEmpty())
		{
			OutPropertyValues.Add(Property->GetName(), PropertyValueString);
		}
	}
}

float UGraphDeduplication::CompareGraphSignatures(const TArray<FGraphSignature>& SignaturesA, const TArray<FGraphSignature>& SignaturesB) const
{
	if (SignaturesA.Num() == 0 && SignaturesB.Num() == 0)
	{
		return 1.0f;
	}

	if (SignaturesA.Num() == 0 || SignaturesB.Num() == 0)
	{
		return 0.0f;
	}

	int32 TotalGraphs = FMath::Max(SignaturesA.Num(), SignaturesB.Num());
	int32 MatchedGraphs = 0;
	float TotalSimilarity = 0.0f;

	TArray<bool> MatchedGraphsB;
	MatchedGraphsB.Init(false, SignaturesB.Num());

	for (const FGraphSignature& SignatureA : SignaturesA)
	{
		float BestMatch = 0.0f;
		int32 BestMatchIndex = -1;

		for (int32 IndexB = 0; IndexB < SignaturesB.Num(); ++IndexB)
		{
			if (MatchedGraphsB[IndexB])
			{
				continue;
			}

			const FGraphSignature& SignatureB = SignaturesB[IndexB];

			if (SignatureA == SignatureB)
			{
				BestMatch = 1.0f;
				BestMatchIndex = IndexB;
				break;
			}
			else
			{
				int32 TotalNodesA = SignatureA.NodeSignatures.Num();
				int32 TotalNodesB = SignatureB.NodeSignatures.Num();
				int32 MaxNodes = FMath::Max(TotalNodesA, TotalNodesB);

				if (MaxNodes == 0)
				{
					continue;
				}

				int32 MatchedNodes = 0;
				TArray<bool> MatchedNodesB;
				MatchedNodesB.Init(false, TotalNodesB);

				for (const FGraphNodeSignature& NodeSignatureA : SignatureA.NodeSignatures)
				{
					for (int32 NodeIndexB = 0; NodeIndexB < TotalNodesB; ++NodeIndexB)
					{
						if (MatchedNodesB[NodeIndexB])
						{
							continue;
						}

						if (NodeSignatureA == SignatureB.NodeSignatures[NodeIndexB])
						{
							MatchedNodes++;
							MatchedNodesB[NodeIndexB] = true;
							break;
						}
					}
				}

				float NodeSimilarity = static_cast<float>(MatchedNodes) / static_cast<float>(MaxNodes);
				if (NodeSimilarity > BestMatch)
				{
					BestMatch = NodeSimilarity;
					BestMatchIndex = IndexB;
				}
			}
		}

		if (BestMatchIndex >= 0)
		{
			MatchedGraphsB[BestMatchIndex] = true;
			MatchedGraphs++;
		}

		TotalSimilarity += BestMatch;
	}

	float AverageSimilarity = TotalSimilarity / static_cast<float>(TotalGraphs);
	return FMath::Clamp(AverageSimilarity, 0.0f, 1.0f);
}

float UGraphDeduplication::CalculateGraphSize(const TArray<FGraphSignature>& Signatures) const
{
	float TotalSize = 0.0f;
	for (const FGraphSignature& Signature : Signatures)
	{
		TotalSize += static_cast<float>(Signature.TotalNodeCount);
	}
	return TotalSize;
}
