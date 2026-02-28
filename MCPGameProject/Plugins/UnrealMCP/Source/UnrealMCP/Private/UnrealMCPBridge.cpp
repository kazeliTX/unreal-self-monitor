#include "UnrealMCPBridge.h"
#include "MCPServerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "EditorAssetLibrary.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "AssetRegistry/AssetRegistryModule.h"
#else
#include "AssetRegistryModule.h"
#endif
#include "JsonObjectConverter.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
// Add Blueprint related includes
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
// UE5.5 correct includes
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
// Blueprint Graph specific includes
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "Subsystems/EditorActorSubsystem.h"
#endif
// Command registry and handler modules
#include "MCPCommandRegistry.h"
#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPBlueprintCommands.h"
#include "Commands/UnrealMCPBlueprintNodeCommands.h"
#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Commands/UnrealMCPUMGCommands.h"
#include "Commands/UnrealMCPLevelCommands.h"
#include "Commands/UnrealMCPAssetCommands.h"
#include "Commands/UnrealMCPDiagnosticsCommands.h"
#include "Commands/UnrealMCPTestCommands.h"
// Settings, editor menu, and cross-version compat macros
#include "UnrealMCPSettings.h"
#include "UnrealMCPCompat.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "UnrealMCP"

// Default settings
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

UUnrealMCPBridge::UUnrealMCPBridge()
{
    // Create the central command registry
    CommandRegistry = MakeShared<FMCPCommandRegistry>();

    // Instantiate all command handler modules
    EditorCommands        = MakeShared<FUnrealMCPEditorCommands>();
    BlueprintCommands     = MakeShared<FUnrealMCPBlueprintCommands>();
    BlueprintNodeCommands = MakeShared<FUnrealMCPBlueprintNodeCommands>();
    ProjectCommands       = MakeShared<FUnrealMCPProjectCommands>();
    UMGCommands           = MakeShared<FUnrealMCPUMGCommands>();
    LevelCommands         = MakeShared<FUnrealMCPLevelCommands>();
    AssetCommands         = MakeShared<FUnrealMCPAssetCommands>();
    DiagnosticsCommands   = MakeShared<FUnrealMCPDiagnosticsCommands>();
    TestCommands          = MakeShared<FUnrealMCPTestCommands>();
    MaterialCommands      = MakeShared<FUnrealMCPMaterialCommands>();

    // Each module self-registers into the registry.
    // To add a new command module: instantiate it and call RegisterCommands here.
    EditorCommands->RegisterCommands(*CommandRegistry);
    BlueprintCommands->RegisterCommands(*CommandRegistry);
    BlueprintNodeCommands->RegisterCommands(*CommandRegistry);
    ProjectCommands->RegisterCommands(*CommandRegistry);
    UMGCommands->RegisterCommands(*CommandRegistry);
    LevelCommands->RegisterCommands(*CommandRegistry);
    AssetCommands->RegisterCommands(*CommandRegistry);
    DiagnosticsCommands->RegisterCommands(*CommandRegistry);
    TestCommands->RegisterCommands(*CommandRegistry);
    MaterialCommands->RegisterCommands(*CommandRegistry);
}

UUnrealMCPBridge::~UUnrealMCPBridge()
{
    CommandRegistry.Reset();
    EditorCommands.Reset();
    BlueprintCommands.Reset();
    BlueprintNodeCommands.Reset();
    ProjectCommands.Reset();
    UMGCommands.Reset();
    LevelCommands.Reset();
    AssetCommands.Reset();
    DiagnosticsCommands.Reset();
    TestCommands.Reset();
    MaterialCommands.Reset();
}

// Initialize subsystem
void UUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Initializing"));

    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

    // Read port from settings (falls back to compile-time default if config missing)
    const UUnrealMCPSettings* Settings = GetDefault<UUnrealMCPSettings>();
    Port = static_cast<uint16>(Settings->Port);

    // Register editor Tools menu (deferred until ToolMenus system is ready)
    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &UUnrealMCPBridge::RegisterMenus));

    // Conditionally auto-start based on settings
    if (Settings->bAutoStartServer)
    {
        StartServer();
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Auto-start disabled — use Tools > UnrealMCP > MCP Server to start"));
    }
}

// Clean up resources when subsystem is destroyed
void UUnrealMCPBridge::Deinitialize()
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Shutting down"));
    StopServer();

    // Unregister startup callback and remove all menus owned by this subsystem
    UToolMenus::UnRegisterStartupCallback(this);
    if (UToolMenus* ToolMenus = UToolMenus::TryGet())
    {
        ToolMenus->UnregisterOwner(this);
    }
}

// Register the "Tools > UnrealMCP" menu section AND a Level Editor toolbar button
void UUnrealMCPBridge::RegisterMenus()
{
    // All entries registered here are owned by this subsystem and auto-removed on Deinitialize
    FToolMenuOwnerScoped OwnerScoped(this);

    // --- Tools menu entry (keep for discoverability) ---
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("MainFrame.MainMenu.Tools");
    FToolMenuSection& Section = Menu->AddSection("UnrealMCPSection", LOCTEXT("UnrealMCPHeading", "UnrealMCP"));
    Section.AddMenuEntry(
        "ToggleMCPServer",
        LOCTEXT("ToggleMCPServer", "MCP Server"),
        LOCTEXT("ToggleMCPServerTip", "Start or stop the MCP TCP server (port 55557). Configure in Project Settings > Plugins > UnrealMCP."),
        FSlateIcon(MCP_STYLE_NAME, "Icons.Toolbar.Settings"),
        FUIAction(
            FExecuteAction::CreateUObject(this, &UUnrealMCPBridge::ToggleServer),
            FCanExecuteAction(),
            FIsActionChecked::CreateUObject(this, &UUnrealMCPBridge::IsRunning)
        ),
        EUserInterfaceActionType::ToggleButton
    );

    // --- Level Editor toolbar button (always visible, shows ON/OFF state) ---
    UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
    FToolMenuSection& ToolbarSection = Toolbar->AddSection("UnrealMCPToolbarSection");
    ToolbarSection.AddEntry(FToolMenuEntry::InitToolBarButton(
        "ToggleMCPServerToolbar",
        FUIAction(
            FExecuteAction::CreateUObject(this, &UUnrealMCPBridge::ToggleServer),
            FCanExecuteAction(),
            FIsActionChecked::CreateUObject(this, &UUnrealMCPBridge::IsRunning)
        ),
        LOCTEXT("MCPToolbarLabel", "MCP"),
        LOCTEXT("MCPToolbarTip", "Start / Stop MCP Server (port 55557)\nGreen = running, use Tools > UnrealMCP to configure."),
        FSlateIcon(MCP_STYLE_NAME, "Icons.Toolbar.Settings"),
        EUserInterfaceActionType::ToggleButton
    ));
}

// Toggle the MCP server on / off
void UUnrealMCPBridge::ToggleServer()
{
    if (bIsRunning)
    {
        StopServer();
    }
    else
    {
        // Re-read port from settings in case it changed since last start
        Port = static_cast<uint16>(GetDefault<UUnrealMCPSettings>()->Port);
        StartServer();
    }
}

// Start the MCP server
void UUnrealMCPBridge::StartServer()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("UnrealMCPBridge: Server is already running"));
        return;
    }

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to get socket subsystem"));
        return;
    }

    // Create listener socket
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to create listener socket"));
        return;
    }

    // Allow address reuse for quick restarts
    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to bind listener socket to %s:%d"), *ServerAddress.ToString(), Port);
        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to start listening"));
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server started on %s:%d"), *ServerAddress.ToString(), Port);

    // Start server thread
    ServerThread = FRunnableThread::Create(
        new FMCPServerRunnable(this, ListenerSocket),
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to create server thread"));
        StopServer();
        return;
    }
}

// Stop the MCP server
void UUnrealMCPBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;

    // Clean up thread
    if (ServerThread)
    {
        ServerThread->Kill(true);
        delete ServerThread;
        ServerThread = nullptr;
    }

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
        ListenerSocket.Reset();
    }

    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server stopped"));
}

// Execute a command received from a client
FString UUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Executing command: %s"), *CommandType);

    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();

    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
    {
        TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);

        try
        {
            TSharedPtr<FJsonObject> ResultJson;

            // --- Built-in commands (not routed via registry) ---
            if (CommandType == TEXT("ping"))
            {
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
            }
            else if (CommandType == TEXT("get_capabilities"))
            {
                TArray<FString> Commands = CommandRegistry->GetRegisteredCommands();
                // Append built-in commands that are not in the registry
                Commands.Add(TEXT("batch"));
                Commands.Add(TEXT("get_capabilities"));
                Commands.Add(TEXT("ping"));
                Commands.Sort();

                TArray<TSharedPtr<FJsonValue>> CmdArray;
                for (const FString& Cmd : Commands)
                {
                    CmdArray.Add(MakeShared<FJsonValueString>(Cmd));
                }

                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetArrayField(TEXT("commands"), CmdArray);
                ResultJson->SetStringField(TEXT("version"), TEXT("1.0.0"));
            }
            else if (CommandType == TEXT("batch"))
            {
                ResultJson = ExecuteBatchCommand(Params);
            }
            // --- All other commands: registry lookup ---
            else
            {
                ResultJson = CommandRegistry->ExecuteCommand(CommandType, Params);
            }

            // Wrap result using the same success/error logic as before
            bool bSuccess = true;
            FString ErrorMessage;

            if (ResultJson->HasField(TEXT("success")))
            {
                bSuccess = ResultJson->GetBoolField(TEXT("success"));
                if (!bSuccess && ResultJson->HasField(TEXT("error")))
                {
                    ErrorMessage = ResultJson->GetStringField(TEXT("error"));
                }
            }

            if (bSuccess)
            {
                ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
                ResponseJson->SetObjectField(TEXT("result"), ResultJson);
            }
            else
            {
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
            }
        }
        catch (const std::exception& e)
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
        }

        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        Promise.SetValue(ResultString);
    });

    return Future.Get();
}

// Execute a batch of commands sequentially on the game thread.
// Always executes all commands regardless of individual failures.
TSharedPtr<FJsonObject> UUnrealMCPBridge::ExecuteBatchCommand(const TSharedPtr<FJsonObject>& Params)
{
    const TArray<TSharedPtr<FJsonValue>>* CommandsArray = nullptr;
    if (!Params.IsValid() || !Params->TryGetArrayField(TEXT("commands"), CommandsArray))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("batch: Missing required 'commands' array parameter"));
    }

    TArray<TSharedPtr<FJsonValue>> Results;
    bool bAllSucceeded = true;

    for (const TSharedPtr<FJsonValue>& CmdValue : *CommandsArray)
    {
        TSharedPtr<FJsonObject> CmdObj = CmdValue->AsObject();
        if (!CmdObj.IsValid())
        {
            TSharedPtr<FJsonObject> ErrEntry = MakeShareable(new FJsonObject);
            ErrEntry->SetStringField(TEXT("command"), TEXT("(invalid)"));
            ErrEntry->SetBoolField(TEXT("success"), false);
            ErrEntry->SetStringField(TEXT("error"), TEXT("batch: Command entry is not a valid JSON object"));
            Results.Add(MakeShared<FJsonValueObject>(ErrEntry));
            bAllSucceeded = false;
            continue;
        }

        FString SubType;
        if (!CmdObj->TryGetStringField(TEXT("type"), SubType))
        {
            TSharedPtr<FJsonObject> ErrEntry = MakeShareable(new FJsonObject);
            ErrEntry->SetStringField(TEXT("command"), TEXT("(missing type)"));
            ErrEntry->SetBoolField(TEXT("success"), false);
            ErrEntry->SetStringField(TEXT("error"), TEXT("batch: Command object is missing the 'type' field"));
            Results.Add(MakeShared<FJsonValueObject>(ErrEntry));
            bAllSucceeded = false;
            continue;
        }

        // Prevent nested batch / built-in commands to avoid recursion
        if (SubType == TEXT("batch") || SubType == TEXT("ping") || SubType == TEXT("get_capabilities"))
        {
            TSharedPtr<FJsonObject> ErrEntry = MakeShareable(new FJsonObject);
            ErrEntry->SetStringField(TEXT("command"), SubType);
            ErrEntry->SetBoolField(TEXT("success"), false);
            ErrEntry->SetStringField(TEXT("error"),
                FString::Printf(TEXT("batch: '%s' cannot be nested inside a batch command"), *SubType));
            Results.Add(MakeShared<FJsonValueObject>(ErrEntry));
            bAllSucceeded = false;
            continue;
        }

        // Extract params (optional)
        TSharedPtr<FJsonObject> SubParams = MakeShareable(new FJsonObject);
        if (CmdObj->HasField(TEXT("params")))
        {
            SubParams = CmdObj->GetObjectField(TEXT("params"));
        }

        // Execute via registry (we are already on the game thread)
        TSharedPtr<FJsonObject> SubResult = CommandRegistry->ExecuteCommand(SubType, SubParams);

        // Build result entry
        TSharedPtr<FJsonObject> ResultEntry = MakeShareable(new FJsonObject);
        ResultEntry->SetStringField(TEXT("command"), SubType);
        ResultEntry->SetObjectField(TEXT("result"), SubResult);

        bool bSubSuccess = true;
        if (SubResult->HasField(TEXT("success")))
        {
            bSubSuccess = SubResult->GetBoolField(TEXT("success"));
        }
        ResultEntry->SetBoolField(TEXT("success"), bSubSuccess);
        if (!bSubSuccess)
        {
            bAllSucceeded = false;
        }

        Results.Add(MakeShared<FJsonValueObject>(ResultEntry));
    }

    TSharedPtr<FJsonObject> BatchResult = MakeShareable(new FJsonObject);
    BatchResult->SetArrayField(TEXT("results"), Results);
    BatchResult->SetBoolField(TEXT("all_succeeded"), bAllSucceeded);
    BatchResult->SetNumberField(TEXT("count"), static_cast<double>(Results.Num()));
    return BatchResult;
}

#undef LOCTEXT_NAMESPACE