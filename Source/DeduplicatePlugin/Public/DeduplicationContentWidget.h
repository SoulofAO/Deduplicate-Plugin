/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
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
#include "DeduplicationContentWidget.generated.h"

struct FContentItem : public TSharedFromThis<FContentItem>
{
	FString Name;
	FString Path;
	FAssetData Data;
	bool bIsFolder = false;
	bool bSetupColor = false;
	FSlateColor Color;
	TArray<TSharedPtr<FContentItem>> Children;
	TSharedPtr<FContentItem> Parent;

	static TSharedPtr<FContentItem> CreateFolder(const FString& InName, const FString& InPath = TEXT(""));

	static TSharedPtr<FContentItem> CreateAsset(FAssetData InData);
};

DECLARE_DELEGATE_OneParam(FOnContentItemSelectedDelegate, TSharedPtr<FContentItem>);

class SContentFolderSimple : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SContentFolderSimple) {}
		SLATE_EVENT(FOnContentItemSelectedDelegate, OnContentItemSelectedDelegate)
	SLATE_END_ARGS()

	void Destruct();

	void Construct(const FArguments& InArgs);

	TArray<TSharedPtr<FContentItem>> RootItems;
	TSharedPtr<STreeView<TSharedPtr<FContentItem>>> TreeView;
	FOnContentItemSelectedDelegate OnContentItemSelectedDelegate;
	TMap<FString, FSlateColor> ItemColors;

	bool bIncludeContent = true;
	bool bIncludePlugins = false;
	bool bIncludeEngine = false;

	void OnAssetAdded(const FAssetData& AssetData);
	void AddAsset(const FAssetData& AssetData);
	void OnAssetRemoved(const FAssetData& AssetData);
	void RemoveAsset(const FAssetData& AssetData);
	void OnPathAdded(const FString& Path);
	void AddPath(FString Path);
	void OnPathRemoved(const FString& Path);
	void RemovePath(FString Path);

	FDelegateHandle AssetAddedHandle;
	FDelegateHandle AssetRemovedHandle;
	FDelegateHandle PathAddedHandle;
	FDelegateHandle PathRemovedHandle;

	ECheckBoxState GetContentCheckState() const;
	ECheckBoxState GetPluginsCheckState() const;
	ECheckBoxState GetEngineCheckState() const;

	void OnContentCheckChanged(ECheckBoxState NewState);
	void OnPluginsCheckChanged(ECheckBoxState NewState);
	void OnEngineCheckChanged(ECheckBoxState NewState);

	TSharedPtr<FContentItem> GetSelectedItem();

	void HandleGetChildren(TSharedPtr<FContentItem> InItem, TArray<TSharedPtr<FContentItem>>& OutChildren);
	TSharedRef<ITableRow> HandleGenerateRow(TSharedPtr<FContentItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void RebuildRootFolderPaths();
	void GetIncludeRootPaths(TArray<FString>& RootFolderPaths);
	void HandleSelectionChanged(TSharedPtr<FContentItem> SelectedItem, ESelectInfo::Type SelectInfo);

	TArray<TSharedPtr<FContentItem>> GetAllContentItems();
	static TArray<TSharedPtr<FContentItem>> GetAllContentItemsByRootItems(TArray<TSharedPtr<FContentItem>> InRootItems);

public:
	TSharedPtr<FContentItem> FindContentItemByAssetItem(FAssetData Data);
	void SetAssetDataColor(FAssetData Data, const FSlateColor& Color);
	void ClearAssetDataColor(FAssetData Data);

	void SetPathColor(FString Path, const FSlateColor& Color, bool Refresh = true);
	void ClearPathColor(FString Path);
	void ClearAllPathColor();
	FSlateColor GetItemColorOrDefault(TSharedPtr<FContentItem> Item) const;

	TSharedPtr<FContentItem> FindContentItemBySegmentsRecursive(const TSharedPtr<FContentItem>& CurrentItem, const TArray<FString>& Segments, int32 Index);
	TSharedPtr<FContentItem> FindContentItemByPathAcrossRoots(const TArray<TSharedPtr<FContentItem>>& InRootItems, const FString& InPath);
	TSharedPtr<FContentItem> FindContentItemByPathAcrossRoots(const FString& InPath);

};

UCLASS(meta = (DisplayName = "Content Folder Host"))
class UContentFolderHostWidget : public UWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return FText::FromString(TEXT("Custom")); }
#endif

private:
	TSharedPtr<SContentFolderSimple> SlateContentFolderWidget;
};
