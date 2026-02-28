#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "MCPCommandRegistry.h"

/**
 * Handler class for Editor-related MCP commands
 * Handles viewport control, actor manipulation, and level management
 */
class UNREALMCP_API FUnrealMCPEditorCommands
{
public:
    FUnrealMCPEditorCommands();

    /** Register all editor commands into the central registry. */
    void RegisterCommands(FMCPCommandRegistry& Registry);

private:
    // Actor manipulation commands
    TSharedPtr<FJsonObject> HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params);

    // Blueprint actor spawning
    TSharedPtr<FJsonObject> HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params);

    // Editor viewport commands
    TSharedPtr<FJsonObject> HandleFocusViewport(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params);

    // Actor selection commands
    TSharedPtr<FJsonObject> HandleSelectActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeselectAll(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params);

    // Actor label commands
    TSharedPtr<FJsonObject> HandleSetActorLabel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetActorLabel(const TSharedPtr<FJsonObject>& Params);

    // Actor hierarchy commands
    TSharedPtr<FJsonObject> HandleAttachActorToActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDetachActor(const TSharedPtr<FJsonObject>& Params);

    // Actor tag commands
    TSharedPtr<FJsonObject> HandleAddActorTag(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveActorTag(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetActorTags(const TSharedPtr<FJsonObject>& Params);

    // World settings commands
    TSharedPtr<FJsonObject> HandleGetWorldSettings(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWorldSettings(const TSharedPtr<FJsonObject>& Params);
};