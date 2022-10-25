// Copyright 2019-2022 Chris Garnier (The Tool Shed). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserDelegates.h"
#include "SlateCore/Public/Widgets/SCompoundWidget.h"

class SAssetsCleanerBrowser;

/**
 * Main widget for Assets Cleaner
 */

class ASSETSCLEANER_API SAssetsCleaner : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetsCleaner)
		{
		}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	
	~SAssetsCleaner();
private:
	//Delegate for refresh button
	void OnRefreshTriggered();

	//refresh asset views with new data
	void RefreshView();


	//builds the main toolbar
	TSharedRef<SWidget> BuildToolbar();
	
	//Called to update the filter after we've changed settings that could change assets filter
	void UpdateFilter();	

private:

	//Dlegate passed on to an asset view that we execute when refresh is hit
	FRefreshAssetViewDelegate RefreshAssetViewDelegate;
	
	//Dlegate passed on to an asset view that we execute to change its filter
	FSetARFilterDelegate SetFilterDelegate;

};
