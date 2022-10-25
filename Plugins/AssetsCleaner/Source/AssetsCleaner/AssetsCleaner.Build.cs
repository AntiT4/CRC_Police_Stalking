// Copyright 2019-2022 Chris Garnier (The Tool Shed). All Rights Reserved.

using UnrealBuildTool;

public class AssetsCleaner : ModuleRules
{
	public AssetsCleaner(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				
				// ... add other private include paths required here ...
            }
            );
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"SlateCore",
				"AssetRegistry",
                "UnrealEd",
                "AssetManagerEditor"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects", //for plugin manager
				"UnrealEd",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "EditorStyle",
                "SourceControl",
                "AssetTools",
                "WorkspaceMenuStructure",
				"ContentBrowser"
                // ... add private dependencies that you statically link with here ...	
			}
			);
		
		//UE4 specific version modules
		if (base.Target.Version.MajorVersion == 4)
		{
			if (base.Target.Version.MinorVersion >= 26)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"ContentBrowserData" //introduced in 426
					}
				);			
			}
		}
		
		//UE5 specific modules
		if (base.Target.Version.MajorVersion > 4)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DeveloperToolSettings", //new in 5.0
					"ContentBrowserData" //introduced in 426

				}
			);
		}
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
            }
			);
	}
}
