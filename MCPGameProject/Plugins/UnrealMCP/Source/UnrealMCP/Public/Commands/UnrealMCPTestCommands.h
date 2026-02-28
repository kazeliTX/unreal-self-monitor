#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "MCPCommandRegistry.h"

/**
 * Handler class for Test & Validation MCP commands (Phase 2B).
 *
 * Provides structural validation for:
 *   - validate_blueprint  : compile status, error/warning counts, node & variable counts
 *   - run_level_validation: actors with issues, broken asset refs, uncompiled blueprints
 */
class UNREALMCP_API FUnrealMCPTestCommands
{
public:
    FUnrealMCPTestCommands();

    /** Register all test commands into the central registry. */
    void RegisterCommands(FMCPCommandRegistry& Registry);

private:
    TSharedPtr<FJsonObject> HandleValidateBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRunLevelValidation(const TSharedPtr<FJsonObject>& Params);
};
