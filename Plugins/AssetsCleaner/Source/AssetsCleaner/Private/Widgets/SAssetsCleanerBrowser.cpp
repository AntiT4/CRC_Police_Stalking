// Copyright 2019-2022 Chris Garnier (The Tool Shed). All Rights Reserved.


#include "Widgets/SAssetsCleanerBrowser.h"


#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <=23
#include "Toolkits/AssetEditorManager.h" //only needed for EditAsset (no AssetSubsytem)
#endif
#include "AssetManagerEditorModule.h"
#include "AssetsCleanerUtils.h"
#include "ContentBrowserModule.h"
#include "UnrealEd/Public/Toolkits/GlobalEditorCommonCommands.h"
#include "IContentBrowserSingleton.h"
#include "Misc/ScopedSlowTask.h"

#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAssetsCleanerBrowser::Construct(const FArguments& InArgs)
{	
	FAssetPickerConfig cfg;
	cfg.bAutohideSearchBar = false;
	//cfg.bShowBottomToolbar = false; //does not hide the settings button in UE5... wtf
	cfg.bShowBottomToolbar = true;
	cfg.bAddFilterUI = true;
	cfg.bCanShowDevelopersFolder = true;
	cfg.bShowPathInColumnView = true;
	cfg.bShowTypeInColumnView = true;
	cfg.bCanShowFolders = false;
	cfg.bCanShowClasses = false;
	cfg.bFocusSearchBoxWhenOpened = true;
	cfg.InitialAssetViewType = EAssetViewType::Tile;
	cfg.bAllowDragging = true;
#if ENGINE_MAJOR_VERSION ==4 && ENGINE_MINOR_VERSION < 25
#else //ForceEngineCOntent appears on 4.25
	cfg.bForceShowEngineContent = true; //will set to true but grey out the option (we filter it out anyway)
#endif
	cfg.bPreloadAssetsForContextMenu = false;
	cfg.AssetShowWarningText = FText::FromString("No unused assets found! Check filters, or try deleting levels that are not in use");
	cfg.OnAssetTagWantsToBeDisplayed = FOnShouldDisplayAssetTag::CreateLambda([](FName AssetType, FName TagName)
	{
		return TagName == "Path" || TagName == "Class";
	});
	cfg.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateLambda([](const FAssetData& Asset){GEditor->EditObject(Asset.FastGetAsset(true));});
	cfg.RefreshAssetViewDelegates.Add(&InternalRefreshDelegate);
	if (InArgs._RefreshAssetViewDelegate) {cfg.RefreshAssetViewDelegates.Add(InArgs._RefreshAssetViewDelegate);} //This is a pointer to a delegate that is given to this slate by its creator and will trigger from there to here
	if (InArgs._SetFilterDelegate) {cfg.SetFilterDelegates.Add(InArgs._SetFilterDelegate);} // so that external owner can change the filter afterwards 
	cfg.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SAssetsCleanerBrowser::OnGetContextMenu);
	cfg.OnShouldFilterAsset = FOnShouldFilterAsset::CreateStatic(AssetsCleanerUtils::ShouldFilterOutAsset);

	IAssetManagerEditorModule& AMModule = FModuleManager::LoadModuleChecked< IAssetManagerEditorModule >("AssetManagerEditor");
	AMModule.GetCurrentRegistrySource();//TODO RefreshRegistryData used to crash on 4.23 but doing this check somehow makes it not crash and work with columns
	//AMModule.RefreshRegistryData(); //Used to be needed for custom disksize column to work
	cfg.CustomColumns.Emplace(AMModule.DiskSizeName,
		FText::FromString("Exclusive Disk Size"),
		FText::FromString("Size of saved file on disk for only this asset"),
		UObject::FAssetRegistryTag::TT_Numerical,
		FOnGetCustomAssetColumnData::CreateLambda([&AMModule](FAssetData& AssetData, FName ColumnName){ FString OutValue;	AMModule.GetStringValueForCustomColumn(AssetData, ColumnName, OutValue); return OutValue;}),
		FOnGetCustomAssetColumnDisplayText::CreateLambda([&AMModule](FAssetData& AssetData, FName ColumnName){	FText OutValue;	AMModule.GetDisplayTextForCustomColumn(AssetData, ColumnName, OutValue); return OutValue;}));
	//cfg.SaveSettingsName //TODO: Implement this this is nice

	const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	
	ChildSlot
	[
		ContentBrowserModule.Get().CreateAssetPicker(cfg)
	];
	
	
}

TSharedPtr<SWidget> SAssetsCleanerBrowser::OnGetContextMenu(const TArray<FAssetData>& Assets)
{
	FMenuBuilder Builder(false,nullptr);
	Builder.BeginSection(NAME_None,FText::FromString("Locate"));
	Builder.AddMenuEntry(
		FText::FromString("Find in Content Browser"),
		FText::FromString("Finds selected assets in a content browser"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "SystemWideCommands.FindInContentBrowser"),
		FUIAction(
			FExecuteAction::CreateLambda([Assets]()
			{
				const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				ContentBrowserModule.Get().SyncBrowserToAssets( Assets, false, true );
			})));
	
	// Find in Explorer
	Builder.AddMenuEntry(
		FText::FromString("Find in " + FPlatformMisc::GetFileManagerName().ToString()),
		FText::FromString("Finds this asset on disk"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.OpenSourceLocation"),
		FUIAction(
		FExecuteAction::CreateLambda([Assets]()
		{
			AssetsCleanerUtils::LocateAssetsOnDisk(Assets);
		})));

	Builder.BeginSection(NAME_None, FText::FromString("Operations"));
	Builder.AddMenuEntry(
	FText::FromString("Edit"),
	FText::FromString("Open selected assets in their default editor"),
	FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.Edit"),
	FUIAction(
		FExecuteAction::CreateLambda([Assets]()
		{
			FScopedSlowTask LoadTask(Assets.Num(), FText::FromString("Loading Assets"));
			LoadTask.MakeDialogDelayed(0.5f, true);
			for (const FAssetData& Asset : Assets)
			{
				if (!LoadTask.ShouldCancel())
				{
					LoadTask.EnterProgressFrame(1);
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <=23					
					FAssetEditorManager::Get().OpenEditorForAsset(Asset.GetAsset());
#else
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset.GetAsset());	//ASsetEditorSubsystem is only 4.24+
#endif
				} else
				{
					break;
				}			
			}
		})));
	Builder.AddMenuSeparator();
	Builder.AddMenuEntry(
	FText::FromString("Delete"),
	FText::FromString("Delete the selected assets using the editor's standard delete window"),
	FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelScript.Delete"),
	FUIAction(
		FExecuteAction::CreateLambda([this, Assets]()
		{
			AssetsCleanerUtils::DeleteAssets(Assets);
			InternalRefreshDelegate.ExecuteIfBound(true);
		})));
	Builder.AddSubMenu(FText::FromString("Advanced Delete..."), FText::FromString("Delete selected assets using faster methods with less checks"),
		FNewMenuDelegate::CreateLambda([this, Assets](FMenuBuilder& SubMenuBuilder)
		{
			SubMenuBuilder.AddMenuEntry(
			FText::FromString("Smart Delete (Experimental, Coming Soon!)"),
			FText::FromString("Load assets one by one instead of all at once. Reduces memory issues, allows cancelling, and a bit faster. Assets with delete issues will be shown in the editor's delete window at the end."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.Delete"),
			FUIAction(
				FExecuteAction::CreateLambda([this, Assets]()
				{
					AssetsCleanerUtils::SmartDeleteAssets(Assets);
					InternalRefreshDelegate.ExecuteIfBound(true);
					
				}),FCanExecuteAction::CreateLambda([](){return false;}))); //greyed out for now
			//TEMP DISABLED UNTIL TESTED MORE
			/*SubMenuBuilder.AddMenuEntry(
			FText::FromString("Fast Force Delete (Coming soon)"),
			FText::FromString("Directly deletes the selected assets' files on disk. Does not load assets to check them. Way faster, but unsafe. Will restart editor once done. BACKUP YOUR FILES OR USE SOURCE CONTROL FIRST"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Warning"),
			FUIAction(
			FExecuteAction::CreateLambda([this, Assets]()
			{
				AssetsCleanerUtils::ForceDeleteAssetFilesOnDisk(Assets);
				InternalRefreshDelegate.ExecuteIfBound(true);

			}
			)));*/
		}));
	Builder.AddMenuSeparator();
	Builder.AddMenuEntry(
	FText::FromString("Move to..."),
	FText::FromString("Move selected assets to a different folder"),
	FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.OpenLevel"),
	FUIAction(
		FExecuteAction::CreateLambda([this, Assets]()
		{
			AssetsCleanerUtils::MoveAssetsToPickedFolder(Assets);
			InternalRefreshDelegate.ExecuteIfBound(true);

		})));
	
	Builder.BeginSection(NAME_None,FText::FromString("Management"));
	Builder.AddMenuEntry(
	FText::FromString("Size map..."),
	FText::FromString("Show assets in the Size Map which displays an interactive map of these assets' size (and  their references)"),
	FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Type.Memory"),
	FUIAction(
		FExecuteAction::CreateLambda([Assets]()
		{
			AssetsCleanerUtils::OpenAssetsInSizeMap(Assets);
		})));
	Builder.AddMenuEntry(
	FText::FromString("Reference Viewer..."),
	FText::FromString("Launch the reference viewer showing the assets' references"),
	FSlateIcon(FEditorStyle::GetStyleSetName(), "PhysicsAssetEditor.Tabs.Graph"),
	FUIAction(
		FExecuteAction::CreateLambda([Assets]()
		{
			AssetsCleanerUtils::OpenAssetsInRefViewer(Assets);
		})));
	Builder.AddMenuEntry(
	FText::FromString("Audit Asset..."),
	FText::FromString("Show assets in the audit assets UI for more detailed information on disk size and usage"),
	FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"),
	FUIAction(
		FExecuteAction::CreateLambda([Assets]()
		{
			AssetsCleanerUtils::OpenAssetsInAudit(Assets);
		})));		
	Builder.EndSection();

	return Builder.MakeWidget();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION
