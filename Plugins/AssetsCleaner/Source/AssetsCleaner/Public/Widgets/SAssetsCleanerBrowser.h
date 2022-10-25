// Copyright 2019-2022 Chris Garnier (The Tool Shed). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserDelegates.h"

class SAssetView;
/**
 * Main widget for Assets Cleaner tool window
 */
class ASSETSCLEANER_API SAssetsCleanerBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetsCleanerBrowser){}
	SLATE_ARGUMENT(FARFilter, InitialFilter);
	SLATE_ARGUMENT(FSetARFilterDelegate*, SetFilterDelegate);
	SLATE_ARGUMENT(FRefreshAssetViewDelegate*, RefreshAssetViewDelegate)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
private:
	//Build right click menu
	TSharedPtr<SWidget> OnGetContextMenu(const TArray<FAssetData>& Assets);

private:
	//UI Commands to register
	TSharedPtr<FUICommandList> Commands;

	//Delegate to refresh, only used by the asset picker itself to refresh after a right click menu action
	FRefreshAssetViewDelegate InternalRefreshDelegate;


};
