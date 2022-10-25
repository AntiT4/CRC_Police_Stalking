// Copyright 2019-2022 Chris Garnier (The Tool Shed). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreUObject/Public/UObject/Object.h"
#include "AssetsCleanerSettings.generated.h"

/**
 * Settings class for Assets Cleaner plugin
 */
UCLASS(config=EditorPerProjectUserSettings)
class ASSETSCLEANER_API UAssetsCleanerSettings : public UObject
{
	GENERATED_BODY()

public:
	//Should we include levels as well (knowing it's not possible to reliably check for references)
	UPROPERTY(Config)
	bool bShowLevels;

	//Also delete folders if they're empty when deleting assets
	UPROPERTY(Config)
	bool bDeleteEmptyFolders;


};
