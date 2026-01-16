/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#include "DeduplicationContentWidget.h"
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
#include "HAL/ThreadManager.h"


TSharedRef<SWidget> UContentFolderHostWidget::RebuildWidget()
{
	SlateContentFolderWidget = SNew(SContentFolderSimple);

	return SNew(SBox)
		[
			SlateContentFolderWidget.ToSharedRef()
		];
}

void SContentFolderSimple::Destruct()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	AssetRegistry.OnAssetAdded().Remove(AssetAddedHandle);
	AssetRegistry.OnAssetRemoved().Remove(AssetRemovedHandle);
	AssetRegistry.OnPathAdded().Remove(PathAddedHandle);
	AssetRegistry.OnPathRemoved().Remove(PathRemovedHandle);
}

void SContentFolderSimple::Construct(const FArguments& InArgs)
{
	OnContentItemSelectedDelegate = InArgs._OnContentItemSelectedDelegate;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	AssetAddedHandle = AssetRegistry.OnAssetAdded().AddRaw(this, &SContentFolderSimple::OnAssetAdded);
	AssetRemovedHandle = AssetRegistry.OnAssetRemoved().AddRaw(this, &SContentFolderSimple::OnAssetRemoved);
	PathAddedHandle = AssetRegistry.OnPathAdded().AddRaw(this, &SContentFolderSimple::OnPathAdded);
	PathRemovedHandle = AssetRegistry.OnPathRemoved().AddRaw(this, &SContentFolderSimple::OnPathRemoved);



	ChildSlot
		[
			SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)

						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SCheckBox)
								.IsChecked(this, &SContentFolderSimple::GetContentCheckState)
								.OnCheckStateChanged(this, &SContentFolderSimple::OnContentCheckChanged)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("Content")))
								]
						]

						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8, 0, 0, 0))
						[
							SNew(SCheckBox)
								.IsChecked(this, &SContentFolderSimple::GetPluginsCheckState)
								.OnCheckStateChanged(this, &SContentFolderSimple::OnPluginsCheckChanged)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("Plugins")))
								]
						]

						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8, 0, 0, 0))
						[
							SNew(SCheckBox)
								.IsChecked(this, &SContentFolderSimple::GetEngineCheckState)
								.OnCheckStateChanged(this, &SContentFolderSimple::OnEngineCheckChanged)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("Engine")))
								]
						]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 0))
				[
					SNew(SBorder)
				]

				+ SVerticalBox::Slot().FillHeight(1)
				[
					SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1)
						[
							SAssignNew(TreeView, STreeView<TSharedPtr<FContentItem>>)
								.TreeItemsSource(&RootItems)
								.OnGetChildren(this, &SContentFolderSimple::HandleGetChildren)
								.OnGenerateRow(this, &SContentFolderSimple::HandleGenerateRow)
								.SelectionMode(ESelectionMode::Single)
								.OnSelectionChanged(this, &SContentFolderSimple::HandleSelectionChanged)
						]
				]
		];

	RebuildRootFolderPaths();
}

void SContentFolderSimple::OnAssetAdded(const FAssetData& AssetData)
{
	AddAsset(AssetData);
}

void SContentFolderSimple::AddAsset(const FAssetData& AssetData)
{
	if (AssetData.IsRedirector()) return;
	if (FindContentItemByAssetItem(AssetData)) return;

	TArray<FString> RootFolderPaths;
	GetIncludeRootPaths(RootFolderPaths);

	FString AssetPackagePath = AssetData.PackagePath.ToString();
	AssetPackagePath = AssetPackagePath.Replace(TEXT("\\"), TEXT("/"));
	AssetPackagePath.TrimStartAndEndInline();
	while (AssetPackagePath.EndsWith(TEXT("/"))) AssetPackagePath.RemoveAt(AssetPackagePath.Len() - 1);
	if (!AssetPackagePath.StartsWith(TEXT("/")))
	{
		AssetPackagePath = FString::Format(TEXT("/{0}"), { AssetPackagePath });
	}

	bool bAllowed = RootFolderPaths.Num() == 0;
	if (!bAllowed)
	{
		for (FString RootFolderPath : RootFolderPaths)
		{
			RootFolderPath = RootFolderPath.Replace(TEXT("\\"), TEXT("/"));
			RootFolderPath.TrimStartAndEndInline();
			while (RootFolderPath.EndsWith(TEXT("/"))) RootFolderPath.RemoveAt(RootFolderPath.Len() - 1);
			if (!RootFolderPath.StartsWith(TEXT("/")))
			{
				RootFolderPath = FString::Format(TEXT("/{0}"), { RootFolderPath });
			}

			if (AssetPackagePath.StartsWith(RootFolderPath, ESearchCase::IgnoreCase))
			{
				bAllowed = true;
				break;
			}
		}
	}

	if (!bAllowed) return;

	TSharedPtr<FContentItem> NewItem = FContentItem::CreateAsset(AssetData);
	TSharedPtr<FContentItem> ParentItem = FindContentItemByPathAcrossRoots(AssetData.PackagePath.ToString());
	if (ParentItem.IsValid())
	{
		ParentItem->Children.Add(NewItem);
		NewItem->Path = FString::Format(TEXT("{0}/{1}"), { AssetData.PackagePath.ToString(), AssetData.AssetName.ToString() });
		NewItem->Parent = ParentItem;
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}


void SContentFolderSimple::OnAssetRemoved(const FAssetData& AssetData )
{
	RemoveAsset(AssetData);
}

void SContentFolderSimple::RemoveAsset(const FAssetData& AssetData)
{
	TSharedPtr<FContentItem> ItemPtr = FindContentItemByPathAcrossRoots(AssetData.PackagePath.ToString() + "/" + AssetData.AssetName.ToString());
	if (ItemPtr)
	{
		ItemPtr->Parent->Children.Remove(ItemPtr);
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}
void SContentFolderSimple::OnPathAdded(const FString& InPath)
{
	AddPath(InPath);
}

void SContentFolderSimple::AddPath(FString Path)
{
	Path = Path.Replace(TEXT("\\"), TEXT("/"));
	Path.TrimStartAndEndInline();
	while (Path.StartsWith(TEXT("/"))) Path.RemoveAt(0);
	while (Path.EndsWith(TEXT("/"))) Path.RemoveAt(Path.Len() - 1);
	if (Path.IsEmpty()) return;

	TArray<FString> Segments;
	Path.ParseIntoArray(Segments, TEXT("/"), true);
	if (Segments.Num() == 0) return;

	TArray<FString> RootFolderPaths;
	GetIncludeRootPaths(RootFolderPaths);

	FString FullPathWithLeadingSlash = FString::Format(TEXT("/{0}"), { Path });

	bool bAllowed = RootFolderPaths.Num() == 0;
	if (!bAllowed)
	{
		for (FString RootFolderPath : RootFolderPaths)
		{
			RootFolderPath = RootFolderPath.Replace(TEXT("\\"), TEXT("/"));
			RootFolderPath.TrimStartAndEndInline();
			while (RootFolderPath.EndsWith(TEXT("/"))) RootFolderPath.RemoveAt(RootFolderPath.Len() - 1);
			if (!RootFolderPath.StartsWith(TEXT("/")))
			{
				RootFolderPath = FString::Format(TEXT("/{0}"), { RootFolderPath });
			}

			if (FullPathWithLeadingSlash.StartsWith(RootFolderPath) && RootFolderPath.Contains(Segments[0]))
			{
				bAllowed = true;
				break;
			}
		}
	}

	if (!bAllowed) return;

	TSharedPtr<FContentItem> RootItem;
	for (const TSharedPtr<FContentItem>& CandidateRoot : RootItems)
	{
		if (CandidateRoot.IsValid() && CandidateRoot->Name.Equals(Segments[0], ESearchCase::IgnoreCase))
		{
			RootItem = CandidateRoot;
			break;
		}
	}

	if (!RootItem.IsValid())
	{
		FString RootPath = FString::Format(TEXT("/{0}"), { Segments[0] });
		RootItem = FContentItem::CreateFolder(Segments[0], RootPath);
		RootItems.Add(RootItem);
	}

	TSharedPtr<FContentItem> CurrentItem = RootItem;

	for (int32 Index = 1; Index < Segments.Num(); ++Index)
	{
		const FString& Segment = Segments[Index];

		TSharedPtr<FContentItem> FoundChild;
		for (const TSharedPtr<FContentItem>& Child : CurrentItem->Children)
		{
			if (Child.IsValid() && Child->Name.Equals(Segment, ESearchCase::IgnoreCase))
			{
				FoundChild = Child;
				break;
			}
		}

		if (FoundChild.IsValid())
		{
			CurrentItem = FoundChild;
			continue;
		}

		FString ChildPath;
		if (CurrentItem->Path.IsEmpty())
		{
			ChildPath = FString::Format(TEXT("/{0}/{1}"), { CurrentItem->Name, Segment });
		}
		else
		{
			ChildPath = FString::Format(TEXT("{0}/{1}"), { CurrentItem->Path, Segment });
		}

		TSharedPtr<FContentItem> NewFolder = FContentItem::CreateFolder(Segment, ChildPath);
		NewFolder->Parent = CurrentItem;
		CurrentItem->Children.Add(NewFolder);
		CurrentItem = NewFolder;
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}


void SContentFolderSimple::OnPathRemoved(const FString& InPath)
{
	RemovePath(InPath);
}

void SContentFolderSimple::RemovePath(FString Path)
{
	Path = Path.Replace(TEXT("\\"), TEXT("/"));
	Path.TrimStartAndEndInline();
	while (Path.StartsWith(TEXT("/"))) Path.RemoveAt(0);
	while (Path.EndsWith(TEXT("/"))) Path.RemoveAt(Path.Len() - 1);
	if (Path.IsEmpty()) return;

	TSharedPtr<FContentItem> TargetItem = FindContentItemByPathAcrossRoots(Path);
	if (!TargetItem.IsValid()) return;

	TSharedPtr<FContentItem> ParentItem = TargetItem->Parent;

	if (ParentItem.IsValid())
	{
		ParentItem->Children.RemoveSingle(TargetItem);
		TargetItem->Parent.Reset();
	}
	else
	{
		RootItems.RemoveSingle(TargetItem);
	}

	TSharedPtr<FContentItem> Node = ParentItem;
	while (Node.IsValid() && Node->Children.Num() == 0)
	{
		TSharedPtr<FContentItem> ParentNode = Node->Parent;
		if (ParentNode.IsValid())
		{
			ParentNode->Children.RemoveSingle(Node);
			Node->Parent.Reset();
		}
		else
		{
			RootItems.RemoveSingle(Node);
		}
		Node = ParentNode;
	}
	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

ECheckBoxState SContentFolderSimple::GetContentCheckState() const
{
	return bIncludeContent ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SContentFolderSimple::GetPluginsCheckState() const
{
	return bIncludePlugins ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SContentFolderSimple::GetEngineCheckState() const
{
	return bIncludeEngine ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SContentFolderSimple::OnContentCheckChanged(ECheckBoxState NewState)
{
	bIncludeContent = (NewState == ECheckBoxState::Checked);
	RebuildRootFolderPaths();
}

void SContentFolderSimple::OnPluginsCheckChanged(ECheckBoxState NewState)
{
	bIncludePlugins = (NewState == ECheckBoxState::Checked);
	RebuildRootFolderPaths();
}

void SContentFolderSimple::OnEngineCheckChanged(ECheckBoxState NewState)
{
	bIncludeEngine = (NewState == ECheckBoxState::Checked);
	RebuildRootFolderPaths();
}

TSharedPtr<FContentItem> SContentFolderSimple::GetSelectedItem()
{
	if (!TreeView)
	{
		return nullptr;
	}
	if (TreeView->GetNumItemsSelected() > 0)
	{
		return TreeView->GetSelectedItems()[0];
	}
	return nullptr;
}

void SContentFolderSimple::HandleGetChildren(TSharedPtr<FContentItem> InItem, TArray<TSharedPtr<FContentItem>>& OutChildren)
{
	OutChildren = InItem->Children;
}

TSharedRef<ITableRow> SContentFolderSimple::HandleGenerateRow(TSharedPtr<FContentItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool = UThumbnailManager::Get().GetSharedThumbnailPool();

	FSlateColor SlateColor = GetItemColorOrDefault(InItem);

	if (!InItem->bIsFolder)
	{
		const int32 TileViewThumbnailResolution = 64;
		TSharedPtr<FAssetThumbnail> AssetThumbnail;
		AssetThumbnail = MakeShared<FAssetThumbnail>(InItem->Data, TileViewThumbnailResolution, TileViewThumbnailResolution, ThumbnailPool);
		AssetThumbnail->GetViewportRenderTargetTexture();

		return SNew(STableRow<TSharedPtr<FContentItem>>, OwnerTable)
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(48).HeightOverride(48)
							[
								AssetThumbnail->MakeThumbnailWidget()
							]
					]
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(FMargin(6, 0, 0, 0))
					[
						SNew(STextBlock).
							Text(FText::FromString(InItem->Name))
							.ColorAndOpacity_Lambda([this, InItem]() -> FSlateColor {
								return GetItemColorOrDefault(InItem);
							})
					]
			];

	}
	else
	{
		const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("ContentBrowser.AssetTreeFolderOpen");

		return SNew(STableRow<TSharedPtr<FContentItem>>, OwnerTable)
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(48).HeightOverride(48)
							[
								SNew(SImage).Image(Brush)
							]
					]
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(FMargin(6, 0, 0, 0))
					[
						SNew(STextBlock).Text(FText::FromString(InItem->Name))
							.ColorAndOpacity_Lambda([this, InItem]() -> FSlateColor {
								return GetItemColorOrDefault(InItem);
							})
					]
			];
	}

}

void SContentFolderSimple::RebuildRootFolderPaths()
{
	RootItems.Empty();

	TArray<FString> RootFolderPaths;
	GetIncludeRootPaths(RootFolderPaths);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	for (const FString& RootPath : RootFolderPaths)
	{
		FString CleanName = FPaths::GetCleanFilename(RootPath);
		if (CleanName.IsEmpty())
		{
			CleanName = RootPath; 
		}
		AddPath(CleanName);
	}

	for (const FString& RootPath : RootFolderPaths)
	{
		TArray<FString> SubFolders;
		AssetRegistry.GetSubPaths(RootPath, SubFolders, true);
		for (FString SubFolder : SubFolders)
		{
			AddPath(SubFolder);
		}

		TArray<FAssetData> AssetDatas;
		AssetRegistry.GetAssetsByPath(FName(*RootPath), AssetDatas, /*bRecursive=*/true);
		for (const FAssetData& AssetData : AssetDatas)
		{
			AddAsset(AssetData);
		}
	}
	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

void SContentFolderSimple::GetIncludeRootPaths(TArray<FString>& RootFolderPaths)
{
	if (bIncludeContent)
	{
		RootFolderPaths.Add(TEXT("/Game"));
	}

	if (bIncludePlugins)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		TArray<TSharedRef<IPlugin>> Plugins = IPluginManager::Get().GetEnabledPluginsWithContent();
		for (const TSharedRef<IPlugin>& Plugin : Plugins)
		{
			const FString MountedPath = Plugin->GetMountedAssetPath();
			if (!MountedPath.IsEmpty())
			{
				RootFolderPaths.Add(MountedPath);
			}
		}
	}

	if (bIncludeEngine)
	{
		RootFolderPaths.Add(TEXT("/Engine"));
	}
}

void SContentFolderSimple::HandleSelectionChanged(TSharedPtr<FContentItem> SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectedItem.IsValid())
	{
		OnContentItemSelectedDelegate.ExecuteIfBound(SelectedItem);
	}
}

TArray<TSharedPtr<FContentItem>> SContentFolderSimple::GetAllContentItems()
{
	return GetAllContentItemsByRootItems(RootItems);
}

TArray<TSharedPtr<FContentItem>> SContentFolderSimple::GetAllContentItemsByRootItems(TArray<TSharedPtr<FContentItem>> InRootItems)
{
	TArray<TSharedPtr<FContentItem>> AllItems;
	TArray<TSharedPtr<FContentItem>> Stack = InRootItems;

	while (Stack.Num() > 0)
	{
		TSharedPtr<FContentItem> Current = Stack.Pop();
		if (!Current.IsValid())
			continue;

		AllItems.Add(Current);
		for (int32 i = Current->Children.Num() - 1; i >= 0; --i)
		{
			if (Current->Children[i].IsValid())
			{
				Stack.Add(Current->Children[i]);
			}
		}
	}

	return AllItems;
}

TSharedPtr<FContentItem> SContentFolderSimple::FindContentItemByAssetItem(FAssetData Data)
{
	TArray<TSharedPtr<FContentItem>> ContentItems = GetAllContentItems();
	for (TSharedPtr<FContentItem> ContentItem : ContentItems)
	{
		if (ContentItem->Data == Data)
		{
			return ContentItem;
		}
	}
	return TSharedPtr<FContentItem>();
}

void SContentFolderSimple::SetAssetDataColor(FAssetData Data, const FSlateColor& Color)
{
	if (Data.IsValid())
	{
		SetPathColor(Data.PackagePath.ToString() + "/" + Data.AssetName.ToString(), Color);
	}
}

void SContentFolderSimple::ClearAssetDataColor(FAssetData Data)
{
	if (Data.IsValid())
	{
		ClearPathColor(Data.PackagePath.ToString() + "/" + Data.AssetName.ToString());
	}
}

void SContentFolderSimple::SetPathColor(FString Path, const FSlateColor& Color, bool Refresh)
{
	if (Path.IsEmpty())
	{
		return;
	}
	ItemColors.Add(Path, Color);
	if (Refresh)
	{
		if (TreeView.IsValid())
		{
			TreeView->RequestTreeRefresh();
		}
	}
}

void SContentFolderSimple::ClearPathColor(FString Path)
{
	if (Path.IsEmpty())
	{
		return;
	}
	TSharedPtr<FContentItem> ContentItem = FindContentItemByPathAcrossRoots(Path);
	if (ContentItem)
	{
		ContentItem->bSetupColor = false;
	}
	ItemColors.Remove(Path);
	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

TSharedPtr<FContentItem> FContentItem::CreateFolder(const FString& InName, const FString& InPath)
{
	TSharedPtr<FContentItem> Item = MakeShared<FContentItem>();
	Item->Name = InName;
	Item->Path = InPath;
	Item->bIsFolder = true;
	return Item;
}

TSharedPtr<FContentItem> FContentItem::CreateAsset(FAssetData InData)
{
	TSharedPtr<FContentItem> Item = MakeShared<FContentItem>();
	Item->Name = InData.AssetName.ToString();
	Item->Data = InData;
	Item->bIsFolder = false;
	return Item;
}


void SContentFolderSimple::ClearAllPathColor()
{
	TArray<TSharedPtr<FContentItem>> Items = GetAllContentItems();

	for (TSharedPtr<FContentItem> Item : Items)
	{
		if (Item.IsValid())
		{
			Item->bSetupColor = false;
		}
	}

	ItemColors.Empty();
}

FSlateColor SContentFolderSimple::GetItemColorOrDefault(TSharedPtr<FContentItem> Item) const
{
	if (!Item.IsValid())
	{
		return FSlateColor(FLinearColor::White);
	}
	if (Item->bSetupColor)
	{
		return Item->Color;
	}
	else
	{
		const FSlateColor* Found = ItemColors.Find(Item->Path);
		if (Found)
		{
			Item->bSetupColor = true;
			Item->Color = *Found;
			return *Found;
		}
	}

	return FSlateColor(FLinearColor::White);
}

TSharedPtr<FContentItem> SContentFolderSimple::FindContentItemBySegmentsRecursive(const TSharedPtr<FContentItem>& CurrentItem, const TArray<FString>& Segments, int32 Index)
{
	if (!CurrentItem)
	{
		return nullptr;
	}

	if (!CurrentItem.IsValid()) return nullptr;
	if (Index >= Segments.Num()) return CurrentItem;
	if (Index == 0 && Segments.Num() > 0 && CurrentItem->Name.Equals(Segments[0], ESearchCase::IgnoreCase))
	{
		Index++;
		if (Index >= Segments.Num()) return CurrentItem;
	}
	const FString& Segment = Segments[Index];
	for (const TSharedPtr<FContentItem>& Child : CurrentItem->Children)
	{
		if (!Child.IsValid()) continue;
		if (Child->Name.Equals(Segment, ESearchCase::IgnoreCase))
		{
			return FindContentItemBySegmentsRecursive(Child, Segments, Index + 1);
		}
	}
	return nullptr;
}

TSharedPtr<FContentItem> SContentFolderSimple::FindContentItemByPathAcrossRoots(const TArray<TSharedPtr<FContentItem>>& InRootItems, const FString& InPath)
{
	if (InRootItems.Num() == 0) return nullptr;
	FString Path = InPath;
	Path = Path.Replace(TEXT("\\"), TEXT("/"));
	Path.TrimStartAndEndInline();
	while (Path.StartsWith(TEXT("/"))) Path.RemoveAt(0);
	while (Path.EndsWith(TEXT("/"))) Path.RemoveAt(Path.Len() - 1);
	if (Path.IsEmpty()) return nullptr;
	TArray<FString> Segments;
	Path.ParseIntoArray(Segments, TEXT("/"), true);
	for (const TSharedPtr<FContentItem>& Root : InRootItems)
	{
		if (!Root.IsValid()) continue;
		TSharedPtr<FContentItem> Found = FindContentItemBySegmentsRecursive(Root, Segments, 0);
		if (Found.IsValid()) return Found;
	}
	return nullptr;
}

TSharedPtr<FContentItem> SContentFolderSimple::FindContentItemByPathAcrossRoots(const FString& InPath)
{
	return FindContentItemByPathAcrossRoots(RootItems, InPath);
}
