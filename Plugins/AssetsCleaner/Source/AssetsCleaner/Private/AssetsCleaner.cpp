// Copyright 2019-2022 Chris Garnier (The Tool Shed). All Rights Reserved.

#include "AssetsCleaner.h"
#include "AssetsCleanerStyle.h"
#include "AssetsCleanerCommands.h"
#include "LevelEditor.h"
#include "Widgets/SAssetsCleaner.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Runtime/UMG/Public/Blueprint/UserWidget.h"


static const FName AssetsCleanerTabName("AssetsCleaner");

#define LOCTEXT_NAMESPACE "FAssetsCleanerModule"

void FAssetsCleanerModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FAssetsCleanerStyle::Initialize();
	FAssetsCleanerStyle::ReloadTextures();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AssetsCleanerTabName,FOnSpawnTab::CreateRaw(this, &FAssetsCleanerModule::SpawnAssetsCleanerTab))
	.SetDisplayName(FText::FromString("Assets Cleaner"))
	.SetTooltipText(FText::FromString("Open the Assets Cleaner window"))
	.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
	.SetIcon(FSlateIcon(FAssetsCleanerStyle::GetStyleSetName(), "AssetsCleaner.OpenPluginWindow", "AssetsCleaner.OpenPluginWindow"));


}

void FAssetsCleanerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FAssetsCleanerStyle::Shutdown();


	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AssetsCleanerTabName);
}


TSharedRef<SDockTab> FAssetsCleanerModule::SpawnAssetsCleanerTab(const FSpawnTabArgs& SpawnTabArgs)
{
	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
	.TabRole(NomadTab) //DO NOT CHANGE, NEEDS TO BE NOMAD for UE5 (or will be f'd up)
	[
		SNew(SAssetsCleaner)
	];

	return DockTab;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAssetsCleanerModule, AssetsCleaner)