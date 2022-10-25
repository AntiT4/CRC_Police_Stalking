// Copyright 2019-2022 Chris Garnier (The Tool Shed). All Rights Reserved.

#include "AssetsCleanerUtils.h"

#include "AssetManagerEditorModule.h"
#include "AssetsCleanerSettings.h"
#include "AssetToolsModule.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Editor/UnrealEd/Public/Dialogs/Dialogs.h"
#include "Editor/UnrealEd/Public/Dialogs/DlgPickPath.h"
#include "ISourceControlModule.h"
#include "ObjectTools.h"
#include "Core/Public/Misc/ScopedSlowTask.h"
#include "SourceControlHelpers.h"
#include "UnrealEdGlobals.h"
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 24 //4.24 changed buildsettingsversion
#include "AssetRegistryModule.h"
#else
#include "AssetRegistry/AssetRegistryModule.h"
#endif
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25
#include "Misc/BlacklistNames.h" //Appears in 425+ but NOT in ue5 ...wtf
#endif
#include "Editor/UnrealEdEngine.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "Misc/NamePermissionList.h"
#endif
#include "Settings/ProjectPackagingSettings.h"


bool AssetsCleanerUtils::ShouldFilterOutAsset(const FAssetData& AssetData)
{
	//CHECK ACTUAL REFERENCERS
	TArray<FName> Referencers;
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
	AssetRegistryModule.Get().GetReferencers(AssetData.PackageName, Referencers, EAssetRegistryDependencyType::All);
#else
	AssetRegistryModule.Get().GetReferencers(AssetData.PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::All);
#endif
	return Referencers.Num() > 0 || CheckAssetUsedInProjectSettings(AssetData);
}


void AssetsCleanerUtils::LocateAssetsOnDisk(const TArray<FAssetData>& Assets)
{
	for (const FAssetData& Asset : Assets)
	{
		const FString PackageName = Asset.PackageName.ToString();
		const bool bIsWorldAsset = (Asset.AssetClass == UWorld::StaticClass()->GetFName());
		const FString Extension = bIsWorldAsset ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
		const FString FilePath = FPackageName::LongPackageNameToFilename(PackageName, Extension);
		const FString FullFilePath = FPaths::ConvertRelativePathToFull(FilePath);
		FPlatformProcess::ExploreFolder(*FullFilePath);
	}
	
}

void AssetsCleanerUtils::DeleteAssets(const TArray<FAssetData>& Assets)
{
	if (WarnIfTooManyAssetsToDelete(Assets))
	{
		ObjectTools::DeleteAssets(Assets);
		DeleteAssetsEmptyDirectories(Assets);		
	}	
}


void AssetsCleanerUtils::SmartDeleteAssets(const TArray<FAssetData>& AssetsToDelete)
{
	const FText WarningText = FText::FromString(FString::Printf(TEXT("This custom delete method, though faster and cancellable, does less safety checks than the standard method.%sAlways backup your files or use source control before doing this%sThe developer cannot be held responsible for any data loss."),LINE_TERMINATOR, LINE_TERMINATOR));
	const FText ConfirmText = FText::FromString("I understand");
	FSuppressableWarningDialog::FSetupInfo SetupInfo(WarningText, FText::FromString("Disclaimer"),"AssetsCleanerSmartDeleteWarning");
	SetupInfo.ConfirmText = ConfirmText;
	const FSuppressableWarningDialog WarningDialog(SetupInfo);
	if (WarningDialog.ShowModal() != FSuppressableWarningDialog::Cancel)
	{
		
		bool Succeeded = false;
		TArray<FAssetData> FailedDeleteAssets;

		auto AssetsNotDeletable = CheckAssetsCanBeDeleted(AssetsToDelete);
		TArray<FAssetData> DeletableAssets;
		for (auto Asset : AssetsToDelete)
		{
			if (!AssetsNotDeletable.Contains(Asset))
			{
				DeletableAssets.Add(Asset);
			}	
		}
		//This is to hopefully avoid having a SCC popup asking to mark for delete for every single file in the loop
		if (MakeAssetsDeletable(DeletableAssets))
		{
			UE_LOG(LogTemp, Error, TEXT("Some files failed to be made deletable"));
		}
		GEditor->ResetTransaction(FText::FromString("Undo Buffer reset before deleting assets"));
		//first double check all assets for ANY reference
		TArray<FAssetData>AssetsWithReferencers;
		TArray<FAssetData>AssetsWithoutReferencers;
		IAssetRegistry& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry")).Get();
		for (auto Asset : DeletableAssets)
		{
			TArray<FName> Referencers;
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26 //After 4.26 EAssetRegistryDependencyType was deprecated
			AssetRegistryModule.GetReferencers(Asset.PackageName,Referencers,EAssetRegistryDependencyType::All);			
#else//ue4.27+ or ue5
			AssetRegistryModule.GetReferencers(Asset.PackageName,Referencers,UE::AssetRegistry::EDependencyCategory::All,UE::AssetRegistry::EDependencyQuery::NoRequirements);
#endif
			if (Referencers.Num() > 0)
			{
				AssetsWithReferencers.Add(Asset);
			} else
			{
				AssetsWithoutReferencers.Add(Asset);
			}			
		}	
		
		// better delete way, we delete one by one, so we don't preload all assets first. and we allow cancelling		
		FSlateApplication::Get().DismissAllMenus();//force close context menus, because if any context menu is open the slowtask gets cancelled by slate
		FScopedSlowTask DeleteTask(AssetsWithoutReferencers.Num(), FText::FromString("Deleting assets..."));
		DeleteTask.MakeDialog(true, false);
		for (auto Asset : AssetsWithoutReferencers)
		{
			if (DeleteTask.ShouldCancel())
			{
				return;
			}
			DeleteTask.EnterProgressFrame(1, FText::FromString("Deleting " + Asset.AssetName.ToString()));
			if (ObjectTools::DeleteAssets(TArray<FAssetData>{Asset},false) != 1)
			{
				FailedDeleteAssets.Add(Asset);
			}			
		}
		if (AssetsWithReferencers.Num() > 0 || FailedDeleteAssets.Num() > 0)
		//If we found any asset with undetected ref or failed to delete any file we use the old way
		{
			FText Title =FText::FromString("Remaining Assets to Delete");
			FText Message = FText::FromString(FString::Printf(TEXT("Some of the assets could not be deleted.%sThe editor's Delete window will now be opened with those assets to try the standard method and show potential memory references."), LINE_TERMINATOR));
			if (FMessageDialog::Open(EAppMsgType::OkCancel,	Message, &Title) == EAppReturnType::Ok)
			{
				TArray<FAssetData> AssetsToTryAgain;
				AssetsToTryAgain.Append(AssetsWithReferencers);
				AssetsToTryAgain.Append(FailedDeleteAssets);
				int32 numAssetsDeleted = ObjectTools::DeleteAssets(AssetsToTryAgain, true);
				if (numAssetsDeleted == AssetsToTryAgain.Num())
				{
					Succeeded = true;
				}					
			}

			//CustopmDialog doesn't work in 423 (link errors)
			/*TSharedRef<SCustomDialog> LeftOverAssetsDialog = SNew(SCustomDialog)
			.Title(FText::FromString("Remaining Assets to Delete"))
			.DialogContent( SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Some of the assets could not be deleted.%sThe editor's Delete window will now be opened with those assets to try the standard method and show potential memory references."), LINE_TERMINATOR))))
			.Buttons({
				SCustomDialog::FButton(FText::FromString("Ok")),
				SCustomDialog::FButton(FText::FromString("Cancel"))
			});
			// returns 0 when OK is pressed, 1 when Cancel is pressed, -1 if the window is closed
			const int ButtonPressed = LeftOverAssetsDialog->ShowModal();
			//if (FMessageDialog::Open(EAppMsgType::OkCancel, FText::FromString(FString::Printf(TEXT("Some of the assets could not be deleted.%sThe editor's Delete window will now be opened with those assets to try a standard method."), LINE_TERMINATOR))) == EAppReturnType::Ok)
			if (ButtonPressed == 0)
			{
				TArray<FAssetData> AssetsToTryAgain;
				AssetsToTryAgain.Append(AssetsWithReferencers);
				AssetsToTryAgain.Append(FailedDeleteAssets);
				int32 numAssetsDeleted = ObjectTools::DeleteAssets(AssetsToTryAgain, true);
				if (numAssetsDeleted == AssetsToTryAgain.Num())
				{
					Succeeded = true;
				}			
			}*/
		} else
		{
			Succeeded = true;
		}

		if (!Succeeded)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Failed to delete some of the assets"));
		}
	}
	
}

void AssetsCleanerUtils::ForceDeleteAssetFilesOnDisk(const TArray<FAssetData>& Assets)
{
	const FText WarningText = FText::FromString(FString::Printf(TEXT("This delete method though much faster, is very experimental and should only be used as last resort (ie: deleting a lot of huge files).%sIt direclty deletes the files on disk without using the editor or checking anything first.%sAlways backup your files or use source control before doing this%sThe developer cannot be held responsible for any data loss."),LINE_TERMINATOR, LINE_TERMINATOR, LINE_TERMINATOR));
	const FText ConfirmText = FText::FromString("I understand");
	FSuppressableWarningDialog::FSetupInfo SetupInfo(WarningText, FText::FromString("Disclaimer"),"AssetsCleanerForceDeleteWarning");
	SetupInfo.ConfirmText = ConfirmText;
	const FSuppressableWarningDialog WarningDialog(SetupInfo);
	if (WarningDialog.ShowModal() != FSuppressableWarningDialog::Cancel)
	{
		FMessageDialog CloseEditorWarning;
		if (CloseEditorWarning.Open(EAppMsgType::OkCancel,FText::FromString("The current level and all asset editors will be closed. The editor will automatically restart after deleting files.")) == EAppReturnType::Ok)
		{
			GEditor->ResetTransaction(FText::FromString("Undo Buffer reset before deleting assets"));
			GUnrealEd->NewMap();
			GEditor->CloseEditedWorldAssets(GEditor->GetWorld());
			GEditor->ClearPreviewComponents();
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 25 //TODO: Audio  manager GetAudioDeviceRaw only appears on 425
#else			
			// Ensure the audio manager is not holding on to any sounds
			FAudioDeviceManager* AudioDeviceManager = GEditor->GetAudioDeviceManager();
			if (AudioDeviceManager != nullptr)
			{
				AudioDeviceManager->UpdateActiveAudioDevices(false);

				const int32 NumAudioDevices = AudioDeviceManager->GetNumActiveAudioDevices();
				for (int32 DeviceIndex = 0; DeviceIndex < NumAudioDevices; DeviceIndex++)
				{
					FAudioDevice* AudioDevice = AudioDeviceManager->GetAudioDeviceRaw(DeviceIndex);
					if (AudioDevice != nullptr)
					{
						AudioDevice->StopAllSounds();
					}
				}
			}
#endif
			auto AssetsNotDeletable = CheckAssetsCanBeDeleted(Assets);
			TArray<FAssetData> DeletableAssets;
			for (auto Asset : Assets)
			{
				if (!AssetsNotDeletable.Contains(Asset))
				{
					DeletableAssets.Add(Asset);
				}	
			}
			if (USourceControlHelpers::IsEnabled())
			{
				TArray<FString> PackagesToDelete;
				TArray<FString> PackagesToRevert;
				for (auto Asset : DeletableAssets)
				{
					FString PackageFileName = USourceControlHelpers::PackageFilename(Asset.PackageName.ToString());
					auto State = USourceControlHelpers::QueryFileState(PackageFileName,true);
					if (State.bCanDelete) //should already be checked before we go here. see todo
					{
						PackagesToDelete.Add(PackageFileName);
					} else
					{
						if (IFileManager::Get().FileExists(*PackageFileName) && !IFileManager::Get().IsReadOnly(*PackageFileName))
						{
							IFileManager::Get().Delete(*PackageFileName);
						}	
					}			
				}
				for (auto Package : PackagesToDelete)
				{
					USourceControlHelpers::MarkFileForDelete(Package, true);
				}
				//USourceControlHelpers::MarkFilesForDelete(PackagesToDelete, true); //DO NOT USE, DOES NOT EXIST < 427
			}
			else
			{
				for (auto Asset : DeletableAssets)
				{
					FString PackageFileName = USourceControlHelpers::PackageFilename(Asset.PackageName.ToString());
					if (IFileManager::Get().FileExists(*PackageFileName) && !IFileManager::Get().IsReadOnly(*PackageFileName))
					{
						IFileManager::Get().Delete(*PackageFileName);
					}				
				} 	
			}	
			FUnrealEdMisc::Get().RestartEditor(false);			
		}
	}
}

void AssetsCleanerUtils::MoveAssetsToPickedFolder(const TArray<FAssetData>& Assets)
{
	TSharedPtr<SDlgPickPath> PickPathWidget =
	SNew(SDlgPickPath)
	.Title(FText::FromString("Move files to..."))
	.DefaultPath(FText::FromString("/Game"));

	if (PickPathWidget->ShowModal() == EAppReturnType::Ok)
	{
		const FString Pickedpath = PickPathWidget->GetPath().ToString();
		if (Pickedpath.IsEmpty() || Pickedpath.StartsWith("/Game/Collections"))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Invalid folder selected"));
		}
		else
		{
			FScopedSlowTask LoadTask(Assets.Num(),FText::FromString("Loading Assets before move"));
			LoadTask.MakeDialogDelayed(0.5f, true);
			TArray<UObject*> AssetsToMove;
			for (auto Asset :Assets)
			{
				if (LoadTask.ShouldCancel())
				{
					return;
				}
				AssetsToMove.Add(Asset.GetAsset());
			}
			MoveAssets(AssetsToMove,Pickedpath);
		}
	}
	

}

void AssetsCleanerUtils::OpenAssetsInSizeMap(const TArray<FAssetData>& Assets)
{
	TArray< FName > Packages;
	for (const FAssetData Asset : Assets)
	{
		Packages.AddUnique(FName(Asset.PackageName));
	}
	IAssetManagerEditorModule& Module = FModuleManager::LoadModuleChecked< IAssetManagerEditorModule >("AssetManagerEditor");
	Module.OpenSizeMapUI(Packages);
}

void AssetsCleanerUtils::OpenAssetsInRefViewer(const TArray<FAssetData>& Assets)
{
	TArray< FName > Packages;
	for (const FAssetData Asset : Assets)
	{
		Packages.AddUnique(FName(Asset.PackageName));
	}
	IAssetManagerEditorModule& Module = FModuleManager::LoadModuleChecked< IAssetManagerEditorModule >("AssetManagerEditor");
	Module.OpenReferenceViewerUI(Packages);
}

void AssetsCleanerUtils::OpenAssetsInAudit(const TArray<FAssetData>& Assets)
{
	TArray< FName > Packages;
	for (const FAssetData Asset : Assets)
	{
		Packages.AddUnique(FName(Asset.PackageName));
	}
	IAssetManagerEditorModule& Module = FModuleManager::LoadModuleChecked< IAssetManagerEditorModule >("AssetManagerEditor");
	Module.OpenAssetAuditUI(Packages);
}

TArray<FAssetData> AssetsCleanerUtils::CheckAssetsCanBeDeleted(const TArray<FAssetData>& Assets)
{
	TArray<FAssetData> FailedAssets;
	if (Assets.Num() > 0)
	{
		if (ISourceControlModule::Get().IsEnabled())
		{
			for (FAssetData Asset : Assets)
			{
				const FSourceControlState State = USourceControlHelpers::QueryFileState(
					USourceControlHelpers::PackageFilename(Asset.PackageName.ToString()));
				if (!State.bIsSourceControlled || State.bCanDelete ||State.bCanRevert)
				{
					//DELETE IS POSSIBLE
				} else
				{
					FailedAssets.Add(Asset);
				}
			}
		}
		else
		{
			for (FAssetData Asset : Assets)
			{
				FString PackageFileName = USourceControlHelpers::PackageFilename(Asset.PackageName.ToString());
				if (IFileManager::Get().FileExists(*PackageFileName) && IFileManager::Get().IsReadOnly(*PackageFileName))
				{
					//UE_LOG(LogAssetTools, Warning, TEXT("FAssetRenameManager::AutoCheckOut: package %s is read-only, will not make writable"), *PackageFilename);
					FailedAssets.Add(Asset);
				}
				else
				{
					//DELETE POSSIBLE
				}
			}
		}
	}

	return FailedAssets;
}

bool AssetsCleanerUtils::MakeAssetsDeletable(const TArray<FAssetData>& Assets)
{
	bool bHasAnyFailed = false;
	for (FAssetData Asset : Assets)
	{
		FString PackageName = USourceControlHelpers::PackageFilename(Asset.PackageName.ToString());
		if (USourceControlHelpers::IsEnabled())
		{
			if (!USourceControlHelpers::CheckOutFile(PackageName))
			{
				bHasAnyFailed = true;
			}
		} else
		{
			if (IFileManager::Get().FileExists(*PackageName) && IFileManager::Get().IsReadOnly(*PackageName))
			{
				
				if (!FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*PackageName, false))
				{
					bHasAnyFailed = true;
				}
			}
		}		
	}
	return bHasAnyFailed;
}

bool AssetsCleanerUtils::CheckAssetUsedInProjectSettings(const FAssetData& AssetData)
{
	const bool bShowingLevels = GetDefault<UAssetsCleanerSettings>()->bShowLevels;
	
	if ((bShowingLevels && AssetData.AssetClass == UWorld::StaticClass()->GetFName()) || AssetData.AssetClass == UBlueprint::StaticClass()->GetFName()) //we only need to check for levels or gamemode/gameinstance classes
		{
			TArray<FString>GameSettings;
			if (GConfig->GetSection(TEXT("/Script/EngineSettings.GameMapsSettings"), GameSettings, GEngineIni))
			{
				for (auto Entry : GameSettings)
				{
					if (Entry.Contains(AssetData.PackageName.ToString()))
					{
						return true;
					}
				}
			}
		}
	if (bShowingLevels)
	{
		if (AssetData.AssetClass == UWorld::StaticClass()->GetFName())
		{
			for (auto Path : GetDefault<UProjectPackagingSettings>()->MapsToCook)
			{				
				if (Path.FilePath.Contains(AssetData.PackageName.ToString()))
				{
					return true;
				}
			}		
			for (auto Path : GetDefault<UProjectPackagingSettings>()->MapsToCook)
			{				
				if (Path.FilePath.Contains(AssetData.PackageName.ToString()))
				{
					return true;
				}
			}
		}
	}
	return false;
}

void AssetsCleanerUtils::DeleteAssetsEmptyDirectories(const TArray<FAssetData>& Assets)
{
	if (!GetDefault<UAssetsCleanerSettings>()->bDeleteEmptyFolders)
	{
		return;
	}
	// Load the asset registry module
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FString>FoldersToDelete;
	for (const FAssetData Asset :Assets)
	{
		const FName AssetFolder = Asset.PackagePath;
		// Form a filter from the paths
		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.PackagePaths.Add(AssetFolder);
		TArray<FAssetData> AssetDataList;
		// Query for a list of assets in the selected paths
		AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);
		if (AssetDataList.Num() < 1)
		{
			FoldersToDelete.AddUnique(AssetFolder.ToString());
		}
	}
	bool DeleteFailed = false;
	for (FString Folder : FoldersToDelete)
	{
		if (!DeleteEmptyFolderFromDisk(Folder))
		{
			DeleteFailed = true;
		}
	}
	if (DeleteFailed)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to delete folders that were thought to be empty"));
	}

}

bool AssetsCleanerUtils::DeleteEmptyFolderFromDisk(const FString& PathToDelete)
{

	struct FEmptyFolderVisitor : public IPlatformFile::FDirectoryVisitor
	{
		bool bIsEmpty;

		FEmptyFolderVisitor()
			: bIsEmpty(true)
		{
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if (!bIsDirectory)
			{
				bIsEmpty = false;
				return false; // abort searching
			}

			return true; // continue searching
		}
	};

	FString PathToDeleteOnDisk;
	if (FPackageName::TryConvertLongPackageNameToFilename(PathToDelete, PathToDeleteOnDisk))
	{
		// Look for files on disk in case the folder contains things not tracked by the asset registry
		FEmptyFolderVisitor EmptyFolderVisitor;
		IFileManager::Get().IterateDirectoryRecursively(*PathToDeleteOnDisk, EmptyFolderVisitor);

		if (EmptyFolderVisitor.bIsEmpty)
		{
			return IFileManager::Get().DeleteDirectory(*PathToDeleteOnDisk, false, true);
		}
	}

	return false;

}

void AssetsCleanerUtils::MoveAssets(const TArray<UObject*>& Assets, const FString& DestPath, const FString& SourcePath)
{
	check(DestPath.Len() > 0);

	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25 //UE4.25+
	if (!AssetToolsModule.Get().GetWritableFolderBlacklist()->PassesStartsWithFilter(DestPath))
	{
		AssetToolsModule.Get().NotifyBlockedByWritableFolderFilter();
		return;
	}
#endif
#if ENGINE_MAJOR_VERSION >= 5 //UE5
	if (!AssetToolsModule.Get().GetWritableFolderPermissionList()->PassesStartsWithFilter(DestPath))
	{
		AssetToolsModule.Get().NotifyBlockedByWritableFolderFilter();
		return;
	}
#endif
	TArray<FAssetRenameData> AssetsAndNames;
	for ( auto AssetIt = Assets.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		UObject* Asset = *AssetIt;

		if ( !ensure(Asset) )
		{
			continue;
		}

		FString PackagePath;
		FString ObjectName = Asset->GetName();

		if ( SourcePath.Len() )
		{
			const FString CurrentPackageName = Asset->GetOutermost()->GetName();

			// This is a relative operation
			if ( !ensure(CurrentPackageName.StartsWith(SourcePath)) )
			{
				continue;
			}
				
			// Collect the relative path then use it to determine the new location
			// For example, if SourcePath = /Game/MyPath and CurrentPackageName = /Game/MyPath/MySubPath/MyAsset
			//     /Game/MyPath/MySubPath/MyAsset -> /MySubPath

			const int32 ShortPackageNameLen = FPackageName::GetLongPackageAssetName(CurrentPackageName).Len();
			const int32 RelativePathLen = CurrentPackageName.Len() - ShortPackageNameLen - SourcePath.Len() - 1; // -1 to exclude the trailing "/"
			const FString RelativeDestPath = CurrentPackageName.Mid(SourcePath.Len(), RelativePathLen);

			PackagePath = DestPath + RelativeDestPath;
		}
		else
		{
			// Only a DestPath was supplied, use it
			PackagePath = DestPath;
		}

		new(AssetsAndNames) FAssetRenameData(Asset, PackagePath, ObjectName);
	}

	if ( AssetsAndNames.Num() > 0 )
	{
		AssetToolsModule.Get().RenameAssetsWithDialog(AssetsAndNames);
	}	
}

bool AssetsCleanerUtils::WarnIfTooManyAssetsToDelete(const TArray<FAssetData>& Assets)
{
	int64 TotalDiskSize = 0;
	IAssetManagerEditorModule& AMModule = FModuleManager::LoadModuleChecked< IAssetManagerEditorModule >("AssetManagerEditor");
	AMModule.GetCurrentRegistrySource();//TODO RefreshRegistryData used to crash on 4.23 but doing this check somehow makes it not crash and work with columns
	for (auto Asset : Assets)
	{
		int64 AssetSize;
		if (AMModule.GetIntegerValueForCustomColumn(Asset, AMModule.DiskSizeName,AssetSize))
		{
			TotalDiskSize += AssetSize;
		}
	}
	constexpr int64 NumThreshold = 100;
	constexpr int64 SizeThreshold =  3221225472; //=3Gb //WTF, doing SizeThreshold = 2 * 1024 * 1024 * 1024 ends up being NEGATIVE 2147483648 ... WTF!!
	if (Assets.Num() >NumThreshold || TotalDiskSize > SizeThreshold) 
	{
		FString Message = "This many assets (or size) may take a long time to delete.";
		Message += LINE_TERMINATOR;
		Message += "Unreal loads assets before deleting, and can freeze the editor on some computers.";
		Message += LINE_TERMINATOR;
		Message += "It is recommended to delete smaller amount/size of assets at a time.";
		Message += LINE_TERMINATOR;
		Message += LINE_TERMINATOR;
		Message += "Delete anyway?";
		const FText Title = FText::FromString("Slow delete operation");

		return (FMessageDialog::Open(EAppMsgType::OkCancel,FText::FromString(Message), &Title) == EAppReturnType::Ok);		
	}
	 return true;
	
}


