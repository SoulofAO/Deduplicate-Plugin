// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeduplicationWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SComboBox.h"
#include "EditorStyleSet.h"
#include "ContentBrowserModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Async/Async.h"
#include "Misc/Optional.h"
#include "UObject/ObjectRedirector.h"
#include "DeduplicationFunctionLibrary.h"

/*
#pragma push_macro("private")
#define private public
#include "M:/UE/UE_5.5/UE_5.5/Engine/Source/Developer/AssetTools/Private/AssetTools.h"
#include "M:\UE\UE_5.5\UE_5.5\Engine\Source\Developer\AssetTools\Private\AssetFixUpRedirectors.h"
#include "M:\UE\UE_5.5\UE_5.5\Engine\Source\Developer\AssetTools\Private\AssetFixUpRedirectors.cpp"
#pragma pop_macro("private")*/


void SDeduplicationWidget::Construct(const FArguments& InArgs)
{
	DeduplicationManager = InArgs._DeduplicationManager;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bShowPropertyMatrixButton = false;
	DetailsView = PropertyModule.CreateDetailView(DetailsArgs);
	DetailsView->SetObject(DeduplicationManager);

	ChildSlot
		[
			SNew(SSplitter)
				+ SSplitter::Slot()
				.Value(0.35f)
				[
					SNew(SBorder)
						.Padding(8)
						[
							SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.FillHeight(1.0)
								.Padding(0, 8, 0, 0)
								[
									SNew(SSplitter)
												.Orientation(EOrientation::Orient_Vertical)
												+ SSplitter::Slot()
										.MinSize(200.0f)
												.Value(0.5f)
												[
														SNew(SScrollBox)
															+ SScrollBox::Slot()
															[
																DetailsView->AsShared()
															]
												]

												+ SSplitter::Slot()
												.Value(0.35f)
												.MinSize(220.0f)
												[
															SNew(SVerticalBox)

																+ SVerticalBox::Slot()
																.AutoHeight()
																.Padding(5, 2, 5, 2)
																[
																	SAssignNew(AnalyzeButton, SButton)
																		.Text(FText::FromString(TEXT("Analize")))
																		.OnClicked(this, &SDeduplicationWidget::OnAnalyzeClicked)
																		.IsEnabled_Lambda([this]() { return !DeduplicationManager->bIsAnalyze; })

																]
																+ SVerticalBox::Slot()
																.AutoHeight()
																.Padding(5, 2, 5, 2)
																[
																	SAssignNew(AnalyzeInFolderButton, SButton)
																		.Text(FText::FromString(TEXT("AnalyzeInSelectFolder")))
																		.OnClicked(this, &SDeduplicationWidget::OnAnalyzeClickedInSelectedFolder)
																		.IsEnabled_Lambda([this]() { return !DeduplicationManager->bIsAnalyze; })
																]
																+ SVerticalBox::Slot()
																.AutoHeight()
																.Padding(5, 2, 5, 2)
																[
																	SAssignNew(FixRedirectAfterUniteCheckBox, SCheckBox)
																		.IsChecked(ECheckBoxState::Unchecked)
																		.Content()
																		[
																			SNew(STextBlock)

																				.Text(FText::FromString(TEXT("FixRedirectAfterUnite")))
																		]
																]
																+ SVerticalBox::Slot()
																.AutoHeight()
																.Padding(5, 2, 5, 2)
																[
																	SAssignNew(MergeButton, SButton)
																		.Text(FText::FromString(TEXT("Unite")))
																		.OnClicked(this, &SDeduplicationWidget::OnMergeClicked)
																		.IsEnabled(false)
																]
																+ SVerticalBox::Slot()
																.AutoHeight()
																.Padding(0, 8, 0, 0)
																[
																	SNew(STextBlock)
																		.Text_Lambda([this]() {
																		if (DeduplicationManager->bIsAnalyze)
																		{
																			return FText::FromString("Analyzing");
																		}
																		else if (DeduplicationManager->bCompleteAnalyze)
																		{
																			return FText::FromString("Complete");
																		}
																		else
																		{
																			return FText::GetEmpty();
																		}
																			})
																]
																+ SVerticalBox::Slot()
																.AutoHeight()
																.Padding(0, 8, 0, 0)
																[
																	SNew(SHorizontalBox)

																		+ SHorizontalBox::Slot()
																		.FillWidth(1.0f)
																		.VAlign(VAlign_Center)
																		[
																			SAssignNew(ProgressBar, SProgressBar)
																				.ToolTipText_Lambda([this]() {
																				int32 Pct = FMath::RoundToInt(DeduplicationManager->ProgressValue * 100.0);
																				return FText::FromString(FString::Printf(TEXT("%d%%"), Pct));
																					})
																		]

																		+ SHorizontalBox::Slot()
																		.AutoWidth()
																		.VAlign(VAlign_Center)
																		.Padding(6, 0, 0, 0)
																		[
																			SNew(STextBlock)
																				.Text_Lambda([this]() {
																				int32 Pct = FMath::RoundToInt(DeduplicationManager->ProgressValue * 100.0);
																				return FText::FromString(FString::Printf(TEXT("%d%%"), Pct));
																					})
																		]
																]
																+ SVerticalBox::Slot()
																.FillHeight(1.0)
																.Padding(0, 8, 0, 0)
																[
																	SNew(SBorder)
																		.Padding(6)
																		[
																			SAssignNew(ResultsTextBlock, STextBlock)
																				.Text(FText::FromString(ResultsString))
																				.AutoWrapText(true)
																		]
																]
													
												]
										]
								]
						]
					
					+ SSplitter::Slot()
					.Value(0.65f)
					[
						SNew(SScrollBox)

							+ SScrollBox::Slot()
							[
								SAssignNew(ContentFolder, SContentFolderSimple)
									.OnContentItemSelectedDelegate(this, &SDeduplicationWidget::HandleContentItemSelected)
							]
					]
				];
										

	RebuildAnalyze();
}

void SDeduplicationWidget::UpdateDeduplicationProgress(double Progress)
{
	float DeduplicationProgress = FMath::Clamp(Progress, 0.0, 1.0);
	if (ProgressBar.IsValid())
	{
		ProgressBar->SetPercent(DeduplicationProgress);
	}
}


FReply SDeduplicationWidget::OnAnalyzeClicked()
{
	if (!DeduplicationManager)
	{
		return FReply::Handled();
	}

	TArray<FString> RootFolderPaths;
	ContentFolder->GetIncludeRootPaths(RootFolderPaths);

	StartAnalyze(RootFolderPaths);

	return FReply::Handled();
}

FReply SDeduplicationWidget::OnAnalyzeClickedInSelectedFolder()
{
	TSharedPtr<FContentItem> SelectedItem = ContentFolder->GetSelectedItem();
	if (SelectedItem->bIsFolder)
	{
		StartAnalyze({ SelectedItem->Path});
	}
	return FReply::Handled();
}

void SDeduplicationWidget::StartAnalyze(TArray<FString> RootFolderPaths)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDatas;
	for (const FString& RootPath : RootFolderPaths)
	{
		TArray<FAssetData> NewAssetDatas;
		AssetRegistry.GetAssetsByPath(FName(*RootPath), NewAssetDatas, /*bRecursive=*/true);
		AssetDatas.Append(NewAssetDatas);
	}
	DeduplicationManager->OnDeduplicationAnalyzeCompleted.AddRaw(this, &SDeduplicationWidget::OnDeduplicationAnalyzeFinished);
	DeduplicationManager->StartAnalyzeAssetsAsync(AssetDatas);
	//AnalyzeInFolderButton->SetEnabled(!DeduplicationManager->bIsAnalyze);
	//AnalyzeButton->SetEnabled(!DeduplicationManager->bIsAnalyze);
}

void SDeduplicationWidget::OnFirstMergePriorityChanged(ECheckBoxState NewState)
{

}

void SDeduplicationWidget::Merge(FAssetData AssetData, TArray<FDeduplicationAssetStruct> DuplicateAssets)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	ContentFolder->SetAssetDataColor(AssetData, FLinearColor::Green);

	for (const FDeduplicationAssetStruct& DeduplicationAssetStruct : DuplicateAssets)
	{
		ContentFolder->ClearAssetDataColor(DeduplicationAssetStruct.DuplicateAsset);
	}

	TSet<FString> MergedObjectPaths;
	MergedObjectPaths.Add(AssetData.GetObjectPathString());
	for (const FDeduplicationAssetStruct& DeduplicationAssetStruct : DuplicateAssets)
	{
		MergedObjectPaths.Add(DeduplicationAssetStruct.DuplicateAsset.GetObjectPathString());
	}

	TSet<FName> PathsToCheck;
	PathsToCheck.Add(AssetData.PackagePath);
	for (const FDeduplicationAssetStruct& DeduplicationAssetStruct : DuplicateAssets)
	{
		PathsToCheck.Add(DeduplicationAssetStruct.DuplicateAsset.PackagePath);
	}

	DeduplicationManager->Clasters;

	TArray<UObject*> AssetsToConsolidate;
	for (const FDeduplicationAssetStruct& DeduplicationAssetStruct : DuplicateAssets)
	{
		UObject* AssetObject = DeduplicationAssetStruct.DuplicateAsset.GetAsset();
		if (AssetObject && AssetObject != AssetData.GetAsset())
		{
			AssetsToConsolidate.Add(AssetObject);
		}
	}

	if (AssetsToConsolidate.Num() > 0 && AssetData.IsValid())
	{
		EditorAssetSubsystem->ConsolidateAssets(AssetData.GetAsset(), AssetsToConsolidate);
	}

	if (FixRedirectAfterUniteCheckBox->GetCheckedState() == ECheckBoxState::Checked)
	{
		TArray<TWeakObjectPtr<UObjectRedirector>> RedirectorObjectPaths;
		for (const FDeduplicationAssetStruct& Asset : DuplicateAssets)
		{
			RedirectorObjectPaths.Add(Cast<UObjectRedirector>(Asset.DuplicateAsset.GetAsset()));
		}

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		UDeduplicationFunctionLibrary::ExecuteFixUp_NoUI(RedirectorObjectPaths);
	}

	if (!DeduplicationManager)
	{
		return;
	}

	TSet<FString> ExistingClusterFolderPaths;
	for (const FDuplicateCluster& Cluster : DeduplicationManager->Clasters)
	{
		if (Cluster.AssetData.IsValid())
		{
			const FString ClusterAssetObjectPath = Cluster.AssetData.GetObjectPathString();
			if (!MergedObjectPaths.Contains(ClusterAssetObjectPath))
			{
				ExistingClusterFolderPaths.Add(Cluster.AssetData.PackagePath.ToString());
			}
		}

		for (const FDeduplicationAssetStruct& ClusterDup : Cluster.DuplicateAssets)
		{
			if (ClusterDup.DuplicateAsset.IsValid())
			{
				const FString ClusterDupObjectPath = ClusterDup.DuplicateAsset.GetObjectPathString();
				if (!MergedObjectPaths.Contains(ClusterDupObjectPath))
				{
					ExistingClusterFolderPaths.Add(ClusterDup.DuplicateAsset.PackagePath.ToString());
				}
			}
		}
	}

	TSet<FName> PathsThatAreEmpty;
	for (const FName& Path : PathsToCheck)
	{
		bool bHasOtherInThisPath = false;
		const FString PathString = Path.ToString();

		for (const FString& ExistingPath : ExistingClusterFolderPaths)
		{
			if (ExistingPath.Equals(PathString, ESearchCase::CaseSensitive) || ExistingPath.StartsWith(PathString + TEXT("/")))
			{
				bHasOtherInThisPath = true;
				break;
			}
		}

		if (!bHasOtherInThisPath)
		{
			PathsThatAreEmpty.Add(Path);
		}
	}

	auto GetParentPath = [&](const FString& InPath) -> FString
		{
			int32 LastSlash = INDEX_NONE;
			if (InPath.FindLastChar('/', LastSlash) && LastSlash > 0)
			{
				return InPath.Left(LastSlash);
			}
			return FString();
		};

	for (const FName& EmptyPathName : PathsThatAreEmpty)
	{
		ContentFolder->ClearPathColor(EmptyPathName.ToString());
	}

	TSet<FString> ClearedPathStrings;
	for (const FName& EmptyPathName : PathsThatAreEmpty)
	{
		ClearedPathStrings.Add(EmptyPathName.ToString());
	}

	TArray<FString> ToProcess = ClearedPathStrings.Array();

	while (ToProcess.Num() > 0)
	{
		const FString CurrentPath = ToProcess.Pop();
		FString Parent = GetParentPath(CurrentPath);

		while (!Parent.IsEmpty())
		{
			if (ClearedPathStrings.Contains(Parent))
			{
				Parent = GetParentPath(Parent);
				continue;
			}

			bool bParentHasOther = false;
			for (const FString& ExistingPath : ExistingClusterFolderPaths)
			{
				if (ExistingPath.Equals(Parent, ESearchCase::CaseSensitive) || ExistingPath.StartsWith(Parent + TEXT("/")))
				{
					bool bExistingIsInCleared = false;
					for (const FString& ClearedPath : ClearedPathStrings)
					{
						if (ExistingPath.Equals(ClearedPath, ESearchCase::CaseSensitive) || ExistingPath.StartsWith(ClearedPath + TEXT("/")))
						{
							bExistingIsInCleared = true;
							break;
						}
					}

					if (!bExistingIsInCleared)
					{
						bParentHasOther = true;
						break;
					}
				}
			}

			if (!bParentHasOther)
			{
				ContentFolder->ClearPathColor(Parent);
				ClearedPathStrings.Add(Parent);
				ToProcess.Add(Parent);
				Parent = GetParentPath(Parent);
			}
			else
			{
				break;
			}
		}
	}
}


FReply SDeduplicationWidget::OnMergeClicked()
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	 
	TSharedPtr<FContentItem> SelectedItem = ContentFolder->GetSelectedItem();
	if (SelectedItem->bIsFolder)
	{
		FString SelectedFolderPath = SelectedItem->Path;
		TArray<FDuplicateCluster> ClastersInFolder = DeduplicationManager->FindMostPriorityDuplicateClusterByPath(SelectedFolderPath);

		while (ClastersInFolder.IsValidIndex(0))
		{
			FDuplicateCluster Cluster = ClastersInFolder[0];
			Merge(Cluster.AssetData, Cluster.DuplicateAssets);
			ClastersInFolder.RemoveAt(0);
		}
	}
	else
	{
		FDuplicateCluster* Cluster = DeduplicationManager->Clasters.FindByKey(SelectedItem->Data);
		if (Cluster)
		{
			TArray<UObject*> AssetsToConsolidate;
			for (FDeduplicationAssetStruct DeduplicationAssetStruct : Cluster->DuplicateAssets)
			{
				AssetsToConsolidate.Add(DeduplicationAssetStruct.DuplicateAsset.GetAsset());
			}
			EditorAssetSubsystem->ConsolidateAssets(SelectedItem->Data.GetAsset(), AssetsToConsolidate);
		}
	}

	return FReply::Handled();
}

void SDeduplicationWidget::OnDeduplicationAnalyzeFinished(const TArray<FDuplicateCluster>& ResultClusters)
{
	RebuildAnalyze();
}

void SDeduplicationWidget::RebuildAnalyze()
{
	HandleContentItemSelected(ContentFolder->GetSelectedItem());
	AnalyzeButton->SetEnabled(true);

	ContentFolder->ClearAllPathColor();
	if (DeduplicationManager->bCompleteAnalyze)
	{
		for (FDuplicateCluster DuplicateCluster : DeduplicationManager->Clasters)
		{
			TSharedPtr<FContentItem> AssetItem = ContentFolder->FindContentItemByAssetItem(DuplicateCluster.AssetData);

			while(AssetItem)
			{
				ContentFolder->SetPathColor(AssetItem->Path, FLinearColor::Red);
				AssetItem = AssetItem->Parent;
			}
		}
		if (ContentFolder->TreeView.IsValid())
		{
			ContentFolder->TreeView->RequestTreeRefresh();
		}
	}

	ProgressBar->SetPercent(DeduplicationManager->ProgressValue);
}

void SDeduplicationWidget::RefreshResultsText()
{
}

void SDeduplicationWidget::HandleContentItemSelected(TSharedPtr<FContentItem> SelectedItem)
{
	MergeButton->SetEnabled(false);
	if (SelectedItem)
	{
		if (!DeduplicationManager->bIsAnalyze)
		{
			if (DeduplicationManager->bCompleteAnalyze)
			{
				if (SelectedItem->bIsFolder)
				{
					TArray<FDuplicateCluster> DuplicateClusters = DeduplicationManager->GetClastersByFolder(SelectedItem->Path);
					ResultsTextBlock->SetText(FText::FromString(FString::Format(TEXT("Count Duplicates In Folder: {0}"), { DuplicateClusters.Num() })));
					if (DuplicateClusters.Num() > 0)
					{
						MergeButton->SetEnabled(true);
					}
				}
				else
				{
					FAssetData Data = SelectedItem->Data;
					FDuplicateCluster* DuplicateCluster = DeduplicationManager->Clasters.FindByKey(Data);
					if (DuplicateCluster)
					{
						MergeButton->SetEnabled(true);
						FString Text = FString::Format(TEXT("Main asset: {0}\n"), { Data.AssetName.ToString() });
						Text += TEXT("Duplicates:\n");

						for (const FDeduplicationAssetStruct& DuplicateAssetData : DuplicateCluster->DuplicateAssets)
						{
							Text += FString::Format(TEXT(" - Name: {0} - Path: {1} - Score: {2}\n"), { DuplicateAssetData.DuplicateAsset.AssetName.ToString(), DuplicateAssetData.DuplicateAsset.PackagePath.ToString(), DuplicateAssetData.DeduplicationAssetScore });
						}

						Text += FString::Format(TEXT("\nSimilarity score: {0}"), { FString::SanitizeFloat(DuplicateCluster->ClusterScore, 2) });

						ResultsTextBlock->SetText(FText::FromString(Text));
					}
					else
					{
						ResultsTextBlock->SetText(FText::FromString(TEXT("No duplicates found for the selected asset.")));
					}
				}
			}
			else
			{
				ResultsTextBlock->SetText(FText::FromString(TEXT("Analysis has not been completed yet.")));
			}
			if (SelectedItem->bIsFolder)
			{
				AnalyzeInFolderButton->SetEnabled(true);
			}
		}
		else
		{
			AnalyzeInFolderButton->SetEnabled(false);
			AnalyzeButton->SetEnabled(false);
		}
	}
}
