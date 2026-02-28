#include "Commands/UnrealMCPTestCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/StaticMesh.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "AssetRegistry/AssetRegistryModule.h"
#else
#include "AssetRegistryModule.h"
#endif
#include "Kismet2/CompilerResultsLog.h"
#include "UnrealMCPCompat.h"

FUnrealMCPTestCommands::FUnrealMCPTestCommands()
{
}

void FUnrealMCPTestCommands::RegisterCommands(FMCPCommandRegistry& Registry)
{
    Registry.RegisterCommand(TEXT("validate_blueprint"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleValidateBlueprint(P); });
    Registry.RegisterCommand(TEXT("run_level_validation"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleRunLevelValidation(P); });
}

// ---------------------------------------------------------------------------
// validate_blueprint
// Params: blueprint_name (string), path (string, optional)
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPTestCommands::HandleValidateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));

    FString AssetPath;
    Params->TryGetStringField(TEXT("path"), AssetPath);

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprintByName(BlueprintName, AssetPath);
    if (!Blueprint)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));

    // Compile with a results log to capture errors/warnings
    FCompilerResultsLog ResultsLog;
    ResultsLog.bSilentMode = true;  // suppress editor notifications
    FKismetEditorUtilities::CompileBlueprint(Blueprint,
        EBlueprintCompileOptions::SkipGarbageCollection, &ResultsLog);

    // Compile status string
    FString StatusStr;
    bool bIsValid = false;
    switch (Blueprint->Status)
    {
        case EBlueprintStatus::BS_UpToDate:
            StatusStr = TEXT("UpToDate"); bIsValid = true; break;
        case EBlueprintStatus::BS_UpToDateWithWarnings:
            StatusStr = TEXT("UpToDateWithWarnings"); bIsValid = true; break;
        case EBlueprintStatus::BS_Dirty:
            StatusStr = TEXT("Dirty"); bIsValid = false; break;
        case EBlueprintStatus::BS_Error:
            StatusStr = TEXT("Error"); bIsValid = false; break;
        default:
            StatusStr = TEXT("Unknown"); bIsValid = false; break;
    }

    // Collect error / warning messages from the compiler log
    TArray<TSharedPtr<FJsonValue>> ErrorArray;
    TArray<TSharedPtr<FJsonValue>> WarningArray;
    for (const TSharedRef<FTokenizedMessage>& Msg : ResultsLog.Messages)
    {
        FString Text = Msg->ToText().ToString();
        switch (Msg->GetSeverity())
        {
            case EMessageSeverity::Error:
            case EMessageSeverity::CriticalError:
                ErrorArray.Add(MakeShared<FJsonValueString>(Text));
                break;
            case EMessageSeverity::Warning:
                WarningArray.Add(MakeShared<FJsonValueString>(Text));
                break;
            default:
                break;
        }
    }

    // Count total nodes across all event graphs and function graphs
    int32 NodeCount = 0;
    for (const UEdGraph* Graph : Blueprint->UbergraphPages)
        if (Graph) NodeCount += Graph->Nodes.Num();
    for (const UEdGraph* Graph : Blueprint->FunctionGraphs)
        if (Graph) NodeCount += Graph->Nodes.Num();

    int32 VariableCount = Blueprint->NewVariables.Num();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Result->SetBoolField(TEXT("is_valid"), bIsValid);
    Result->SetStringField(TEXT("compile_status"), StatusStr);
    Result->SetNumberField(TEXT("error_count"), (double)ResultsLog.NumErrors);
    Result->SetNumberField(TEXT("warning_count"), (double)ResultsLog.NumWarnings);
    Result->SetArrayField(TEXT("errors"), ErrorArray);
    Result->SetArrayField(TEXT("warnings"), WarningArray);
    Result->SetNumberField(TEXT("node_count"), NodeCount);
    Result->SetNumberField(TEXT("variable_count"), VariableCount);
    return Result;
}

// ---------------------------------------------------------------------------
// run_level_validation
// Scans all actors in the current editor world for common issues.
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPTestCommands::HandleRunLevelValidation(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available"));

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));

    IAssetRegistry& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    TArray<TSharedPtr<FJsonValue>> ActorIssues;
    TArray<TSharedPtr<FJsonValue>> BrokenAssetRefs;
    TArray<TSharedPtr<FJsonValue>> UncompiledBlueprints;

    int32 TotalActors = 0;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || Actor->IsPendingKillPending())
            continue;

        ++TotalActors;
        const FString ActorName = Actor->GetActorLabel();

        // ── 1. Check if actor's blueprint class has compile errors ──────────
        UClass* ActorClass = Actor->GetClass();
        if (UBlueprintGeneratedClass* BPGenClass = Cast<UBlueprintGeneratedClass>(ActorClass))
        {
            UBlueprint* BP = Cast<UBlueprint>(BPGenClass->ClassGeneratedBy);
            if (BP)
            {
                if (BP->Status == EBlueprintStatus::BS_Error)
                {
                    // Add to uncompiled/errored blueprints list (deduplicated by path)
                    FString BPPath = BP->GetPathName();
                    bool bAlreadyListed = false;
                    for (const TSharedPtr<FJsonValue>& V : UncompiledBlueprints)
                        if (V->AsString() == BPPath) { bAlreadyListed = true; break; }
                    if (!bAlreadyListed)
                        UncompiledBlueprints.Add(MakeShared<FJsonValueString>(BPPath));

                    TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                    Issue->SetStringField(TEXT("actor"), ActorName);
                    Issue->SetStringField(TEXT("issue_type"), TEXT("blueprint_error"));
                    Issue->SetStringField(TEXT("detail"),
                        FString::Printf(TEXT("Blueprint '%s' has compile errors"), *BP->GetName()));
                    ActorIssues.Add(MakeShared<FJsonValueObject>(Issue));
                }
                else if (BP->Status == EBlueprintStatus::BS_Dirty)
                {
                    TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                    Issue->SetStringField(TEXT("actor"), ActorName);
                    Issue->SetStringField(TEXT("issue_type"), TEXT("blueprint_dirty"));
                    Issue->SetStringField(TEXT("detail"),
                        FString::Printf(TEXT("Blueprint '%s' is dirty (not compiled)"), *BP->GetName()));
                    ActorIssues.Add(MakeShared<FJsonValueObject>(Issue));
                }
            }
        }

        // ── 2. Check StaticMeshComponents for missing meshes ─────────────────
        TArray<UStaticMeshComponent*> MeshComponents;
        Actor->GetComponents<UStaticMeshComponent>(MeshComponents);
        for (const UStaticMeshComponent* MeshComp : MeshComponents)
        {
            if (!MeshComp) continue;
            const UStaticMesh* Mesh = MeshComp->GetStaticMesh();
            if (!Mesh)
            {
                TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                Issue->SetStringField(TEXT("actor"), ActorName);
                Issue->SetStringField(TEXT("issue_type"), TEXT("missing_mesh"));
                Issue->SetStringField(TEXT("detail"),
                    FString::Printf(TEXT("Component '%s' has no StaticMesh assigned"),
                        *MeshComp->GetName()));
                ActorIssues.Add(MakeShared<FJsonValueObject>(Issue));
            }
        }

        // ── 3. Check SkeletalMeshComponents for missing meshes ───────────────
        TArray<USkeletalMeshComponent*> SkelMeshComponents;
        Actor->GetComponents<USkeletalMeshComponent>(SkelMeshComponents);
        for (const USkeletalMeshComponent* SkelComp : SkelMeshComponents)
        {
            if (!SkelComp) continue;
#if ENGINE_MAJOR_VERSION >= 5
            if (!SkelComp->GetSkeletalMeshAsset())
#else
            if (!SkelComp->SkeletalMesh)
#endif
            {
                TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                Issue->SetStringField(TEXT("actor"), ActorName);
                Issue->SetStringField(TEXT("issue_type"), TEXT("missing_skeletal_mesh"));
                Issue->SetStringField(TEXT("detail"),
                    FString::Printf(TEXT("Component '%s' has no SkeletalMesh assigned"),
                        *SkelComp->GetName()));
                ActorIssues.Add(MakeShared<FJsonValueObject>(Issue));
            }
        }

        // ── 4. Check for orphaned scene components (no parent, not root) ─────
        TArray<USceneComponent*> SceneComponents;
        Actor->GetComponents<USceneComponent>(SceneComponents);
        for (const USceneComponent* SceneComp : SceneComponents)
        {
            if (!SceneComp) continue;
            if (SceneComp == Actor->GetRootComponent()) continue;
            if (!SceneComp->GetAttachParent())
            {
                TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                Issue->SetStringField(TEXT("actor"), ActorName);
                Issue->SetStringField(TEXT("issue_type"), TEXT("orphaned_component"));
                Issue->SetStringField(TEXT("detail"),
                    FString::Printf(TEXT("SceneComponent '%s' has no parent attachment"),
                        *SceneComp->GetName()));
                ActorIssues.Add(MakeShared<FJsonValueObject>(Issue));
            }
        }
    }

    // ── 5. Scan asset registry for broken redirectors ─────────────────────────
    TArray<FAssetData> AllAssets;
    AssetRegistry.GetAllAssets(AllAssets, /*bSkipARFilteredAssets=*/true);
    for (const FAssetData& Asset : AllAssets)
    {
        if (Asset.IsRedirector())
        {
            // A redirector that still exists means the target may have moved but the
            // redirector was never fixed up — flag it for the caller.
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
            BrokenAssetRefs.Add(MakeShared<FJsonValueString>(Asset.GetObjectPathString()));
#else
            BrokenAssetRefs.Add(MakeShared<FJsonValueString>(Asset.ObjectPath.ToString()));
#endif
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("total_actors"), TotalActors);
    Result->SetNumberField(TEXT("actors_with_issues_count"), ActorIssues.Num());
    Result->SetArrayField(TEXT("actors_with_issues"), ActorIssues);
    Result->SetNumberField(TEXT("broken_asset_refs_count"), BrokenAssetRefs.Num());
    Result->SetArrayField(TEXT("broken_asset_refs"), BrokenAssetRefs);
    Result->SetNumberField(TEXT("uncompiled_blueprints_count"), UncompiledBlueprints.Num());
    Result->SetArrayField(TEXT("uncompiled_blueprints"), UncompiledBlueprints);
    return Result;
}
