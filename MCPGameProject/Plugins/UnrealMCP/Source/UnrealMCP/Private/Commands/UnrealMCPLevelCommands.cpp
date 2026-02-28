#include "Commands/UnrealMCPLevelCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorLevelLibrary.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Misc/PackageName.h"

FUnrealMCPLevelCommands::FUnrealMCPLevelCommands()
{
}

void FUnrealMCPLevelCommands::RegisterCommands(FMCPCommandRegistry& Registry)
{
    Registry.RegisterCommand(TEXT("new_level"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleNewLevel(P); });
    Registry.RegisterCommand(TEXT("open_level"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleOpenLevel(P); });
    Registry.RegisterCommand(TEXT("save_current_level"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleSaveCurrentLevel(P); });
    Registry.RegisterCommand(TEXT("save_all_levels"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleSaveAllLevels(P); });
    Registry.RegisterCommand(TEXT("get_current_level_name"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetCurrentLevelName(P); });
}

TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleNewLevel(const TSharedPtr<FJsonObject>& Params)
{
    // Optional: asset_path for the new level (e.g. "/Game/Maps/MyLevel")
    FString AssetPath;
    bool bHasPath = Params->TryGetStringField(TEXT("asset_path"), AssetPath) && !AssetPath.IsEmpty();

    bool bSuccess = false;
    if (bHasPath)
    {
        // Create level at the specified path
        bSuccess = UEditorLevelLibrary::NewLevel(AssetPath);
    }
    else
    {
        // Create a default empty level (prompts if needed - use FEditorFileUtils)
        bSuccess = FEditorFileUtils::SaveDirtyPackages(false, true, false, false, false, false);
        bSuccess = true;
        // Create new unsaved level
        GEditor->CreateNewMapForEditing();
    }

    if (!bSuccess && bHasPath)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to create new level at '%s'"), *AssetPath));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    if (World)
    {
        Result->SetStringField(TEXT("level_name"), World->GetName());
        Result->SetStringField(TEXT("level_path"), World->GetPathName());
    }
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    bool bSuccess = UEditorLevelLibrary::LoadLevel(AssetPath);
    if (!bSuccess)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to open level '%s'. Make sure the path is correct (e.g. '/Game/Maps/MyLevel')."), *AssetPath));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    if (World)
    {
        Result->SetStringField(TEXT("level_name"), World->GetName());
        Result->SetStringField(TEXT("level_path"), World->GetPathName());
    }
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
    bool bSuccess = UEditorLevelLibrary::SaveCurrentLevel();
    if (!bSuccess)
    {
        // Fall back to FEditorFileUtils if the level has no saved path yet
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (World && World->GetCurrentLevel())
        {
            bSuccess = FEditorFileUtils::SaveLevel(World->GetCurrentLevel());
        }
    }

    if (!bSuccess)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to save current level. The level may not have a file path yet."));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    if (World)
    {
        Result->SetStringField(TEXT("level_name"), World->GetName());
        Result->SetStringField(TEXT("level_path"), World->GetPathName());
    }
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleSaveAllLevels(const TSharedPtr<FJsonObject>& Params)
{
    bool bSuccess = UEditorLevelLibrary::SaveAllDirtyLevels();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    if (!bSuccess)
    {
        Result->SetStringField(TEXT("message"), TEXT("Some levels may not have been saved (no file path assigned)."));
    }
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleGetCurrentLevelName(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world found"));
    }

    FString LevelName = World->GetName();
    FString LevelPath = World->GetPathName();

    // Strip trailing _C or sub-object suffix
    FString PackageName = World->GetOutermost()->GetName();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("level_name"), LevelName);
    Result->SetStringField(TEXT("level_path"), LevelPath);
    Result->SetStringField(TEXT("package_name"), PackageName);
    return Result;
}
