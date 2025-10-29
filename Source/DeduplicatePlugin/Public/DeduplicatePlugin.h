// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "DeduplicationManager.h"
#include "DeduplicationWidget.h"

class FDeduplicatePluginModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void OpenDeduplicationWidget();

private:
	void RegisterMenuExtensions();
	void UnregisterMenuExtensions();
	void RegisterMenus();
	void HandleMenuCommand();

	TSharedRef<SDockTab> OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs);

	TSharedPtr<SDeduplicationWidget> DeduplicationWidget;
	FDelegateHandle MenuExtensionHandle;
	UDeduplicationManager* DeduplicationManager;
};
