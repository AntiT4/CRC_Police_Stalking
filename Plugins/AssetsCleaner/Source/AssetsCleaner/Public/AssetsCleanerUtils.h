// Copyright 2019-2022 Chris Garnier (The Tool Shed). All Rights Reserved.

#pragma once


#include "Widgets/SAssetsCleanerBrowser.h"

class ASSETSCLEANER_API AssetsCleanerUtils
{
	friend class SAssetsCleanerBrowser;


private:
	//Used by delegate on asset view to see if an asset should be filtered out
	static bool ShouldFilterOutAsset(const FAssetData& AssetData);

	//Util to get an asset on disk in explorer
	static void LocateAssetsOnDisk(const TArray<FAssetData>& Assets);

	//Delete selected assets
	static void DeleteAssets(const TArray<FAssetData>& Assets);

	//Custom deletion method, allows cancelling, and loading only one asset at a time.
	static void SmartDeleteAssets(const TArray<FAssetData>& Assets);

	//Custom deletion method, very dangerous as it directly deletes the file on disk without loading the asset first or checking anything.
	static void ForceDeleteAssetFilesOnDisk(const TArray<FAssetData>& Assets);

	//Open a path picker dialog, then move selected assets to that folder.
	static void MoveAssetsToPickedFolder(const TArray<FAssetData>& Assets);

	//Shows the selected assets in the size map
	static void OpenAssetsInSizeMap(const TArray<FAssetData>& Assets);
	
	//Shows the selected assets in the reference viewer
	static void OpenAssetsInRefViewer(const TArray<FAssetData>& Assets);
	
	//Shows the selected assets in the asset audit
	static void OpenAssetsInAudit(const TArray<FAssetData>& Assets);



private:
	
	//Verify that assets' files can be deleted in source control or read only disk status ,, returns list of assets that cannot be deleted
	static TArray<FAssetData> CheckAssetsCanBeDeleted(const TArray<FAssetData>& Assets);

	//Do a Checkout so files can be deleted without having a prompt for every single file. if no SCC just marks as writable if read only
	//Returns true if any file failed to be made deletable
	static bool MakeAssetsDeletable(const TArray<FAssetData>& Assets);

	//checks all relevant project settings sections for any string that would match this asset
	static bool CheckAssetUsedInProjectSettings(const FAssetData& Asset);

	//Delete all directories that are empty from the list of assets
	static void DeleteAssetsEmptyDirectories(const TArray<FAssetData>& Assets);

	//Copied over from AssetViewUtils since the class doesn't exist < 426 and nothing else public does it
	static bool DeleteEmptyFolderFromDisk(const FString& PathToDelete);

	//Copied over from AssetViewUtils since the class doesn't exist < 426 and nothing else public does it
	static void MoveAssets(const TArray<UObject*>& Assets, const FString& DestPath, const FString& SourcePath = FString());

	//Check assets to delete and warn if it's too many/too big. Returns true if user said ok to continue
	static bool WarnIfTooManyAssetsToDelete(const TArray<FAssetData>& Assets);
};
