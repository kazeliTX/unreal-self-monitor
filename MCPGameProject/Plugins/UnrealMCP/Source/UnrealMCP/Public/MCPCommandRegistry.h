#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Command handler function signature.
 * Each registered command maps to one of these functions:
 *   Input:  JSON params object
 *   Output: JSON result object (use CreateErrorResponse on failure)
 */
using FMCPCommandHandler = TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&)>;

/**
 * Central command registry for the MCP plugin.
 *
 * Replaces the double if-else chain that previously lived in
 * UUnrealMCPBridge::ExecuteCommand and each FUnrealMCPXxxCommands::HandleCommand.
 *
 * Usage:
 *   // Registration (once, at startup)
 *   Registry.RegisterCommand(TEXT("my_command"),
 *       [this](const TSharedPtr<FJsonObject>& Params) { return HandleMyCommand(Params); });
 *
 *   // Dispatch (per incoming TCP command)
 *   TSharedPtr<FJsonObject> Result = Registry.ExecuteCommand(CommandType, Params);
 *
 * Adding a new command module:
 *   1. Create FUnrealMCPXxxCommands with a RegisterCommands(FMCPCommandRegistry&) method.
 *   2. Call that method from UUnrealMCPBridge constructor.
 *   No changes to UnrealMCPBridge::ExecuteCommand are needed.
 */
class UNREALMCP_API FMCPCommandRegistry
{
public:
	FMCPCommandRegistry() = default;

	/**
	 * Register a command handler.
	 * Logs a warning if CommandName was already registered (last writer wins).
	 */
	void RegisterCommand(const FString& CommandName, FMCPCommandHandler Handler);

	/**
	 * Execute a registered command.
	 * Returns an error JSON object if CommandName is not registered.
	 */
	TSharedPtr<FJsonObject> ExecuteCommand(const FString& CommandName,
	                                        const TSharedPtr<FJsonObject>& Params) const;

	/** Returns true if CommandName has been registered. */
	bool HasCommand(const FString& CommandName) const;

	/**
	 * Returns a sorted list of all registered command names.
	 * Used by the get_capabilities built-in command.
	 */
	TArray<FString> GetRegisteredCommands() const;

private:
	TMap<FString, FMCPCommandHandler> Commands;
};
