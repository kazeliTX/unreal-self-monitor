// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMCP : ModuleRules
{
	public UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// IWYUSupport was introduced in UE5.5 and replaces the deprecated bEnforceIWYU.
		// We set neither so the same Build.cs compiles on both UE4 and UE5.

		PublicIncludePaths.AddRange(
			new string[] {
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Networking",
				"Sockets",
				"HTTP",
				"Json",
				"JsonUtilities",
				"DeveloperSettings"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"Slate",
				"SlateCore",
				"UMG",
				"Kismet",
				"KismetCompiler",
				"BlueprintGraph",
				"Projects",
				"AssetRegistry",
				"AssetTools",       // For IAssetTools (DataTable/InputAction creation)
				"LevelEditor",      // For level management commands
				"GameplayTags",     // For tag operations
				"LiveCoding"        // For hot-reload / compile status queries
			}
		);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"PropertyEditor",      // For widget property editing
					"ToolMenus",           // For editor UI
					"UMGEditor"            // For WidgetBlueprint.h and other UMG editor functionality
				}
			);

			// BlueprintEditorLibrary is a UE5-only module.
			if (Target.Version.MajorVersion >= 5)
			{
				PrivateDependencyModuleNames.Add("BlueprintEditorLibrary");
			}
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
