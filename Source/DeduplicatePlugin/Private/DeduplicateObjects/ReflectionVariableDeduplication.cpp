/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#include "DeduplicateObjects/ReflectionVariableDeduplication.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"
#include "UObject/Script.h"
#include "Engine/AssetManager.h"
#include "Misc/DefaultValueHelper.h"
#include "Engine/Blueprint.h"

UReflectionVariableDeduplication::UReflectionVariableDeduplication()
{
	IncludeClasses = { UBlueprint::StaticClass() };
}

bool UReflectionVariableDeduplication::ShouldLoadAssets_Implementation()
{
	return true;
}

TArray<FDuplicateGroup> UReflectionVariableDeduplication::Internal_FindDuplicates_Implementation(const TArray<FAssetData>& AssetsToAnalyze)
{
	TArray<FDuplicateGroup> DuplicateGroups;

	if (AssetsToAnalyze.Num() < 2)
	{
		return DuplicateGroups;
	}

	TArray<UObject*> LoadedObjects;
	TMap<int32, int32> LoadedObjectIndexToAssetIndex;
	LoadedObjects.Reserve(AssetsToAnalyze.Num());

	for (int32 AssetIndex = 0; AssetIndex < AssetsToAnalyze.Num(); ++AssetIndex)
	{
		const FAssetData& AssetData = AssetsToAnalyze[AssetIndex];
		UObject* LoadedObject = AssetData.GetAsset();
		if (LoadedObject != nullptr)
		{
			int32 LoadedObjectIndex = LoadedObjects.Add(LoadedObject);
			LoadedObjectIndexToAssetIndex.Add(LoadedObjectIndex, AssetIndex);
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
		
		UObject* ObjectA = LoadedObjects[IndexA];
		if (ObjectA == nullptr)
		{
			continue;
		}

		TArray<FAssetData> CurrentGroup;
		CurrentGroup.Add(AssetsToAnalyze[IndexA]);

		for (int32 IndexB = IndexA + 1; IndexB < LoadedObjects.Num(); ++IndexB)
		{
			if (ShouldStop())
			{
				break;
			}
			
			UObject* ObjectB = LoadedObjects[IndexB];
			if (ObjectB == nullptr)
			{
				continue;
			}

			float Similarity = CompareObjectsByReflection(ObjectA, ObjectB);
			if (Similarity >= SimilarityThreshold)
			{
				int32 AssetIndexB = LoadedObjectIndexToAssetIndex[IndexB];
				bool bAlreadyInGroup = false;
				for (const FAssetData& ExistingAsset : CurrentGroup)
				{
					if (ExistingAsset == AssetsToAnalyze[AssetIndexB])
					{
						bAlreadyInGroup = true;
						break;
					}
				}

				if (!bAlreadyInGroup)
				{
					CurrentGroup.Add(AssetsToAnalyze[AssetIndexB]);
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

FString UReflectionVariableDeduplication::GetAlgorithmName_Implementation() const
{
	return TEXT("Reflection Variable Deduplication");
}

float UReflectionVariableDeduplication::CalculateConfidenceScore_Implementation(const TArray<FAssetData>& Assets) const
{
	const int32 NumAssets = Assets.Num();
	if (NumAssets <= 1)
	{
		return 1.0f;
	}

	TArray<UObject*> LoadedObjects;
	for (const FAssetData& AssetData : Assets)
	{
		UObject* LoadedObject = AssetData.GetAsset();
		if (LoadedObject != nullptr)
		{
			LoadedObjects.Add(LoadedObject);
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
		UObject* ObjectA = LoadedObjects[IndexA];
		if (ObjectA == nullptr)
		{
			continue;
		}

		for (int32 IndexB = IndexA + 1; IndexB < LoadedObjects.Num(); ++IndexB)
		{
			UObject* ObjectB = LoadedObjects[IndexB];
			if (ObjectB == nullptr)
			{
				continue;
			}

			float Similarity = CompareObjectsByReflection(ObjectA, ObjectB);
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

float UReflectionVariableDeduplication::CalculateComplexity_Implementation(const TArray<FAssetData>& CheckAssets)
{
	int32 NumAssets = CheckAssets.Num();
	AlgorithmComplexity = static_cast<float>(NumAssets * (NumAssets - 1) / 2);
	return AlgorithmComplexity;
}

float UReflectionVariableDeduplication::CompareObjectsByReflection(UObject* ObjectA, UObject* ObjectB) const
{
	if (ObjectA == nullptr || ObjectB == nullptr)
	{
		return 0.0f;
	}

	if (bRequireSameParentClass)
	{
		UClass* ParentClassA = GetCppParentClass(ObjectA);
		UClass* ParentClassB = GetCppParentClass(ObjectB);

		if (ParentClassA != ParentClassB)
		{
			return 0.0f;
		}
	}

	UClass* ClassA = GetRealClass(ObjectA);
	UClass* ClassB = GetRealClass(ObjectB);

	if (ClassA == nullptr || ClassB == nullptr)
	{
		return 0.0f;
	}

	UObject* DefaultObjectA = ClassA->GetDefaultObject();
	UObject* DefaultObjectB = ClassB->GetDefaultObject();

	if (DefaultObjectA == nullptr || DefaultObjectB == nullptr)
	{
		return 0.0f;
	}

	TMap<FName, FProperty*> PropertiesMapB;
	for (TFieldIterator<FProperty> PropertyIterator(ClassB); PropertyIterator; ++PropertyIterator)
	{
		FProperty* Property = *PropertyIterator;
		if (Property == nullptr)
		{
			continue;
		}

		if (bCompareOnlyVisibleProperties)
		{
			if (Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance))
			{
				continue;
			}
		}

		if (bIgnoreTransientProperties)
		{
			if (Property->HasAnyPropertyFlags(CPF_Transient))
			{
				continue;
			}
		}

		if (bIgnoreEditDefaultsOnlyProperties)
		{
			if (Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance) && !Property->HasAnyPropertyFlags(CPF_Edit))
			{
				continue;
			}
		}

		if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			continue;
		}

		PropertiesMapB.Add(Property->GetFName(), Property);
	}

	int32 TotalPropertiesInB = PropertiesMapB.Num();

	int32 MatchingProperties = 0;
	int32 DifferentProperties = 0;
	int32 StructureDifference = 0;
	int32 TotalPropertiesInA = 0;

	for (TFieldIterator<FProperty> PropertyIterator(ClassA); PropertyIterator; ++PropertyIterator)
	{
		FProperty* PropertyA = *PropertyIterator;
		if (PropertyA == nullptr)
		{
			continue;
		}

		if (bCompareOnlyVisibleProperties)
		{
			if (PropertyA->HasAnyPropertyFlags(CPF_DisableEditOnInstance))
			{
				continue;
			}
		}

		if (bIgnoreTransientProperties)
		{
			if (PropertyA->HasAnyPropertyFlags(CPF_Transient))
			{
				continue;
			}
		}

		if (bIgnoreEditDefaultsOnlyProperties)
		{
			if (PropertyA->HasAnyPropertyFlags(CPF_DisableEditOnInstance) && !PropertyA->HasAnyPropertyFlags(CPF_Edit))
			{
				continue;
			}
		}

		if (!PropertyA->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			continue;
		}

		TotalPropertiesInA++;

		FProperty** PropertyBFound = PropertiesMapB.Find(PropertyA->GetFName());
		if (PropertyBFound == nullptr)
		{
			StructureDifference++;
			continue;
		}

		FProperty* PropertyB = *PropertyBFound;
		if (PropertyB == nullptr)
		{
			StructureDifference++;
			PropertiesMapB.Remove(PropertyA->GetFName());
			continue;
		}

		FString SignatureA = GetPropertySignature(PropertyA);
		FString SignatureB = GetPropertySignature(PropertyB);

		if (SignatureA != SignatureB)
		{
			StructureDifference++;
			PropertiesMapB.Remove(PropertyA->GetFName());
			continue;
		}

		const void* ValueA = PropertyA->ContainerPtrToValuePtr<void>(DefaultObjectA);
		const void* ValueB = PropertyB->ContainerPtrToValuePtr<void>(DefaultObjectB);

		PropertiesMapB.Remove(PropertyA->GetFName());

		if (ArePropertyValuesEqual(PropertyA, ValueA, ValueB))
		{
			MatchingProperties++;
		}
		else
		{
			DifferentProperties++;
		}
	}

	int32 PropertiesOnlyInB = PropertiesMapB.Num();
	StructureDifference += PropertiesOnlyInB;

	int32 TotalProperties = FMath::Max(TotalPropertiesInA, TotalPropertiesInB);

	if (TotalProperties == 0)
	{
		return 1.0f;
	}

	float StructurePenalty = static_cast<float>(StructureDifference) * PenaltyByStructureDifference;
	float Similarity = static_cast<float>(MatchingProperties) / static_cast<float>(TotalProperties);
	float ValuePenalty = static_cast<float>(DifferentProperties) * PenaltyByPropertyDifference;
	float FinalSimilarity = FMath::Clamp(Similarity - ValuePenalty - StructurePenalty, 0.0f, 1.0f);

	return FinalSimilarity;
}

FString UReflectionVariableDeduplication::GetPropertySignature(const FProperty* Property) const
{
	if (Property == nullptr)
	{
		return FString();
	}

	FString PropertyName = Property->GetName();
	FString PropertyType = Property->GetCPPType();
	return FString::Format(TEXT("{0}:{1}"), { PropertyName, PropertyType });
}

bool UReflectionVariableDeduplication::ArePropertyValuesEqual(const FProperty* Property, const void* ValueA, const void* ValueB) const
{
	if (Property == nullptr || ValueA == nullptr || ValueB == nullptr)
	{
		return false;
	}

	FString TextA;
	FString TextB;

	const uint8* DataA = static_cast<const uint8*>(ValueA);
	const uint8* DataB = static_cast<const uint8*>(ValueB);

	Property->ExportTextItem_Direct(TextA, DataA, DataA, nullptr, PPF_None, nullptr);
	Property->ExportTextItem_Direct(TextB, DataB, DataB, nullptr, PPF_None, nullptr);

	return TextA == TextB;
}

UClass* UReflectionVariableDeduplication::GetRealClass(UObject* Object) const
{
	if (Object == nullptr)
	{
		return nullptr;
	}

	UClass* ObjectClass = Object->GetClass();

	if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
	{
		if (Blueprint->GeneratedClass != nullptr)
		{
			return Blueprint->GeneratedClass;
		}

	}

	return ObjectClass;
}

UClass* UReflectionVariableDeduplication::GetCppParentClass(UObject* Object) const
{
	if (Object == nullptr)
	{
		return nullptr;
	}

	UClass* RealClass = GetRealClass(Object);
	if (RealClass == nullptr)
	{
		return nullptr;
	}

	UClass* CurrentClass = RealClass;
	while (CurrentClass != nullptr)
	{
		UClass* SuperClass = CurrentClass->GetSuperClass();
		if (SuperClass == nullptr)
		{
			break;
		}

		if (SuperClass->IsNative())
		{
			return SuperClass;
		}

		CurrentClass = SuperClass;
	}

	return nullptr;
}
