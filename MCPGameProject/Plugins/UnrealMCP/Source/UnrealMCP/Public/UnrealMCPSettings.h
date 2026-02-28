#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UnrealMCPSettings.generated.h"

/**
 * Per-project, per-user settings for the UnrealMCP plugin.
 * Appears under Edit > Project Settings > Plugins > UnrealMCP.
 */
UCLASS(config=EditorPerProjectUserSettings, defaultconfig, meta=(DisplayName="UnrealMCP"))
class UNREALMCP_API UUnrealMCPSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UUnrealMCPSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName()  const override { return TEXT("UnrealMCP"); }

	/** Automatically start the MCP TCP server when the editor opens. */
	UPROPERTY(config, EditAnywhere, Category="Server",
		meta=(DisplayName="Auto-Start Server on Editor Open"))
	bool bAutoStartServer = true;

	/** TCP port for the MCP server. Requires a server restart to take effect. */
	UPROPERTY(config, EditAnywhere, Category="Server",
		meta=(DisplayName="Server Port", ClampMin=1024, ClampMax=65535))
	int32 Port = 55557;
};
