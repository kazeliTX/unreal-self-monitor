#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "MCPCommandRegistry.h"

/**
 * Handler class for Diagnostics MCP commands.
 *
 * Provides self-healing capabilities:
 *   - Viewport camera info / actor screen-position queries
 *   - Actor highlighting (select + focus)
 *   - LiveCoding hot-reload control
 *   - C++ source file read / write (with automatic backup)
 *   - Engine installation path discovery
 */
class UNREALMCP_API FUnrealMCPDiagnosticsCommands
{
public:
    FUnrealMCPDiagnosticsCommands();

    /** Register all diagnostics commands into the central registry. */
    void RegisterCommands(FMCPCommandRegistry& Registry);

private:
    // Visual perception
    TSharedPtr<FJsonObject> HandleGetViewportCameraInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetActorScreenPosition(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleHighlightActor(const TSharedPtr<FJsonObject>& Params);

    // Hot-reload / LiveCoding
    TSharedPtr<FJsonObject> HandleTriggerHotReload(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetLiveCodingStatus(const TSharedPtr<FJsonObject>& Params);

    // Source file access
    TSharedPtr<FJsonObject> HandleGetSourceFile(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleModifySourceFile(const TSharedPtr<FJsonObject>& Params);

    // Engine / project path discovery
    TSharedPtr<FJsonObject> HandleGetEnginePath(const TSharedPtr<FJsonObject>& Params);
};
