#include "MCPCommandRegistry.h"
#include "Commands/UnrealMCPCommonUtils.h"

void FMCPCommandRegistry::RegisterCommand(const FString& CommandName, FMCPCommandHandler Handler)
{
	if (Commands.Contains(CommandName))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("MCPCommandRegistry: Overwriting existing handler for command '%s'"),
		       *CommandName);
	}
	Commands.Add(CommandName, MoveTemp(Handler));
}

TSharedPtr<FJsonObject> FMCPCommandRegistry::ExecuteCommand(
	const FString& CommandName, const TSharedPtr<FJsonObject>& Params) const
{
	const FMCPCommandHandler* Handler = Commands.Find(CommandName);
	if (!Handler)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown command: %s"), *CommandName));
	}
	return (*Handler)(Params);
}

bool FMCPCommandRegistry::HasCommand(const FString& CommandName) const
{
	return Commands.Contains(CommandName);
}

TArray<FString> FMCPCommandRegistry::GetRegisteredCommands() const
{
	TArray<FString> Keys;
	Commands.GetKeys(Keys);
	Keys.Sort();
	return Keys;
}
