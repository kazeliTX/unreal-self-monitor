#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "MCPCommandRegistry.h"

/**
 * Handler class for Project-wide MCP commands
 */
class UNREALMCP_API FUnrealMCPProjectCommands
{
public:
    FUnrealMCPProjectCommands();

    /** Register all project commands into the central registry. */
    void RegisterCommands(FMCPCommandRegistry& Registry);

private:
    // Specific project command handlers
    TSharedPtr<FJsonObject> HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params);

    // New project commands
    TSharedPtr<FJsonObject> HandleRunConsoleCommand(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateInputAction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateInputMappingContext(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddInputMapping(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetInputActionType(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetProjectSettings(const TSharedPtr<FJsonObject>& Params);
};