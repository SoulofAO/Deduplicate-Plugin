/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

#include "DeduplicatePlugin.h"
#include "DeduplicationManager.h"
#include "DeduplicationWidget.h"
#include "ToolMenus.h"
#include "EditorStyleSet.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "HAL/ThreadManager.h"


#define LOCTEXT_NAMESPACE "FDeduplicatePluginModule"

static const FName DeduplicationTabName("DeduplicationTab");

void FDeduplicatePluginModule::StartupModule()
{
	RegisterMenuExtensions();
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DeduplicationTabName, FOnSpawnTab::CreateRaw(this, &FDeduplicatePluginModule::OnSpawnTab))
		.SetDisplayName(LOCTEXT("FDeduplicationTabTitle", "Asset Deduplication"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	DeduplicationManager = NewObject<UDeduplicationManager>();
}

void FDeduplicatePluginModule::ShutdownModule()
{
	UnregisterMenuExtensions();
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DeduplicationTabName);
	DeduplicationManager = nullptr;
}

void FDeduplicatePluginModule::RegisterMenuExtensions()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FDeduplicatePluginModule::RegisterMenus));
}

void FDeduplicatePluginModule::UnregisterMenuExtensions()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FDeduplicatePluginModule::RegisterMenus()
{	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		{
			FToolMenuSection& Section = Menu->AddSection("DeduplicatePluginSection", LOCTEXT("DeduplicationSection", "Deduplication"));
			Section.AddMenuEntry(FName("OpenDeduplicationWidget"),
				LOCTEXT("OpenDeduplicationWidget", "Asset Deduplication"),
				LOCTEXT("OpenDeduplicationWidgetTooltip", "Open the Asset Deduplication tool"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(this, &FDeduplicatePluginModule::HandleMenuCommand)));
		}
	}
}

TSharedRef<SDockTab> FDeduplicatePluginModule::OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SDeduplicationWidget::CreateWidget(DeduplicationManager)
		];
}

void FDeduplicatePluginModule::HandleMenuCommand()
{
	OpenDeduplicationWidget();
}

void FDeduplicatePluginModule::OpenDeduplicationWidget()
{
	FGlobalTabmanager::Get()->TryInvokeTab(DeduplicationTabName);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDeduplicatePluginModule, DeduplicatePlugin)