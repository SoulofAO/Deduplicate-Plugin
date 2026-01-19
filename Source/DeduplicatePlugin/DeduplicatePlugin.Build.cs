/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */

using UnrealBuildTool;

public class DeduplicatePlugin : ModuleRules
{
	public DeduplicatePlugin(ReadOnlyTargetRules Target) : base(Target)
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
				"Engine",
				"UnrealEd",
				"EditorStyle",
				"EditorWidgets",
				"ToolMenus",
				"AssetTools",
				"AssetRegistry",
				"ContentBrowser",
				"Slate",
				"SlateCore",
				"ToolWidgets",
                "RenderCore",
				"RHI",
				"Json",
				"Niagara"
            }
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"EditorStyle",
				"EditorWidgets",
				"ToolMenus",
				"AssetTools",
				"AssetRegistry",
				"ContentBrowser",
				"PropertyEditor",
				"DetailCustomizations",
				"BlueprintGraph",
				"KismetCompiler",
				"MaterialEditor",
				"ToolMenus",
				"EditorSubsystem",
				"Slate",
                "UMG",
				"UnrealEd",
                "SourceControl"
            }
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
