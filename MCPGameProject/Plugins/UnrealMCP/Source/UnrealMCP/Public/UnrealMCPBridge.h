#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Http.h"
#include "Json.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "MCPCommandRegistry.h"
#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPBlueprintCommands.h"
#include "Commands/UnrealMCPBlueprintNodeCommands.h"
#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPUMGCommands.h"
#include "Commands/UnrealMCPLevelCommands.h"
#include "Commands/UnrealMCPAssetCommands.h"
#include "Commands/UnrealMCPDiagnosticsCommands.h"
#include "UnrealMCPBridge.generated.h"

class FMCPServerRunnable;

/**
 * Editor subsystem for MCP Bridge
 * Handles communication between external tools and the Unreal Editor
 * through a TCP socket connection. Commands are received as JSON and
 * routed to the central FMCPCommandRegistry.
 *
 * To add a new command module:
 *   1. Create FUnrealMCPXxxCommands with RegisterCommands(FMCPCommandRegistry&).
 *   2. Instantiate it and call RegisterCommands in the constructor below.
 *   No changes to ExecuteCommand are needed.
 */
UCLASS()
class UNREALMCP_API UUnrealMCPBridge : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UUnrealMCPBridge();
	virtual ~UUnrealMCPBridge();

	// UEditorSubsystem implementation
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Server functions
	void StartServer();
	void StopServer();
	bool IsRunning() const { return bIsRunning; }

	// Command execution
	FString ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Server state
	bool bIsRunning;
	TSharedPtr<FSocket> ListenerSocket;
	TSharedPtr<FSocket> ConnectionSocket;
	FRunnableThread* ServerThread;

	// Server configuration
	FIPv4Address ServerAddress;
	uint16 Port;

	// Central command registry (replaces the double if-else dispatch chain)
	TSharedPtr<FMCPCommandRegistry> CommandRegistry;

	// Command handler instances (kept alive for Lambda captures)
	TSharedPtr<FUnrealMCPEditorCommands>         EditorCommands;
	TSharedPtr<FUnrealMCPBlueprintCommands>      BlueprintCommands;
	TSharedPtr<FUnrealMCPBlueprintNodeCommands>  BlueprintNodeCommands;
	TSharedPtr<FUnrealMCPProjectCommands>        ProjectCommands;
	TSharedPtr<FUnrealMCPUMGCommands>            UMGCommands;
	TSharedPtr<FUnrealMCPLevelCommands>          LevelCommands;
	TSharedPtr<FUnrealMCPAssetCommands>          AssetCommands;
	TSharedPtr<FUnrealMCPDiagnosticsCommands>    DiagnosticsCommands;

	// Built-in special commands (not routed via registry)
	TSharedPtr<FJsonObject> ExecuteBatchCommand(const TSharedPtr<FJsonObject>& Params);
};
