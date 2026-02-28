#include "Commands/UnrealMCPAssetCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "EditorAssetLibrary.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "Subsystems/AssetEditorSubsystem.h"
#else
#include "Toolkits/AssetEditorManager.h"
#endif
#if ENGINE_MAJOR_VERSION >= 5
#include "AssetRegistry/AssetRegistryModule.h"
#else
#include "AssetRegistryModule.h"
#endif
#if ENGINE_MAJOR_VERSION >= 5
#include "AssetRegistry/IAssetRegistry.h"
#else
#include "IAssetRegistry.h"
#endif
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/DataTable.h"
#include "Factories/DataTableFactory.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

FUnrealMCPAssetCommands::FUnrealMCPAssetCommands()
{
}

void FUnrealMCPAssetCommands::RegisterCommands(FMCPCommandRegistry& Registry)
{
    Registry.RegisterCommand(TEXT("list_assets"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleListAssets(P); });
    Registry.RegisterCommand(TEXT("find_asset"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleFindAsset(P); });
    Registry.RegisterCommand(TEXT("does_asset_exist"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleDoesAssetExist(P); });
    Registry.RegisterCommand(TEXT("get_asset_info"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetAssetInfo(P); });
    Registry.RegisterCommand(TEXT("create_folder"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleCreateFolder(P); });
    Registry.RegisterCommand(TEXT("list_folders"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleListFolders(P); });
    Registry.RegisterCommand(TEXT("delete_folder"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleDeleteFolder(P); });
    Registry.RegisterCommand(TEXT("duplicate_asset"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleDuplicateAsset(P); });
    Registry.RegisterCommand(TEXT("rename_asset"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleRenameAsset(P); });
    Registry.RegisterCommand(TEXT("delete_asset"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleDeleteAsset(P); });
    Registry.RegisterCommand(TEXT("save_asset"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleSaveAsset(P); });
    Registry.RegisterCommand(TEXT("save_all_assets"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleSaveAllAssets(P); });
    Registry.RegisterCommand(TEXT("create_data_table"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleCreateDataTable(P); });
    Registry.RegisterCommand(TEXT("add_data_table_row"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleAddDataTableRow(P); });
    Registry.RegisterCommand(TEXT("get_data_table_rows"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetDataTableRows(P); });
    Registry.RegisterCommand(TEXT("open_asset_editor"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleOpenAssetEditor(P); });
}

// ---------------------------------------------------------------------------
// Asset Registry queries
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleListAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString DirectoryPath = TEXT("/Game/");
    Params->TryGetStringField(TEXT("path"), DirectoryPath);

    bool bRecursive = true;
    if (Params->HasField(TEXT("recursive")))
    {
        bRecursive = Params->GetBoolField(TEXT("recursive"));
    }

    FString ClassFilter;
    Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

    TArray<FString> AssetPaths = UEditorAssetLibrary::ListAssets(DirectoryPath, bRecursive, false);

    TArray<TSharedPtr<FJsonValue>> AssetArray;
    for (const FString& AssetPath : AssetPaths)
    {
        // Apply class filter if specified
        if (!ClassFilter.IsEmpty())
        {
            FAssetData AssetData = UEditorAssetLibrary::FindAssetData(AssetPath);
            if (AssetData.IsValid())
            {
#if ENGINE_MAJOR_VERSION >= 5
                FString AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();
#else
                FString AssetClass = AssetData.AssetClass.ToString();
#endif
                if (!AssetClass.Contains(ClassFilter))
                {
                    continue;
                }
            }
        }

        TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
        AssetObj->SetStringField(TEXT("path"), AssetPath);

        FAssetData AssetData = UEditorAssetLibrary::FindAssetData(AssetPath);
        if (AssetData.IsValid())
        {
            AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
#if ENGINE_MAJOR_VERSION >= 5
            AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
#else
            AssetObj->SetStringField(TEXT("class"), AssetData.AssetClass.ToString());
#endif
            AssetObj->SetStringField(TEXT("package"), AssetData.PackageName.ToString());
        }

        AssetArray.Add(MakeShared<FJsonValueObject>(AssetObj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("assets"), AssetArray);
    Result->SetNumberField(TEXT("count"), static_cast<double>(AssetArray.Num()));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleFindAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetName;
    if (!Params->TryGetStringField(TEXT("name"), AssetName) || AssetName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString SearchPath = TEXT("/Game/");
    Params->TryGetStringField(TEXT("path"), SearchPath);

    TArray<FString> AssetPaths = UEditorAssetLibrary::ListAssets(SearchPath, true, false);

    TArray<TSharedPtr<FJsonValue>> FoundArray;
    for (const FString& AssetPath : AssetPaths)
    {
        FString FileName = FPaths::GetBaseFilename(AssetPath);
        if (FileName.Contains(AssetName, ESearchCase::IgnoreCase))
        {
            FAssetData AssetData = UEditorAssetLibrary::FindAssetData(AssetPath);
            TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
            AssetObj->SetStringField(TEXT("path"), AssetPath);
            if (AssetData.IsValid())
            {
                AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
#if ENGINE_MAJOR_VERSION >= 5
                AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
#else
                AssetObj->SetStringField(TEXT("class"), AssetData.AssetClass.ToString());
#endif
                AssetObj->SetStringField(TEXT("package"), AssetData.PackageName.ToString());
            }
            FoundArray.Add(MakeShared<FJsonValueObject>(AssetObj));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("assets"), FoundArray);
    Result->SetNumberField(TEXT("count"), static_cast<double>(FoundArray.Num()));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleDoesAssetExist(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    bool bExists = UEditorAssetLibrary::DoesAssetExist(AssetPath);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("exists"), bExists);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetAssetInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
    }

    FAssetData AssetData = UEditorAssetLibrary::FindAssetData(AssetPath);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"), AssetPath);
    Result->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
#if ENGINE_MAJOR_VERSION >= 5
    Result->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
#else
    Result->SetStringField(TEXT("class"), AssetData.AssetClass.ToString());
#endif
    Result->SetStringField(TEXT("package"), AssetData.PackageName.ToString());

    // Add tags
    TArray<TSharedPtr<FJsonValue>> TagArray;
#if ENGINE_MAJOR_VERSION >= 5
    for (const TPair<FName, FAssetTagValueRef>& Tag : AssetData.TagsAndValues)
    {
        TSharedPtr<FJsonObject> TagObj = MakeShared<FJsonObject>();
        TagObj->SetStringField(TEXT("key"), Tag.Key.ToString());
        TagObj->SetStringField(TEXT("value"), Tag.Value.AsString());
        TagArray.Add(MakeShared<FJsonValueObject>(TagObj));
    }
#else
    for (const TPair<FName, FString>& Tag : AssetData.TagsAndValues)
    {
        TSharedPtr<FJsonObject> TagObj = MakeShared<FJsonObject>();
        TagObj->SetStringField(TEXT("key"), Tag.Key.ToString());
        TagObj->SetStringField(TEXT("value"), Tag.Value);
        TagArray.Add(MakeShared<FJsonValueObject>(TagObj));
    }
#endif
    Result->SetArrayField(TEXT("tags"), TagArray);

    return Result;
}

// ---------------------------------------------------------------------------
// Folder management
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCreateFolder(const TSharedPtr<FJsonObject>& Params)
{
    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("path"), FolderPath) || FolderPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    bool bSuccess = UEditorAssetLibrary::MakeDirectory(FolderPath);
    if (!bSuccess)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to create folder: %s"), *FolderPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("path"), FolderPath);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleListFolders(const TSharedPtr<FJsonObject>& Params)
{
    FString BasePath = TEXT("/Game/");
    Params->TryGetStringField(TEXT("path"), BasePath);

    bool bRecursive = false;
    if (Params->HasField(TEXT("recursive")))
    {
        bRecursive = Params->GetBoolField(TEXT("recursive"));
    }

    TArray<FString> SubPaths = UEditorAssetLibrary::ListAssets(BasePath, bRecursive, true);

    // Filter to only include directories (paths that don't have a .uasset extension representation)
    TArray<TSharedPtr<FJsonValue>> FolderArray;
    for (const FString& SubPath : SubPaths)
    {
        // ListAssets with bIncludeFolder=true returns both assets and folder paths
        // Folder paths end with '/' or don't contain a dot after the last '/'
        if (!UEditorAssetLibrary::DoesAssetExist(SubPath))
        {
            FolderArray.Add(MakeShared<FJsonValueString>(SubPath));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("folders"), FolderArray);
    Result->SetNumberField(TEXT("count"), static_cast<double>(FolderArray.Num()));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleDeleteFolder(const TSharedPtr<FJsonObject>& Params)
{
    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("path"), FolderPath) || FolderPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    bool bSuccess = UEditorAssetLibrary::DeleteDirectory(FolderPath);
    if (!bSuccess)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to delete folder: %s (may not be empty or not exist)"), *FolderPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("path"), FolderPath);
    return Result;
}

// ---------------------------------------------------------------------------
// Asset lifecycle
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath;
    if (!Params->TryGetStringField(TEXT("source_path"), SourcePath) || SourcePath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter"));
    }

    FString DestPath;
    if (!Params->TryGetStringField(TEXT("dest_path"), DestPath) || DestPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'dest_path' parameter"));
    }

    UObject* DuplicatedAsset = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);
    if (!DuplicatedAsset)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to duplicate asset from '%s' to '%s'"), *SourcePath, *DestPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("source_path"), SourcePath);
    Result->SetStringField(TEXT("dest_path"), DestPath);
    Result->SetStringField(TEXT("asset_name"), DuplicatedAsset->GetName());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleRenameAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath;
    if (!Params->TryGetStringField(TEXT("source_path"), SourcePath) || SourcePath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter"));
    }

    FString DestPath;
    if (!Params->TryGetStringField(TEXT("dest_path"), DestPath) || DestPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'dest_path' parameter"));
    }

    bool bSuccess = UEditorAssetLibrary::RenameAsset(SourcePath, DestPath);
    if (!bSuccess)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to rename asset from '%s' to '%s'"), *SourcePath, *DestPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("source_path"), SourcePath);
    Result->SetStringField(TEXT("dest_path"), DestPath);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    bool bSuccess = UEditorAssetLibrary::DeleteAsset(AssetPath);
    if (!bSuccess)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to delete asset: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSaveAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    bool bOnlyIfDirty = true;
    if (Params->HasField(TEXT("only_if_dirty")))
    {
        bOnlyIfDirty = Params->GetBoolField(TEXT("only_if_dirty"));
    }

    bool bSuccess = UEditorAssetLibrary::SaveAsset(AssetPath, bOnlyIfDirty);
    if (!bSuccess)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to save asset: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSaveAllAssets(const TSharedPtr<FJsonObject>& Params)
{
    bool bOnlyIfDirty = true;
    if (Params->HasField(TEXT("only_if_dirty")))
    {
        bOnlyIfDirty = Params->GetBoolField(TEXT("only_if_dirty"));
    }

    bool bSuccess = UEditorAssetLibrary::SaveDirectory(TEXT("/Game/"), bOnlyIfDirty, true);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    return Result;
}

// ---------------------------------------------------------------------------
// DataTable operations
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCreateDataTable(const TSharedPtr<FJsonObject>& Params)
{
    FString TableName;
    if (!Params->TryGetStringField(TEXT("name"), TableName) || TableName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString StructClassName;
    if (!Params->TryGetStringField(TEXT("row_struct"), StructClassName) || StructClassName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'row_struct' parameter (e.g. 'TableRowBase' or a custom struct class name)"));
    }

    FString PackagePath = TEXT("/Game/DataTables/");
    Params->TryGetStringField(TEXT("path"), PackagePath);
    if (!PackagePath.EndsWith(TEXT("/")))
    {
        PackagePath += TEXT("/");
    }

    // Find the row struct
    UScriptStruct* RowStruct = FindObject<UScriptStruct>(ANY_PACKAGE, *StructClassName);
    if (!RowStruct)
    {
        // Try with common prefixes / paths
        RowStruct = LoadObject<UScriptStruct>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *StructClassName));
    }
    if (!RowStruct)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Row struct '%s' not found. Use an existing struct class name (e.g. 'TableRowBase')."), *StructClassName));
    }

    // Check if asset already exists
    FString FullPath = PackagePath + TableName;
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("DataTable '%s' already exists at '%s'"), *TableName, *FullPath));
    }

    // Create the DataTable using AssetTools
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    UDataTableFactory* Factory = NewObject<UDataTableFactory>();
    Factory->Struct = RowStruct;

    UObject* NewAsset = AssetTools.CreateAsset(TableName, PackagePath, UDataTable::StaticClass(), Factory);
    UDataTable* DataTable = Cast<UDataTable>(NewAsset);
    if (!DataTable)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to create DataTable '%s'"), *TableName));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), TableName);
    Result->SetStringField(TEXT("path"), FullPath);
    Result->SetStringField(TEXT("row_struct"), StructClassName);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleAddDataTableRow(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    FString RowName;
    if (!Params->TryGetStringField(TEXT("row_name"), RowName) || RowName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'row_name' parameter"));
    }

    // row_data is a JSON object with the row's field values
    const TSharedPtr<FJsonObject>* RowDataObj = nullptr;
    if (!Params->TryGetObjectField(TEXT("row_data"), RowDataObj))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'row_data' parameter (must be a JSON object)"));
    }

    // Load the DataTable
    UDataTable* DataTable = Cast<UDataTable>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!DataTable)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("DataTable not found at: %s"), *AssetPath));
    }

    // Build JSON string for the new row
    // UDataTable's CreateTableFromJSONString expects format: [{"Name":"RowName", "Field":"Value", ...}]
    // We merge existing rows + new row using GetTableAsJSON + append
    FString ExistingJSON = DataTable->GetTableAsJSON(EDataTableExportFlags::None);

    // Parse existing rows array
    TArray<TSharedPtr<FJsonValue>> ExistingRows;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ExistingJSON);
    FJsonSerializer::Deserialize(Reader, ExistingRows);

    // Build new row entry
    TSharedPtr<FJsonObject> NewRow = MakeShared<FJsonObject>();
    NewRow->SetStringField(TEXT("Name"), RowName);
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*RowDataObj)->Values)
    {
        NewRow->SetField(Field.Key, Field.Value);
    }
    ExistingRows.Add(MakeShared<FJsonValueObject>(NewRow));

    // Serialize back to JSON string
    FString NewJSON;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&NewJSON);
    FJsonSerializer::Serialize(ExistingRows, Writer);

    // Apply to DataTable
    TArray<FString> Errors;
    DataTable->CreateTableFromJSONString(NewJSON);
    DataTable->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("row_name"), RowName);
    Result->SetNumberField(TEXT("total_rows"), static_cast<double>(DataTable->GetRowMap().Num()));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetDataTableRows(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    UDataTable* DataTable = Cast<UDataTable>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!DataTable)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("DataTable not found at: %s"), *AssetPath));
    }

    FString JSON = DataTable->GetTableAsJSON(EDataTableExportFlags::None);

    // Parse JSON and return as array
    TArray<TSharedPtr<FJsonValue>> RowArray;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JSON);
    FJsonSerializer::Deserialize(Reader, RowArray);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("rows"), RowArray);
    Result->SetNumberField(TEXT("count"), static_cast<double>(RowArray.Num()));
    Result->SetStringField(TEXT("row_struct"), DataTable->RowStruct ? DataTable->RowStruct->GetName() : TEXT("Unknown"));
    return Result;
}

// ---------------------------------------------------------------------------
// Asset Editor
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleOpenAssetEditor(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path parameter is required"));
    }

    // Load the asset
    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset not found or failed to load: %s"), *AssetPath));
    }

    // Open asset in its editor
    bool bOpened = false;
#if ENGINE_MAJOR_VERSION >= 5
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (AssetEditorSubsystem)
    {
        bOpened = AssetEditorSubsystem->OpenEditorForAsset(Asset);
    }
#else
    bOpened = FAssetEditorManager::Get().OpenEditorForAsset(Asset);
#endif

    if (!bOpened)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to open editor for asset: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("asset_name"), Asset->GetName());
    Result->SetStringField(TEXT("asset_class"), Asset->GetClass()->GetName());
    Result->SetBoolField(TEXT("opened"), true);
    return Result;
}
