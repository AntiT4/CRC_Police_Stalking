// Copyright 2019-2022 Chris Garnier (The Tool Shed). All Rights Reserved.


#include "Widgets/SAssetsCleaner.h"

#include "AssetsCleanerSettings.h"
#include "AssetsCleanerStyle.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "ContentBrowserDataSubsystem.h"
#include "IContentBrowserDataModule.h"
#endif
#include "Projects/Public/Interfaces/IPluginManager.h"
#include "UnrealEd/Public/Dialogs/Dialogs.h"
#include "Widgets/SAssetsCleanerBrowser.h"
#include "SlateOptMacros.h"
#include "Engine/AssetManager.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Misc/FileHelper.h"
#include "Settings/ContentBrowserSettings.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAssetsCleaner::Construct(const FArguments& InArgs)
{
	//make sure we refresh when options like ShowAllFolder or ShowPluginContent are changed globally
	GetMutableDefault<UContentBrowserSettings>()->OnSettingChanged().AddLambda([this](FName PropertyName)
	{
		UpdateFilter();
	});

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(4)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					BuildToolbar()
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FEditorStyle::Get().GetBrush("Icons.Help"))
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&)
						{
							if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("AssetsCleaner"))
							{
								const FPluginDescriptor Descriptor = Plugin->GetDescriptor();
								FPlatformProcess::LaunchURL(*Descriptor.DocsURL, nullptr, nullptr); 
							}
							return FReply::Handled();

						})
						.ToolTipText(FText::FromString("Open AssetsCleaner documentation"))
					]
				]
			]
			+SVerticalBox::Slot()
			.Padding(4)
			.AutoHeight()			
			[
				SNew(SErrorText)
				.Visibility_Lambda([](){return GetDefault<UAssetsCleanerSettings>()->bShowLevels? EVisibility::Visible : EVisibility::Collapsed;})
				.ErrorText(FText::FromString("Showing Levels. Usage cannot be accurately checked! See documentation"))
			]			
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(4)
				.BorderImage(FEditorStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
				[
					SNew(STextBlock)
					.Text(FText::FromString("UNUSED ASSETS"))
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				]
			]
			+SVerticalBox::Slot()
			.Padding(4)
			.FillHeight(1)
			[
				SNew(SAssetsCleanerBrowser)
				.RefreshAssetViewDelegate(&RefreshAssetViewDelegate)
				.SetFilterDelegate(&SetFilterDelegate)							
			]				
		]
	];
	UpdateFilter();

}

SAssetsCleaner::~SAssetsCleaner()
{
	//Save options before shutting down
   	GetMutableDefault<UAssetsCleanerSettings>()->SaveConfig();
}

void SAssetsCleaner::OnRefreshTriggered()
{
	RefreshView();
	
}

void SAssetsCleaner::RefreshView()
{
	RefreshAssetViewDelegate.ExecuteIfBound(true);
}

TSharedRef<SWidget> SAssetsCleaner::BuildToolbar()
{

	FToolBarBuilder ToolbarBuilder(MakeShareable(new FUICommandList), FMultiBoxCustomization::None);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
	ToolbarBuilder.AddToolBarButton(FUIAction(FExecuteAction::CreateRaw(this, &SAssetsCleaner::OnRefreshTriggered)),
		NAME_None,
		FText()
		,FText::FromString("Refresh results"),
		FSlateIcon(FEditorStyle::Get().GetStyleSetName(), "SourceControl.Actions.Refresh")); //Double check UE5 also has it in startshipstyle
	ToolbarBuilder.AddSeparator();
	FSlateIcon LevelsIcon;
#if ENGINE_MAJOR_VERSION > 4
	LevelsIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.NewLevel");
#else
	LevelsIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Levels");
#endif
	
	ToolbarBuilder.AddToolBarButton(
	FUIAction(
		FExecuteAction::CreateLambda([this]()
		{
			if (!GetDefault<UAssetsCleanerSettings>()->bShowLevels)
			{
				FString WarningText = "Level usage check only works for:" ;
				WarningText += LINE_TERMINATOR;
				WarningText += "- Levels loaded by object reference (not by name)";
				WarningText += LINE_TERMINATOR;
				WarningText += "- Sub levels of a persistent level";
				WarningText += LINE_TERMINATOR;
				WarningText += "- Levels referenced in the project settings (default map, maps to package etc...)";
				WarningText += LINE_TERMINATOR;
				WarningText += LINE_TERMINATOR;
				WarningText += "Level usage check does NOT work for:" ;
				WarningText += LINE_TERMINATOR;
				WarningText += "- Levels loaded by name (eg: Load Level 'level)'";
#if ENGINE_MAJOR_VERSION >= 5
				WarningText += LINE_TERMINATOR;
				WarningText += "- Open Worlds (World Partition) using OneFilePerActor";
#endif
				WarningText += LINE_TERMINATOR;
				WarningText += "Best judgement should be used before deleting a level";
				const FText ConfirmText = FText::FromString("I understand");
				FSuppressableWarningDialog::FSetupInfo SetupInfo(FText::FromString(WarningText), FText::FromString("Level usage warning"),"AssetsCleanerShowLevelsWarning");
				SetupInfo.ConfirmText = ConfirmText;
				const FSuppressableWarningDialog WarningDialog(SetupInfo);
				if (WarningDialog.ShowModal() == FSuppressableWarningDialog::Cancel)
				{
					return;
				}
			}
			UAssetsCleanerSettings* Settings = GetMutableDefault<UAssetsCleanerSettings>(); Settings->bShowLevels = !Settings->bShowLevels; UpdateFilter();				
		})
		,FCanExecuteAction()
		,FIsActionChecked::CreateLambda([this](){return GetDefault<UAssetsCleanerSettings>()->bShowLevels;}))
	,NAME_None
	,FText::FromString("Show Levels"),
	FText::FromString("Include levels?")
	,LevelsIcon
	,EUserInterfaceActionType::ToggleButton);

	ToolbarBuilder.AddToolBarButton(
	FUIAction(
		FExecuteAction::CreateLambda([this]()
		{			
			if (UContentBrowserSettings* Settings = GetMutableDefault<UContentBrowserSettings>())
			{
				Settings->SetDisplayPluginFolders(!Settings->GetDisplayPluginFolders());
				Settings->PostEditChange();
			}
		})
		,FCanExecuteAction()
		,FIsActionChecked::CreateLambda([this](){return GetDefault<UContentBrowserSettings>()->GetDisplayPluginFolders();}))
	,NAME_None
	,FText::FromString("Show Plugins"),
	FText::FromString("Show unused content from plugins with content")
	,FSlateIcon(FAssetsCleanerStyle::GetStyleSetName(), "Icons.Plugins")
	,EUserInterfaceActionType::ToggleButton);
	
	ToolbarBuilder.AddSeparator();
	
	ToolbarBuilder.AddToolBarButton(
	FUIAction(
		FExecuteAction::CreateLambda([this]()
		{			
			if (UAssetsCleanerSettings* Settings = GetMutableDefault<UAssetsCleanerSettings>())
			{
				Settings->bDeleteEmptyFolders = !Settings->bDeleteEmptyFolders;
				Settings->SaveConfig();
			}
		})
		,FCanExecuteAction()
		,FIsActionChecked::CreateLambda([this](){return GetDefault<UAssetsCleanerSettings>()->bDeleteEmptyFolders;}))
	,NAME_None
	,FText::FromString("Delete Empty Folders"),
	FText::FromString("Delete empty folders after deleting assets")
	,FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetTreeFolderOpen")
	,EUserInterfaceActionType::ToggleButton);
	
	return ToolbarBuilder.MakeWidget();
}


void SAssetsCleaner::UpdateFilter()
{
	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.ClassNames.Add(UObject::StaticClass()->GetFName()); //NEEDED for ClassExclusionSet to work, if empty it'll just include everything and ignore exclusion
	if (!GetDefault<UAssetsCleanerSettings>()->bShowLevels)
	{
		Filter.RecursiveClassesExclusionSet.Add(UWorld::StaticClass()->GetFName());
		Filter.RecursiveClassesExclusionSet.Add(UMapBuildDataRegistry::StaticClass()->GetFName());
	}
	
	Filter.bRecursivePaths = true;
	FString GamePath = "/Game";
	FString PrefixToAdd = "";
#if ENGINE_MAJOR_VERSION >= 5
	const UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
	if (GetDefault<UContentBrowserSettings>()->bShowAllFolder)
	{
		//This is very annoying but UE5 has that "all folders" thing that completely breaks package paths filters when on
		PrefixToAdd = ContentBrowserData->GetAllFolderPrefix();
	}	
#endif
	GamePath = PrefixToAdd + GamePath;
	const FName GamePathName = *GamePath; //Necessary for 4.23 that does not support FName(ExistingString)
	Filter.PackagePaths.Add(GamePathName);
	

	//Don;t need this is we use the editor's show plugin content, which we can't disable unfortunately
	if (GetDefault<UContentBrowserSettings>()->GetDisplayPluginFolders())
	{
		TArray<TSharedRef<IPlugin>> Plugins = IPluginManager::Get().GetEnabledPluginsWithContent();
		for (const TSharedRef<IPlugin>& Plugin : Plugins)
		{
			auto Path = Plugin->GetBaseDir();
			if (Path.StartsWith(FPaths::ProjectPluginsDir()))
			{
				auto MountPath = Plugin->GetMountedAssetPath();
				MountPath.RemoveFromEnd("/");
#if ENGINE_MAJOR_VERSION >= 5 //of course UE5 decided to change paths root for plugins		 		
				MountPath = "/Plugins" + MountPath;
#endif
				MountPath = PrefixToAdd + MountPath;
				FName MountPathName = *MountPath; //Necessary because 423 doesnt have a FName(ExistingString)
				Filter.PackagePaths.Add(MountPathName);
			}
		} //TODO: MAke this a selectable thing		
	}
	SetFilterDelegate.ExecuteIfBound(Filter);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
