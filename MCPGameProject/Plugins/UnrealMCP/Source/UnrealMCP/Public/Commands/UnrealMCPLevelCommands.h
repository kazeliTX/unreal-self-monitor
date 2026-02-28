#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "MCPCommandRegistry.h"

/**
 * Handler class for Level management MCP commands.
 * Handles creating, opening, and saving levels.
 */
class UNREALMCP_API FUnrealMCPLevelCommands
{
public:
    FUnrealMCPLevelCommands();

    /** Register all level commands into the central registry. */
    void RegisterCommands(FMCPCommandRegistry& Registry);

private:
    TSharedPtr<FJsonObject> HandleNewLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleOpenLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveAllLevels(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetCurrentLevelName(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetLevelDirtyState(const TSharedPtr<FJsonObject>& Params);

    /** Silently save all dirty packages without prompting the user. */
    void SilentSaveAllDirtyPackages();
};
