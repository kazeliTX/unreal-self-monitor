#include "Commands/UnrealMCPDiagnosticsCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

// Editor / Viewport
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Engine/GameViewportClient.h"
#include "SceneView.h"
#include "EditorSubsystem.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "Subsystems/EditorActorSubsystem.h"
#endif
#include "Engine/Selection.h"
#include "EngineUtils.h"

// HAL / IO
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"

// Module management (for LiveCoding status)
#include "Modules/ModuleManager.h"

FUnrealMCPDiagnosticsCommands::FUnrealMCPDiagnosticsCommands()
{
}

void FUnrealMCPDiagnosticsCommands::RegisterCommands(FMCPCommandRegistry& Registry)
{
    // Visual perception
    Registry.RegisterCommand(TEXT("get_viewport_camera_info"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetViewportCameraInfo(P); });
    Registry.RegisterCommand(TEXT("get_actor_screen_position"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetActorScreenPosition(P); });
    Registry.RegisterCommand(TEXT("highlight_actor"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleHighlightActor(P); });

    // Hot-reload / LiveCoding
    Registry.RegisterCommand(TEXT("trigger_hot_reload"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleTriggerHotReload(P); });
    Registry.RegisterCommand(TEXT("get_live_coding_status"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetLiveCodingStatus(P); });

    // Source file access
    Registry.RegisterCommand(TEXT("get_source_file"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetSourceFile(P); });
    Registry.RegisterCommand(TEXT("modify_source_file"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleModifySourceFile(P); });

    // Engine / project path
    Registry.RegisterCommand(TEXT("get_engine_path"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetEnginePath(P); });
}

// ---------------------------------------------------------------------------
// Visual perception
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPDiagnosticsCommands::HandleGetViewportCameraInfo(
    const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor is not available"));
    }

    FLevelEditorViewportClient* ViewportClient = nullptr;
    for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
    {
        if (Client && Client->IsPerspective())
        {
            ViewportClient = Client;
            break;
        }
    }

    if (!ViewportClient)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No perspective viewport found"));
    }

    FVector Location = ViewportClient->GetViewLocation();
    FRotator Rotation = ViewportClient->GetViewRotation();
    float FOV = ViewportClient->ViewFOV;

    TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
    LocObj->SetNumberField(TEXT("x"), Location.X);
    LocObj->SetNumberField(TEXT("y"), Location.Y);
    LocObj->SetNumberField(TEXT("z"), Location.Z);

    TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
    RotObj->SetNumberField(TEXT("pitch"), Rotation.Pitch);
    RotObj->SetNumberField(TEXT("yaw"), Rotation.Yaw);
    RotObj->SetNumberField(TEXT("roll"), Rotation.Roll);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetObjectField(TEXT("location"), LocObj);
    ResultObj->SetObjectField(TEXT("rotation"), RotObj);
    ResultObj->SetNumberField(TEXT("fov"), FOV);

    FIntPoint ViewportSize = ViewportClient->Viewport ? ViewportClient->Viewport->GetSizeXY() : FIntPoint(0, 0);
    ResultObj->SetNumberField(TEXT("viewport_width"), ViewportSize.X);
    ResultObj->SetNumberField(TEXT("viewport_height"), ViewportSize.Y);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPDiagnosticsCommands::HandleGetActorScreenPosition(
    const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor is not available"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find actor
    AActor* TargetActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Find active perspective viewport
    FLevelEditorViewportClient* ViewportClient = nullptr;
    for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
    {
        if (Client && Client->IsPerspective() && Client->Viewport)
        {
            ViewportClient = Client;
            break;
        }
    }
    if (!ViewportClient)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No perspective viewport found"));
    }

    FViewport* Viewport = ViewportClient->Viewport;
    FIntPoint ViewportSize = Viewport->GetSizeXY();

    // Build scene view to project world position
    FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
        Viewport, ViewportClient->GetScene(), ViewportClient->EngineShowFlags)
        .SetRealtimeUpdate(true));

    FSceneView* SceneView = ViewportClient->CalcSceneView(&ViewFamily);
    if (!SceneView)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not calculate scene view"));
    }

    FVector WorldPos = TargetActor->GetActorLocation();
    FVector2D ScreenPos;
    bool bProjected = SceneView->WorldToPixel(WorldPos, ScreenPos);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("is_visible"), bProjected);
    ResultObj->SetNumberField(TEXT("screen_x"), ScreenPos.X);
    ResultObj->SetNumberField(TEXT("screen_y"), ScreenPos.Y);
    ResultObj->SetNumberField(TEXT("viewport_width"), ViewportSize.X);
    ResultObj->SetNumberField(TEXT("viewport_height"), ViewportSize.Y);

    // Normalized 0-1 coords
    if (ViewportSize.X > 0 && ViewportSize.Y > 0)
    {
        ResultObj->SetNumberField(TEXT("normalized_x"), ScreenPos.X / ViewportSize.X);
        ResultObj->SetNumberField(TEXT("normalized_y"), ScreenPos.Y / ViewportSize.Y);
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPDiagnosticsCommands::HandleHighlightActor(
    const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor is not available"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
    }

    AActor* TargetActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Select the actor
    GEditor->SelectNone(false, true);
    GEditor->SelectActor(TargetActor, true, true, true);

    // Move viewport camera to it
    GEditor->MoveViewportCamerasToActor(*TargetActor, false);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("actor"), TargetActor->GetActorLabel());
    return ResultObj;
}

// ---------------------------------------------------------------------------
// Hot-reload / LiveCoding
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPDiagnosticsCommands::HandleTriggerHotReload(
    const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor is not available"));
    }

    // Try LiveCoding module first (UE5 preferred path)
    IModuleInterface* LiveCodingModule = FModuleManager::Get().GetModule(TEXT("LiveCoding"));
    if (LiveCodingModule)
    {
        // Trigger via console command — avoids needing LiveCoding headers
        UWorld* World = GEditor->GetEditorWorldContext().World();
        GEditor->Exec(World, TEXT("LiveCoding.Compile"));

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetStringField(TEXT("method"), TEXT("live_coding"));
        ResultObj->SetStringField(TEXT("message"), TEXT("LiveCoding compile triggered"));
        return ResultObj;
    }

    // Fallback: hotreload console command
    UWorld* World = GEditor->GetEditorWorldContext().World();
    GEditor->Exec(World, TEXT("HotReload"));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("method"), TEXT("hot_reload_fallback"));
    ResultObj->SetStringField(TEXT("message"), TEXT("HotReload command issued (LiveCoding not available)"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPDiagnosticsCommands::HandleGetLiveCodingStatus(
    const TSharedPtr<FJsonObject>& Params)
{
    bool bModuleLoaded = FModuleManager::Get().IsModuleLoaded(TEXT("LiveCoding"));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("live_coding_available"), bModuleLoaded);

    if (bModuleLoaded)
    {
        ResultObj->SetStringField(TEXT("status"), TEXT("available"));
    }
    else
    {
        ResultObj->SetStringField(TEXT("status"), TEXT("not_loaded"));
    }

    return ResultObj;
}

// ---------------------------------------------------------------------------
// Source file access
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPDiagnosticsCommands::HandleGetSourceFile(
    const TSharedPtr<FJsonObject>& Params)
{
    FString RelativePath;
    if (!Params->TryGetStringField(TEXT("path"), RelativePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    // Resolve relative to project dir if not absolute
    FString AbsolutePath = RelativePath;
    if (FPaths::IsRelative(RelativePath))
    {
        AbsolutePath = FPaths::ConvertRelativePathToFull(
            FPaths::ProjectDir() / RelativePath);
    }

    if (!FPaths::FileExists(AbsolutePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("File not found: %s"), *AbsolutePath));
    }

    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *AbsolutePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to read file: %s"), *AbsolutePath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("path"), AbsolutePath);
    ResultObj->SetStringField(TEXT("content"), FileContent);
    ResultObj->SetNumberField(TEXT("size"), FileContent.Len());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPDiagnosticsCommands::HandleModifySourceFile(
    const TSharedPtr<FJsonObject>& Params)
{
    FString RelativePath;
    if (!Params->TryGetStringField(TEXT("path"), RelativePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    FString NewContent;
    if (!Params->TryGetStringField(TEXT("content"), NewContent))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'content' parameter"));
    }

    // Resolve path
    FString AbsolutePath = RelativePath;
    if (FPaths::IsRelative(RelativePath))
    {
        AbsolutePath = FPaths::ConvertRelativePathToFull(
            FPaths::ProjectDir() / RelativePath);
    }

    // Create backup of existing file
    FString BackupPath;
    if (FPaths::FileExists(AbsolutePath))
    {
        FDateTime Now = FDateTime::Now();
        FString Timestamp = FString::Printf(TEXT("%04d%02d%02d_%02d%02d%02d"),
            Now.GetYear(), Now.GetMonth(), Now.GetDay(),
            Now.GetHour(), Now.GetMinute(), Now.GetSecond());
        BackupPath = AbsolutePath + TEXT(".bak.") + Timestamp;

        FString ExistingContent;
        if (FFileHelper::LoadFileToString(ExistingContent, *AbsolutePath))
        {
            FFileHelper::SaveStringToFile(ExistingContent, *BackupPath);
        }
    }

    // Write new content
    if (!FFileHelper::SaveStringToFile(NewContent, *AbsolutePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to write file: %s"), *AbsolutePath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("path"), AbsolutePath);
    ResultObj->SetNumberField(TEXT("bytes_written"), NewContent.Len());
    if (!BackupPath.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("backup_path"), BackupPath);
    }
    return ResultObj;
}

// ---------------------------------------------------------------------------
// Engine / project path discovery
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPDiagnosticsCommands::HandleGetEnginePath(
    const TSharedPtr<FJsonObject>& Params)
{
    FString EngineDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
    FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FString ProjectFile = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());

    // Build the UBT batch script path (Win64)
    FString UBTBatchScript = FPaths::Combine(EngineDir,
        TEXT("Build/BatchFiles/Build.bat"));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("engine_dir"), EngineDir);
    ResultObj->SetStringField(TEXT("project_dir"), ProjectDir);
    ResultObj->SetStringField(TEXT("project_file"), ProjectFile);
    ResultObj->SetStringField(TEXT("ubt_batch_script"), UBTBatchScript);
    ResultObj->SetBoolField(TEXT("ubt_exists"), FPaths::FileExists(UBTBatchScript));
    return ResultObj;
}
