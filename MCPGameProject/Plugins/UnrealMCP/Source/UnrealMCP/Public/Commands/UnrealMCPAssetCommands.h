#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "MCPCommandRegistry.h"

/**
 * Handler class for Asset management MCP commands.
 * Handles listing, finding, creating, importing, and managing Content Browser assets.
 * Also handles DataTable operations.
 */
class UNREALMCP_API FUnrealMCPAssetCommands
{
public:
    FUnrealMCPAssetCommands();

    /** Register all asset commands into the central registry. */
    void RegisterCommands(FMCPCommandRegistry& Registry);

private:
    // Asset Registry queries
    TSharedPtr<FJsonObject> HandleListAssets(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDoesAssetExist(const TSharedPtr<FJsonObject>& Params);

    // Folder management
    TSharedPtr<FJsonObject> HandleCreateFolder(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListFolders(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteFolder(const TSharedPtr<FJsonObject>& Params);

    // Asset lifecycle
    TSharedPtr<FJsonObject> HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRenameAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveAllAssets(const TSharedPtr<FJsonObject>& Params);

    // DataTable operations
    TSharedPtr<FJsonObject> HandleCreateDataTable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddDataTableRow(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetDataTableRows(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAssetInfo(const TSharedPtr<FJsonObject>& Params);

    // Asset editor
    TSharedPtr<FJsonObject> HandleOpenAssetEditor(const TSharedPtr<FJsonObject>& Params);
};
