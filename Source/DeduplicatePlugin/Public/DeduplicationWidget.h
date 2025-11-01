// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "DeduplicationManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/STreeView.h"
#include "Engine/Texture2D.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Styling/CoreStyle.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "Components/Widget.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "DeduplicationContentWidget.h"

class SDeduplicationWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDeduplicationWidget)
		: _DeduplicationManager(nullptr)
		{
		}
		SLATE_ARGUMENT(UDeduplicationManager*, DeduplicationManager)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void Destruct();

	static TSharedRef<SWidget> CreateWidget(UDeduplicationManager* NewDeduplicationManager)
	{
		return SNew(SDeduplicationWidget)
			.DeduplicationManager(NewDeduplicationManager);
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SContentFolderSimple> ContentFolder;
	TSharedPtr<STextBlock> ResultsTextBlock;
	TSharedPtr<SButton> MergeButton;
	TSharedPtr<SCheckBox> FixRedirectAfterUniteCheckBox;
	FString ResultsString;
	UDeduplicationManager* DeduplicationManager;
	TSharedPtr<SProgressBar> ProgressBar;
	TSharedPtr<SButton> AnalyzeInFolderButton;
	TSharedPtr<SButton> AnalyzeButton;

	FReply OnAnalyzeClicked();
	FReply OnAnalyzeClickedInSelectedFolder();
	void StartAnalyze(TArray<FString> RootFolderPaths);
	void Merge(FAssetData AssetData, TArray<FDeduplicationAssetStruct> DuplicateAssets);
	FReply OnMergeClicked();
	void OnDeduplicationAnalyzeFinished(const TArray<FDuplicateCluster>& ResultClusters);
	void RebuildAnalyze();
	void RefreshResultsText();
	void HandleContentItemSelected(TSharedPtr<FContentItem> SelectedItem);

	float GetConfidenceThreshold();
	void OnConfidenceThresholdChanged(float NewValue);
	float ConfidenceThresholdDelaySeconds = 0.25f;
	FTSTicker::FDelegateHandle ConfidenceThresholdTickerHandle;

	float GetGroupConfidenceThreshold();
	void OnGroupConfidenceThresholdChanged(float NewValue);
	float GroupConfidenceThresholdDelaySeconds = 0.25f;
	FTSTicker::FDelegateHandle GroupConfidenceThresholdTickerHandle;


	bool HandleRebuildAnalyze(float DeltaTime);
};