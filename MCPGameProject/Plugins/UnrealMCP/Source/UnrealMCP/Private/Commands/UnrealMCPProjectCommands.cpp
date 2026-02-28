#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "GameFramework/InputSettings.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/Factory.h"
#include "GeneralProjectSettings.h"

FUnrealMCPProjectCommands::FUnrealMCPProjectCommands()
{
}

void FUnrealMCPProjectCommands::RegisterCommands(FMCPCommandRegistry& Registry)
{
    Registry.RegisterCommand(TEXT("create_input_mapping"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleCreateInputMapping(P); });
    Registry.RegisterCommand(TEXT("run_console_command"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleRunConsoleCommand(P); });
    Registry.RegisterCommand(TEXT("create_input_action"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleCreateInputAction(P); });
    Registry.RegisterCommand(TEXT("create_input_mapping_context"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleCreateInputMappingContext(P); });
    Registry.RegisterCommand(TEXT("add_input_mapping"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleAddInputMapping(P); });
    Registry.RegisterCommand(TEXT("set_input_action_type"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleSetInputActionType(P); });
    Registry.RegisterCommand(TEXT("get_project_settings"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetProjectSettings(P); });
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    FString Key;
    if (!Params->TryGetStringField(TEXT("key"), Key))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'key' parameter"));
    }

    // Get the input settings
    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get input settings"));
    }

    // Create the input action mapping
    FInputActionKeyMapping ActionMapping;
    ActionMapping.ActionName = FName(*ActionName);
    ActionMapping.Key = FKey(*Key);

    // Add modifiers if provided
    if (Params->HasField(TEXT("shift")))
    {
        ActionMapping.bShift = Params->GetBoolField(TEXT("shift"));
    }
    if (Params->HasField(TEXT("ctrl")))
    {
        ActionMapping.bCtrl = Params->GetBoolField(TEXT("ctrl"));
    }
    if (Params->HasField(TEXT("alt")))
    {
        ActionMapping.bAlt = Params->GetBoolField(TEXT("alt"));
    }
    if (Params->HasField(TEXT("cmd")))
    {
        ActionMapping.bCmd = Params->GetBoolField(TEXT("cmd"));
    }

    // Add the mapping
    InputSettings->AddActionMapping(ActionMapping);
    InputSettings->SaveConfig();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("action_name"), ActionName);
    ResultObj->SetStringField(TEXT("key"), Key);
    return ResultObj;
}

// ---------------------------------------------------------------------------
// run_console_command
// Params: command (string)
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleRunConsoleCommand(const TSharedPtr<FJsonObject>& Params)
{
    FString Command;
    if (!Params->TryGetStringField(TEXT("command"), Command))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'command' parameter"));

    if (!GEditor)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available"));

    GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command, *GLog);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("command"), Command);
    Result->SetBoolField(TEXT("executed"), true);
    return Result;
}

// ---------------------------------------------------------------------------
// create_input_action  (Enhanced Input)
// Params: name, path (optional, default /Game/Input/Actions/)
// Requires EnhancedInput module and UInputAction class
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateInputAction(const TSharedPtr<FJsonObject>& Params)
{
    FString ActionName;
    if (!Params->TryGetStringField(TEXT("name"), ActionName))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));

    FString SavePath;
    if (!Params->TryGetStringField(TEXT("path"), SavePath) || SavePath.IsEmpty())
        SavePath = TEXT("/Game/Input/Actions/");
    if (!SavePath.EndsWith(TEXT("/"))) SavePath += TEXT("/");

    FString FullPath = SavePath + ActionName;
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("name"), ActionName);
        Result->SetStringField(TEXT("path"), FullPath);
        Result->SetBoolField(TEXT("already_existed"), true);
        return Result;
    }

    // Find UInputAction class (lives in EnhancedInput plugin)
    UClass* InputActionClass = FindObject<UClass>(ANY_PACKAGE, TEXT("InputAction"));
    if (!InputActionClass)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("UInputAction class not found — ensure EnhancedInput plugin is enabled"));

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    UObject* NewAsset = AssetTools.CreateAsset(ActionName, SavePath, InputActionClass, nullptr);
    if (!NewAsset)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create InputAction asset"));

    FAssetRegistryModule::AssetCreated(NewAsset);
    UEditorAssetLibrary::SaveAsset(FullPath, false);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), ActionName);
    Result->SetStringField(TEXT("path"), FullPath);
    return Result;
}

// ---------------------------------------------------------------------------
// create_input_mapping_context  (Enhanced Input)
// Params: name, path (optional, default /Game/Input/)
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateInputMappingContext(const TSharedPtr<FJsonObject>& Params)
{
    FString ContextName;
    if (!Params->TryGetStringField(TEXT("name"), ContextName))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));

    FString SavePath;
    if (!Params->TryGetStringField(TEXT("path"), SavePath) || SavePath.IsEmpty())
        SavePath = TEXT("/Game/Input/");
    if (!SavePath.EndsWith(TEXT("/"))) SavePath += TEXT("/");

    FString FullPath = SavePath + ContextName;
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("name"), ContextName);
        Result->SetStringField(TEXT("path"), FullPath);
        Result->SetBoolField(TEXT("already_existed"), true);
        return Result;
    }

    UClass* IMCClass = FindObject<UClass>(ANY_PACKAGE, TEXT("InputMappingContext"));
    if (!IMCClass)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("UInputMappingContext class not found — ensure EnhancedInput plugin is enabled"));

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    UObject* NewAsset = AssetTools.CreateAsset(ContextName, SavePath, IMCClass, nullptr);
    if (!NewAsset)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create InputMappingContext asset"));

    FAssetRegistryModule::AssetCreated(NewAsset);
    UEditorAssetLibrary::SaveAsset(FullPath, false);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), ContextName);
    Result->SetStringField(TEXT("path"), FullPath);
    return Result;
}

// ---------------------------------------------------------------------------
// add_input_mapping
// Params: context_path, action_path, key
// Adds a UInputAction -> key mapping to an InputMappingContext via reflection
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleAddInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    FString ContextPath, ActionPath, Key;
    if (!Params->TryGetStringField(TEXT("context_path"), ContextPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'context_path' parameter"));
    if (!Params->TryGetStringField(TEXT("action_path"), ActionPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_path' parameter"));
    if (!Params->TryGetStringField(TEXT("key"), Key))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'key' parameter"));

    UObject* ContextObj = UEditorAssetLibrary::LoadAsset(ContextPath);
    if (!ContextObj)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("InputMappingContext not found: %s"), *ContextPath));

    UObject* ActionObj = UEditorAssetLibrary::LoadAsset(ActionPath);
    if (!ActionObj)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("InputAction not found: %s"), *ActionPath));

    // Use UE reflection to call MapKey — avoids hard dependency on EnhancedInput headers
    UFunction* MapKeyFunc = ContextObj->GetClass()->FindFunctionByName(TEXT("MapKey"));
    if (!MapKeyFunc)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("MapKey function not found on InputMappingContext"));

    struct { UObject* Action; FKey Key; } FuncParams{ ActionObj, FKey(*Key) };
    ContextObj->ProcessEvent(MapKeyFunc, &FuncParams);

    ContextObj->MarkPackageDirty();
    UEditorAssetLibrary::SaveAsset(ContextPath, false);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("context"), ContextPath);
    Result->SetStringField(TEXT("action"), ActionPath);
    Result->SetStringField(TEXT("key"), Key);
    return Result;
}

// ---------------------------------------------------------------------------
// set_input_action_type
// Params: action_path, value_type (Digital, Axis1D, Axis2D, Axis3D)
// Sets UInputAction::ValueType via reflection
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleSetInputActionType(const TSharedPtr<FJsonObject>& Params)
{
    FString ActionPath, ValueType;
    if (!Params->TryGetStringField(TEXT("action_path"), ActionPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_path' parameter"));
    if (!Params->TryGetStringField(TEXT("value_type"), ValueType))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value_type' parameter"));

    UObject* ActionObj = UEditorAssetLibrary::LoadAsset(ActionPath);
    if (!ActionObj)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("InputAction not found: %s"), *ActionPath));

    FProperty* ValueTypeProp = ActionObj->GetClass()->FindPropertyByName(TEXT("ValueType"));
    if (!ValueTypeProp)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("ValueType property not found on InputAction"));

    // EInputActionValueType: Digital=0, Axis1D=1, Axis2D=2, Axis3D=3
    uint8 EnumVal = 0;
    if (ValueType == TEXT("Axis1D")) EnumVal = 1;
    else if (ValueType == TEXT("Axis2D")) EnumVal = 2;
    else if (ValueType == TEXT("Axis3D")) EnumVal = 3;

    FByteProperty* ByteProp = CastField<FByteProperty>(ValueTypeProp);
    if (ByteProp)
    {
        ByteProp->SetPropertyValue_InContainer(ActionObj, EnumVal);
    }

    ActionObj->MarkPackageDirty();
    UEditorAssetLibrary::SaveAsset(ActionPath, false);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("action_path"), ActionPath);
    Result->SetStringField(TEXT("value_type"), ValueType);
    return Result;
}

// ---------------------------------------------------------------------------
// get_project_settings
// Returns basic project settings info
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleGetProjectSettings(const TSharedPtr<FJsonObject>& Params)
{
    const UGeneralProjectSettings* Settings = GetDefault<UGeneralProjectSettings>();
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    if (Settings)
    {
        Result->SetStringField(TEXT("project_name"), Settings->ProjectName);
        Result->SetStringField(TEXT("company_name"), Settings->CompanyName);
        Result->SetStringField(TEXT("description"), Settings->Description);
        Result->SetStringField(TEXT("homepage"), Settings->Homepage);
        Result->SetStringField(TEXT("project_version"), Settings->ProjectVersion);
    }
    // Add engine version
    Result->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
    return Result;
}