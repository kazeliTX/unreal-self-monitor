// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMCP : ModuleRules
{
	public UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// IWYUSupport was introduced in UE5.5 and replaces the deprecated bEnforceIWYU.
		// We set neither so the same Build.cs compiles on both UE4 and UE5.

		// VS2022 14.44+ treats C4459 (local variable shadowing global declaration) as an error.
		// This is triggered by the 'BufferSize' template parameter in StringConv.h, an engine
		// header issue that affects every file using JSON serialization in this module.
		// UE5.3+ renamed bEnableShadowVariableWarnings → ShadowVariableWarningLevel.
		ShadowVariableWarningLevel = WarningLevel.Off;

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
				"JsonUtilities"
			}
		);

		// DeveloperSettings is part of the Engine module in UE4; it became its own module in UE5.
		if (Target.Version.MajorVersion >= 5)
		{
			PublicDependencyModuleNames.Add("DeveloperSettings");
		}

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
				"LiveCoding",       // For hot-reload / compile status queries
				"EngineSettings"    // For UGeneralProjectSettings
			}
		);

		// EditorStyle module provides FEditorStyle in UE4; UE5 uses FAppStyle from Slate.
		if (Target.Version.MajorVersion < 5)
		{
			PrivateDependencyModuleNames.Add("EditorStyle");
		}

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
