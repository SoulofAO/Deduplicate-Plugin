/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#include "DeduplicationFunctionLibrary.h"
#include "CoreMinimal.h"
#include "Templates/UnrealTemplate.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "Misc/PackageName.h"
#include "PackageTools.h"
#include "Internationalization/Text.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#include "SourceControlHelpers.h"
#include "HAL/FileManager.h"
#include "FileHelpers.h"
#include "ObjectTools.h"
#include "CollectionManagerModule.h"
#include "ICollectionManager.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Misc/Paths.h"
#include "Containers/Map.h"
#include "AutoReimport/AssetSourceFilenameCache.h"
#include "UObject/MetaData.h"

#if WITH_EDITOR
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#endif



#define LOCTEXT_NAMESPACE "AssetFixUpRedirectors"


struct FRedirectorRefs
{
	TStrongObjectPtr<const UObjectRedirector> Redirector;
	FName RedirectorPackageName;
	TArray<FName> ReferencingPackageNames;

	// Referencing packages which could not be updated to remove references to this redirector
	TArray<FName> LockedReferencerPackageNames;
	TArray<FName> FailedReferencerPackageNames;

	// Possible failure reason unrelated to above lists of package names.
	// If this is not empty we may not delete the redirector - we may still be able to remove some references to it.
	TArray<FText> OtherFailures;

	bool bSCCError = false; // Failure to check the redirector itself out of source control etc

	explicit FRedirectorRefs(FName PackageName)
		: Redirector(nullptr)
		, RedirectorPackageName(PackageName)
	{
	}

	explicit FRedirectorRefs(const UObjectRedirector* InRedirector)
		: Redirector(InRedirector)
		, RedirectorPackageName(InRedirector->GetOutermost()->GetFName())
	{
	}
};

void UDeduplicationFunctionLibrary::ExecuteFixUp_NoUI(TArray<TWeakObjectPtr<UObjectRedirector>> Objects)
{
	TArray<FRedirectorRefs> RedirectorRefsList;
	for (TWeakObjectPtr<UObjectRedirector> Object : Objects)
	{
		if (UObjectRedirector* ObjectRedirector = Object.Get())
		{
			RedirectorRefsList.Emplace(ObjectRedirector);
		}
	}

	if (RedirectorRefsList.Num() == 0)
	{
		return;
	}

	bool bMayDeleteRedirectors = true;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	bool bAnyRefs = false;
	for (FRedirectorRefs& RedirectorRefs : RedirectorRefsList)
	{
		AssetRegistryModule.Get().GetReferencers(RedirectorRefs.RedirectorPackageName, RedirectorRefs.ReferencingPackageNames);
		bAnyRefs = bAnyRefs || RedirectorRefs.ReferencingPackageNames.Num() != 0;
	}

	if (!bAnyRefs && !bMayDeleteRedirectors)
	{
		return;
	}

	TMultiMap<FName, FRedirectorRefs*> ReferencingAssetToRedirector;
	for (FRedirectorRefs& RedirectorRefs : RedirectorRefsList)
	{
		for (FName PackageName : RedirectorRefs.ReferencingPackageNames)
		{
			ReferencingAssetToRedirector.Add(PackageName, &RedirectorRefs);
		}
	}

	if (bMayDeleteRedirectors && ISourceControlModule::Get().IsEnabled())
	{
		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
		SourceControlProvider.Login();
		if (!SourceControlProvider.IsAvailable())
		{
			bMayDeleteRedirectors = false;
		}
		else
		{
			TArray<UPackage*> PackagesToAddToSCCUpdate;
			for (const FRedirectorRefs& RedirectorRefs : RedirectorRefsList)
			{
				PackagesToAddToSCCUpdate.Add(RedirectorRefs.Redirector->GetOutermost());
			}
			SourceControlProvider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), PackagesToAddToSCCUpdate);
		}
	}

	TSet<UPackage*> ReferencingPackagesToSave;
	TSet<UPackage*> LoadedPackages;
	bool bCancel = false;
	{
		FScopedSlowTask SlowTask(static_cast<float>(RedirectorRefsList.Num()), LOCTEXT("LoadingReferencingPackages", "Loading Referencing Packages..."));
		for (FRedirectorRefs& RedirectorRefs : RedirectorRefsList)
		{
			SlowTask.EnterProgressFrame(1);
			if (SlowTask.ShouldCancel())
			{
				bCancel = true;
				break;
			}

			if (bMayDeleteRedirectors && ISourceControlModule::Get().IsEnabled())
			{
				ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
				FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(RedirectorRefs.Redirector->GetOutermost(), EStateCacheUsage::Use);
				const bool bValidSCCState = !SourceControlState.IsValid() || SourceControlState->IsAdded() || SourceControlState->IsCheckedOut() || SourceControlState->CanCheckout() || !SourceControlState->IsSourceControlled() || SourceControlState->IsIgnored();

				if (!bValidSCCState)
				{
					RedirectorRefs.bSCCError = true;
				}
			}

			for (FName ReferencingPackageName : RedirectorRefs.ReferencingPackageNames)
			{
				FNameBuilder PackageName{ ReferencingPackageName };

				UPackage* Package = FindPackage(nullptr, *PackageName);
				if (!Package)
				{
					Package = LoadPackage(nullptr, *PackageName, LOAD_None);
					if (Package)
					{
						LoadedPackages.Add(Package);
					}
				}

				if (Package)
				{
					if (Package->HasAnyPackageFlags(PKG_CompiledIn))
					{
						RedirectorRefs.OtherFailures.Add(FText::Format(LOCTEXT("RedirectorFixupFailed_CodeReference", "Redirector is referenced by code. Package: {0}"), FText::FromName(ReferencingPackageName)));
					}
					else
					{
						ReferencingPackagesToSave.Add(Package);
					}
				}
			}
		}
	}

	ON_SCOPE_EXIT{
		if (!LoadedPackages.IsEmpty())
		{
			FText ErrorMessage;
			UPackageTools::UnloadPackages(LoadedPackages.Array(), ErrorMessage, true);
		}
	};

	if (bCancel)
	{
		return;
	}

	TArray<TStrongObjectPtr<UObject>> RootedObjects;
	for (UPackage* Package : ReferencingPackagesToSave)
	{
		ForEachObjectWithPackage(Package, [&RootedObjects](UObject* Object)
			{
				RootedObjects.Emplace(Object);
				return true;
			}, false, RF_Standalone, EInternalObjectFlags::RootSet);
	}

	TSet<FName> WorldAssetsNeedingLoadersReset;
	for (UPackage* Package : ReferencingPackagesToSave)
	{
		TArray<FAssetData> ReferencingPackageAssets;
		if (AssetRegistryModule.Get().GetAssetsByPackageName(Package->GetFName(), ReferencingPackageAssets, true))
		{
			for (const FAssetData& Asset : ReferencingPackageAssets)
			{
				if (!Asset.GetOptionalOuterPathName().IsNone())
				{
					WorldAssetsNeedingLoadersReset.Add(FSoftObjectPath(Asset.GetOptionalOuterPathName().ToString()).GetLongPackageFName());
				}
			}
		}
	}
	for (const FName& WorldAsset : WorldAssetsNeedingLoadersReset)
	{
		ULevelInstanceSubsystem::ResetLoadersForWorldAsset(*WorldAsset.ToString());
	}

	bool bUserAcceptedCheckout = true;
	if (ISourceControlModule::Get().IsEnabled() && ReferencingPackagesToSave.Num() > 0)
	{
		TArray<UPackage*> PackagesCheckedOutOrMadeWritable;
		TArray<UPackage*> PackagesNotNeedingCheckout;
		const bool bErrorIfAlreadyCheckedOut = false;
		const bool bConfirmPackageBranchCheckOutStatus = false;
		FEditorFileUtils::CheckoutPackages(ReferencingPackagesToSave.Array(), &PackagesCheckedOutOrMadeWritable, bErrorIfAlreadyCheckedOut, bConfirmPackageBranchCheckOutStatus);

		TSet<UPackage*> PackagesThatCouldNotBeCheckedOut = ReferencingPackagesToSave;
		for (UPackage* Package : PackagesCheckedOutOrMadeWritable)
		{
			PackagesThatCouldNotBeCheckedOut.Remove(Package);
		}
		for (UPackage* Package : PackagesNotNeedingCheckout)
		{
			PackagesThatCouldNotBeCheckedOut.Remove(Package);
		}

		for (UPackage* Package : PackagesThatCouldNotBeCheckedOut)
		{
			FName PackageName = Package->GetFName();
			for (auto It = ReferencingAssetToRedirector.CreateKeyIterator(PackageName); It; ++It)
			{
				It.Value()->LockedReferencerPackageNames.Add(Package->GetFName());
			}
			ReferencingPackagesToSave.Remove(Package);
		}
	}

	if (bUserAcceptedCheckout)
	{
		for (auto It = ReferencingPackagesToSave.CreateIterator(); It; ++It)
		{
			UPackage* Package = *It;
			if (!ensure(Package))
			{
				It.RemoveCurrent();
				continue;
			}

			FString Filename;
			if (FPackageName::DoesPackageExist(Package->GetName(), &Filename)
				&& IFileManager::Get().IsReadOnly(*Filename))
			{
				FName PackageName = Package->GetFName();
				for (auto RedirectorIt = ReferencingAssetToRedirector.CreateKeyIterator(PackageName); RedirectorIt; ++RedirectorIt)
				{
					RedirectorIt.Value()->LockedReferencerPackageNames.Add(Package->GetFName());
				}
				It.RemoveCurrent();
			}
		}

		{
			TSet<UPackage*> PackagesToCheck = ReferencingPackagesToSave;
			TArray<UPackage*> Tmp;
			FEditorFileUtils::GetDirtyWorldPackages(Tmp);
			FEditorFileUtils::GetDirtyContentPackages(Tmp);
			PackagesToCheck.Append(Tmp);

			TMap<FSoftObjectPath, FSoftObjectPath> RedirectorMap;
			for (const FRedirectorRefs& RedirectorRef : RedirectorRefsList)
			{
				const UObjectRedirector* Redirector = RedirectorRef.Redirector.Get();
				FSoftObjectPath OldPath = FSoftObjectPath(Redirector);
				FSoftObjectPath NewPath = FSoftObjectPath(Redirector->DestinationObject);

				RedirectorMap.Add(OldPath, NewPath);
				if (UBlueprint* Blueprint = Cast<UBlueprint>(Redirector->DestinationObject))
				{
					RedirectorMap.Add(FString::Printf(TEXT("%s_C"), *OldPath.ToString()), FString::Printf(TEXT("%s_C"), *NewPath.ToString()));
					RedirectorMap.Add(FString::Printf(TEXT("%s.Default__%s_C"), *OldPath.GetLongPackageName(), *OldPath.GetAssetName()), FString::Printf(TEXT("%s.Default__%s_C"), *NewPath.GetLongPackageName(), *NewPath.GetAssetName()));
				}
			}

			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			AssetTools.RenameReferencingSoftObjectPaths(PackagesToCheck.Array(), RedirectorMap);
		}

		TArray<UPackage*> FailedToSave;
		if (ReferencingPackagesToSave.Num() > 0)
		{
			const TArray<FString> Filenames = USourceControlHelpers::PackageFilenames(ReferencingPackagesToSave.Array());
			const bool bCheckDirty = false;
			const bool bPromptToSave = false;
			FEditorFileUtils::PromptForCheckoutAndSave(ReferencingPackagesToSave.Array(), bCheckDirty, bPromptToSave, &FailedToSave);
			for (UPackage* Package : FailedToSave)
			{
				FName PackageName = Package->GetFName();
				for (auto It = ReferencingAssetToRedirector.CreateKeyIterator(PackageName); It; ++It)
				{
					It.Value()->FailedReferencerPackageNames.Add(Package->GetFName());
				}
			}

			ISourceControlModule::Get().QueueStatusUpdate(Filenames);
		}

		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		for (FRedirectorRefs& RedirectorRefs : RedirectorRefsList)
		{
			for (const UObjectRedirector* Redirector = RedirectorRefs.Redirector.Get(); Redirector; Redirector = Cast<UObjectRedirector>(Redirector->DestinationObject))
			{
				const FSoftObjectPath RedirectorObjectPath = FSoftObjectPath(Redirector);
				FText Error;
				if (!CollectionManagerModule.Get().HandleRedirectorDeleted(RedirectorObjectPath, &Error))
				{
					RedirectorRefs.OtherFailures.Add(FText::Format(LOCTEXT("RedirectorFixupFailed_CollectionsFailedToSave", "Referencing collection(s) failed to save: {0}"), Error));
				}
			}
		}

		{
			TArray<FString> AssetPaths;
			for (const FRedirectorRefs& Redirector : RedirectorRefsList)
			{
				AssetPaths.AddUnique(FPackageName::GetLongPackagePath(Redirector.RedirectorPackageName.ToString()) / TEXT(""));
				for (const auto& Referencer : Redirector.ReferencingPackageNames)
				{
					AssetPaths.AddUnique(FPackageName::GetLongPackagePath(Referencer.ToString()) / TEXT(""));
				}
			}
			AssetRegistryModule.Get().ScanPathsSynchronous(AssetPaths, true);
		}

		TArray<UObject*> ObjectsToDelete;
		for (FRedirectorRefs& RedirectorRefs : RedirectorRefsList)
		{
			if (RedirectorRefs.OtherFailures.IsEmpty()
				&& RedirectorRefs.LockedReferencerPackageNames.IsEmpty()
				&& RedirectorRefs.FailedReferencerPackageNames.IsEmpty())
			{
				ensure(RedirectorRefs.Redirector);
				UPackage* RedirectorPackage = RedirectorRefs.Redirector->GetOutermost();
				bool bContainsAtLeastOneOtherAsset = false;
				ForEachObjectWithOuter(RedirectorPackage, [&ObjectsToDelete, &bContainsAtLeastOneOtherAsset](UObject* Obj)
					{
						if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(Obj))
						{
							Redirector->RemoveFromRoot();
							ObjectsToDelete.Add(Redirector);
						}
						else
						{
							bContainsAtLeastOneOtherAsset = true;
						}
						return true;
					});

				if (!bContainsAtLeastOneOtherAsset)
				{
					RedirectorPackage->RemoveFromRoot();
					ObjectsToDelete.Add(RedirectorPackage);
				}
			}
		}

		RedirectorRefsList.Empty();

		if (ObjectsToDelete.Num() > 0)
		{
			ObjectTools::DeleteObjects(ObjectsToDelete, false);
		}
	}
}

TArray<FAssetData> UDeduplicationFunctionLibrary::FilterRedirects(const TArray<FAssetData>& Assets)
{
	TArray<FAssetData> Filtered;
	Filtered.Reserve(Assets.Num());
	for (const FAssetData& Asset : Assets)
	{
		if (!Asset.IsRedirector())
		{
			Filtered.Add(Asset);
		}
	}
	return Filtered;
}

int32 UDeduplicationFunctionLibrary::FindCommonSubstrings(const TArray<uint8>& Data1, const TArray<uint8>& Data2, int32 MinLength)
{
	int32 CommonCount = 0;

	for (int32 i = 0; i <= Data1.Num() - MinLength; i++)
	{
		for (int32 j = 0; j <= Data2.Num() - MinLength; j++)
		{
			bool bMatch = true;
			for (int32 k = 0; k < MinLength; k++)
			{
				if (Data1[i + k] != Data2[j + k])
				{
					bMatch = false;
					break;
				}
			}

			if (bMatch)
			{
				CommonCount++;
				break;
			}
		}
	}

	return CommonCount;
}

int UDeduplicationFunctionLibrary::ComputeLevenshteinDistance(const FString& Str1, const FString& Str2)
{
	const int Len1 = Str1.Len();
	const int Len2 = Str2.Len();

	TArray<TArray<int>> Matrix;
	Matrix.SetNum(Len1 + 1);
	for (int i = 0; i <= Len1; ++i)
	{
		Matrix[i].SetNum(Len2 + 1);
	}

	for (int i = 0; i <= Len1; ++i)
	{
		Matrix[i][0] = i;
	}
	for (int j = 0; j <= Len2; ++j)
	{
		Matrix[0][j] = j;
	}

	for (int i = 1; i <= Len1; ++i)
	{
		for (int j = 1; j <= Len2; ++j)
		{
			int Cost = (Str1[i - 1] == Str2[j - 1]) ? 0 : 1;
			Matrix[i][j] = FMath::Min3(
				Matrix[i - 1][j] + 1,
				Matrix[i][j - 1] + 1,
				Matrix[i - 1][j - 1] + Cost
			);
		}
	}

	return Matrix[Len1][Len2];
}

#undef LOCTEXT_NAMESPACE
