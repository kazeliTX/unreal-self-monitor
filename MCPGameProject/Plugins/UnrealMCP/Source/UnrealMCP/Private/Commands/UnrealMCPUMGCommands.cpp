#include "Commands/UnrealMCPUMGCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "WidgetBlueprint.h"
// We'll create widgets using regular Factory classes
#include "Factories/Factory.h"
// Remove problematic includes that don't exist in UE 5.5
// #include "UMGEditorSubsystem.h"
// #include "WidgetBlueprintFactory.h"
#include "WidgetBlueprintEditor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "JsonObjectConverter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/Button.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_Event.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

FUnrealMCPUMGCommands::FUnrealMCPUMGCommands()
{
}

void FUnrealMCPUMGCommands::RegisterCommands(FMCPCommandRegistry& Registry)
{
	Registry.RegisterCommand(TEXT("create_umg_widget_blueprint"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleCreateUMGWidgetBlueprint(P); });
	Registry.RegisterCommand(TEXT("add_text_block_to_widget"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddTextBlockToWidget(P); });
	Registry.RegisterCommand(TEXT("add_widget_to_viewport"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddWidgetToViewport(P); });
	Registry.RegisterCommand(TEXT("add_button_to_widget"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddButtonToWidget(P); });
	Registry.RegisterCommand(TEXT("bind_widget_event"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleBindWidgetEvent(P); });
	Registry.RegisterCommand(TEXT("set_text_block_binding"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetTextBlockBinding(P); });
	Registry.RegisterCommand(TEXT("add_image_to_widget"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddImageToWidget(P); });
	Registry.RegisterCommand(TEXT("add_progress_bar_to_widget"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddProgressBarToWidget(P); });
	Registry.RegisterCommand(TEXT("add_horizontal_box_to_widget"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddHorizontalBoxToWidget(P); });
	Registry.RegisterCommand(TEXT("add_vertical_box_to_widget"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddVerticalBoxToWidget(P); });
	Registry.RegisterCommand(TEXT("set_widget_visibility"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetWidgetVisibility(P); });
	Registry.RegisterCommand(TEXT("set_widget_anchor"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetWidgetAnchor(P); });
	Registry.RegisterCommand(TEXT("update_text_block_text"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleUpdateTextBlockText(P); });
	Registry.RegisterCommand(TEXT("get_widget_tree"),
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetWidgetTree(P); });
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	// Resolve save path: use caller-supplied "path" or fall back to default
	FString PackagePath;
	if (!Params->TryGetStringField(TEXT("path"), PackagePath) || PackagePath.IsEmpty())
	{
		PackagePath = TEXT("/Game/Widgets/");
	}
	if (!PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath += TEXT("/");
	}
	FString AssetName = BlueprintName;
	FString FullPath = PackagePath + AssetName;

	// Check if asset already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' already exists"), *BlueprintName));
	}

	// Create package
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
	}

	// Create Widget Blueprint using KismetEditorUtilities
	UBlueprint* NewBlueprint = FKismetEditorUtilities::CreateBlueprint(
		UUserWidget::StaticClass(),  // Parent class
		Package,                     // Outer package
		FName(*AssetName),           // Blueprint name
		BPTYPE_Normal,               // Blueprint type
		UBlueprint::StaticClass(),   // Blueprint class
		UBlueprintGeneratedClass::StaticClass(), // Generated class
		FName("CreateUMGWidget")     // Creation method name
	);

	// Make sure the Blueprint was created successfully
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(NewBlueprint);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Widget Blueprint"));
	}

	// Add a default Canvas Panel if one doesn't exist
	if (!WidgetBlueprint->WidgetTree->RootWidget)
	{
		UCanvasPanel* RootCanvas = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetBlueprint->WidgetTree->RootWidget = RootCanvas;
	}

	// Mark the package dirty and notify asset registry
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(WidgetBlueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("name"), BlueprintName);
	ResultObj->SetStringField(TEXT("path"), FullPath);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddTextBlockToWidget(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	// Resolve path: use caller-supplied "path" or fall back to default
	FString WidgetPath;
	if (!Params->TryGetStringField(TEXT("path"), WidgetPath) || WidgetPath.IsEmpty())
	{
		WidgetPath = TEXT("/Game/Widgets/");
	}
	if (!WidgetPath.EndsWith(TEXT("/"))) { WidgetPath += TEXT("/"); }
	FString FullPath = WidgetPath + BlueprintName;
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(FullPath));
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found at '%s'"), *BlueprintName, *FullPath));
	}

	// Get optional parameters
	FString InitialText = TEXT("New Text Block");
	Params->TryGetStringField(TEXT("text"), InitialText);

	FVector2D Position(0.0f, 0.0f);
	if (Params->HasField(TEXT("position")))
	{
		const TArray<TSharedPtr<FJsonValue>>* PosArray;
		if (Params->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 2)
		{
			Position.X = (*PosArray)[0]->AsNumber();
			Position.Y = (*PosArray)[1]->AsNumber();
		}
	}

	// Create Text Block widget
	UTextBlock* TextBlock = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *WidgetName);
	if (!TextBlock)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Text Block widget"));
	}

	// Set initial text
	TextBlock->SetText(FText::FromString(InitialText));

	// Add to canvas panel
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBlueprint->WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root Canvas Panel not found"));
	}

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(TextBlock);
	PanelSlot->SetPosition(Position);

	// Mark the package dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("widget_name"), WidgetName);
	ResultObj->SetStringField(TEXT("text"), InitialText);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	// Resolve path: use caller-supplied "path" or fall back to default
	FString WidgetPath;
	if (!Params->TryGetStringField(TEXT("path"), WidgetPath) || WidgetPath.IsEmpty())
	{
		WidgetPath = TEXT("/Game/Widgets/");
	}
	if (!WidgetPath.EndsWith(TEXT("/"))) { WidgetPath += TEXT("/"); }
	FString FullPath = WidgetPath + BlueprintName;
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(FullPath));
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found at '%s'"), *BlueprintName, *FullPath));
	}

	// Get optional Z-order parameter
	int32 ZOrder = 0;
	Params->TryGetNumberField(TEXT("z_order"), ZOrder);

	// Create widget instance
	UClass* WidgetClass = WidgetBlueprint->GeneratedClass;
	if (!WidgetClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get widget class"));
	}

	// Note: This creates the widget but doesn't add it to viewport
	// The actual addition to viewport should be done through Blueprint nodes
	// as it requires a game context

	// Create success response with instructions
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("class_path"), WidgetClass->GetPathName());
	ResultObj->SetNumberField(TEXT("z_order"), ZOrder);
	ResultObj->SetStringField(TEXT("note"), TEXT("Widget class ready. Use CreateWidget and AddToViewport nodes in Blueprint to display in game."));
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddButtonToWidget(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing widget_name parameter"));
		return Response;
	}

	FString ButtonText;
	if (!Params->TryGetStringField(TEXT("text"), ButtonText))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing text parameter"));
		return Response;
	}

	// Resolve path: use caller-supplied "path" or fall back to default
	FString WidgetDir;
	if (!Params->TryGetStringField(TEXT("path"), WidgetDir) || WidgetDir.IsEmpty())
	{
		WidgetDir = TEXT("/Game/Widgets/");
	}
	if (!WidgetDir.EndsWith(TEXT("/"))) { WidgetDir += TEXT("/"); }
	const FString BlueprintPath = FString::Printf(TEXT("%s%s.%s"), *WidgetDir, *BlueprintName, *BlueprintName);
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	if (!WidgetBlueprint)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintPath));
		return Response;
	}

	// Create Button widget
	UButton* Button = NewObject<UButton>(WidgetBlueprint->GeneratedClass->GetDefaultObject(), UButton::StaticClass(), *WidgetName);
	if (!Button)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to create Button widget"));
		return Response;
	}

	// Set button text
	UTextBlock* ButtonTextBlock = NewObject<UTextBlock>(Button, UTextBlock::StaticClass(), *(WidgetName + TEXT("_Text")));
	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetText(FText::FromString(ButtonText));
		Button->AddChild(ButtonTextBlock);
	}

	// Get canvas panel and add button
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBlueprint->WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		Response->SetStringField(TEXT("error"), TEXT("Root widget is not a Canvas Panel"));
		return Response;
	}

	// Add to canvas and set position
	UCanvasPanelSlot* ButtonSlot = RootCanvas->AddChildToCanvas(Button);
	if (ButtonSlot)
	{
		const TArray<TSharedPtr<FJsonValue>>* Position;
		if (Params->TryGetArrayField(TEXT("position"), Position) && Position->Num() >= 2)
		{
			FVector2D Pos(
				(*Position)[0]->AsNumber(),
				(*Position)[1]->AsNumber()
			);
			ButtonSlot->SetPosition(Pos);
		}
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetName);
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleBindWidgetEvent(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing widget_name parameter"));
		return Response;
	}

	FString EventName;
	if (!Params->TryGetStringField(TEXT("event_name"), EventName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing event_name parameter"));
		return Response;
	}

	// Resolve path: use caller-supplied "path" or fall back to default
	FString WidgetDir;
	if (!Params->TryGetStringField(TEXT("path"), WidgetDir) || WidgetDir.IsEmpty())
	{
		WidgetDir = TEXT("/Game/Widgets/");
	}
	if (!WidgetDir.EndsWith(TEXT("/"))) { WidgetDir += TEXT("/"); }

	// Load the Widget Blueprint
	const FString BlueprintPath = FString::Printf(TEXT("%s%s.%s"), *WidgetDir, *BlueprintName, *BlueprintName);
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	if (!WidgetBlueprint)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintPath));
		return Response;
	}

	// Create the event graph if it doesn't exist
	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(WidgetBlueprint);
	if (!EventGraph)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to find or create event graph"));
		return Response;
	}

	// Find the widget in the blueprint
	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(*WidgetName);
	if (!Widget)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to find widget: %s"), *WidgetName));
		return Response;
	}

	// Create the event node (e.g., OnClicked for buttons)
	UK2Node_Event* EventNode = nullptr;
	
	// Find existing nodes first
	TArray<UK2Node_Event*> AllEventNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(WidgetBlueprint, AllEventNodes);
	
	for (UK2Node_Event* Node : AllEventNodes)
	{
		if (Node->CustomFunctionName == FName(*EventName) && Node->EventReference.GetMemberParentClass() == Widget->GetClass())
		{
			EventNode = Node;
			break;
		}
	}

	// If no existing node, create a new one
	if (!EventNode)
	{
		// Calculate position - place it below existing nodes
		float MaxHeight = 0.0f;
		for (UEdGraphNode* Node : EventGraph->Nodes)
		{
			MaxHeight = FMath::Max(MaxHeight, Node->NodePosY);
		}
		
		const FVector2D NodePos(200, MaxHeight + 200);

		// Call CreateNewBoundEventForClass, which returns void, so we can't capture the return value directly
		// We'll need to find the node after creating it
		FKismetEditorUtilities::CreateNewBoundEventForClass(
			Widget->GetClass(),
			FName(*EventName),
			WidgetBlueprint,
			nullptr  // We don't need a specific property binding
		);

		// Now find the newly created node
		TArray<UK2Node_Event*> UpdatedEventNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(WidgetBlueprint, UpdatedEventNodes);
		
		for (UK2Node_Event* Node : UpdatedEventNodes)
		{
			if (Node->CustomFunctionName == FName(*EventName) && Node->EventReference.GetMemberParentClass() == Widget->GetClass())
			{
				EventNode = Node;
				
				// Set position of the node
				EventNode->NodePosX = NodePos.X;
				EventNode->NodePosY = NodePos.Y;
				
				break;
			}
		}
	}

	if (!EventNode)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to create event node"));
		return Response;
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("event_name"), EventName);
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetTextBlockBinding(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing widget_name parameter"));
		return Response;
	}

	FString BindingName;
	if (!Params->TryGetStringField(TEXT("binding_name"), BindingName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing binding_name parameter"));
		return Response;
	}

	// Resolve path: use caller-supplied "path" or fall back to default
	FString WidgetDir;
	if (!Params->TryGetStringField(TEXT("path"), WidgetDir) || WidgetDir.IsEmpty())
	{
		WidgetDir = TEXT("/Game/Widgets/");
	}
	if (!WidgetDir.EndsWith(TEXT("/"))) { WidgetDir += TEXT("/"); }

	// Load the Widget Blueprint
	const FString BlueprintPath = FString::Printf(TEXT("%s%s.%s"), *WidgetDir, *BlueprintName, *BlueprintName);
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	if (!WidgetBlueprint)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintPath));
		return Response;
	}

	// Create a variable for binding if it doesn't exist
	FBlueprintEditorUtils::AddMemberVariable(
		WidgetBlueprint,
		FName(*BindingName),
		FEdGraphPinType(UEdGraphSchema_K2::PC_Text, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType())
	);

	// Find the TextBlock widget
	UTextBlock* TextBlock = Cast<UTextBlock>(WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName)));
	if (!TextBlock)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to find TextBlock widget: %s"), *WidgetName));
		return Response;
	}

	// Create binding function
	const FString FunctionName = FString::Printf(TEXT("Get%s"), *BindingName);
	UEdGraph* FuncGraph = FBlueprintEditorUtils::CreateNewGraph(
		WidgetBlueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (FuncGraph)
	{
		// Add the function to the blueprint with proper template parameter
		// Template requires null for last parameter when not using a signature-source
		FBlueprintEditorUtils::AddFunctionGraph<UClass>(WidgetBlueprint, FuncGraph, false, nullptr);

		// Create entry node
		UK2Node_FunctionEntry* EntryNode = nullptr;
		
		// Create entry node - use the API that exists in UE 5.5
		EntryNode = NewObject<UK2Node_FunctionEntry>(FuncGraph);
		FuncGraph->AddNode(EntryNode, false, false);
		EntryNode->NodePosX = 0;
		EntryNode->NodePosY = 0;
		EntryNode->FunctionReference.SetExternalMember(FName(*FunctionName), WidgetBlueprint->GeneratedClass);
		EntryNode->AllocateDefaultPins();

		// Create get variable node
		UK2Node_VariableGet* GetVarNode = NewObject<UK2Node_VariableGet>(FuncGraph);
		GetVarNode->VariableReference.SetSelfMember(FName(*BindingName));
		FuncGraph->AddNode(GetVarNode, false, false);
		GetVarNode->NodePosX = 200;
		GetVarNode->NodePosY = 0;
		GetVarNode->AllocateDefaultPins();

		// Connect nodes
		UEdGraphPin* EntryThenPin = EntryNode->FindPin(UEdGraphSchema_K2::PN_Then);
		UEdGraphPin* GetVarOutPin = GetVarNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
		if (EntryThenPin && GetVarOutPin)
		{
			EntryThenPin->MakeLinkTo(GetVarOutPin);
		}
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("binding_name"), BindingName);
	return Response;
}

// ---------------------------------------------------------------------------
// Helper: load a WidgetBlueprint from params (blueprint_name + optional path)
// ---------------------------------------------------------------------------
static UWidgetBlueprint* LoadWidgetBP(const TSharedPtr<FJsonObject>& Params, FString& OutError)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		OutError = TEXT("Missing 'blueprint_name' parameter");
		return nullptr;
	}
	FString WidgetDir;
	if (!Params->TryGetStringField(TEXT("path"), WidgetDir) || WidgetDir.IsEmpty())
	{
		WidgetDir = TEXT("/Game/Widgets/");
	}
	if (!WidgetDir.EndsWith(TEXT("/"))) { WidgetDir += TEXT("/"); }
	FString FullPath = WidgetDir + BlueprintName;
	UWidgetBlueprint* WB = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(FullPath));
	if (!WB)
	{
		OutError = FString::Printf(TEXT("Widget Blueprint '%s' not found at '%s'"), *BlueprintName, *FullPath);
	}
	return WB;
}

// ---------------------------------------------------------------------------
// add_image_to_widget
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddImageToWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UWidgetBlueprint* WB = LoadWidgetBP(Params, Error);
	if (!WB) return FUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WB->WidgetTree->RootWidget);
	if (!RootCanvas)
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root widget is not a Canvas Panel"));

	UImage* ImageWidget = WB->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *WidgetName);
	if (!ImageWidget)
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Image widget"));

	// Optional texture path
	FString TexturePath;
	if (Params->TryGetStringField(TEXT("texture"), TexturePath) && !TexturePath.IsEmpty())
	{
		UTexture2D* Tex = Cast<UTexture2D>(UEditorAssetLibrary::LoadAsset(TexturePath));
		if (Tex)
		{
			ImageWidget->SetBrushFromTexture(Tex);
		}
	}

	UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(ImageWidget);
	if (Slot)
	{
		FVector2D Position(0.0f, 0.0f);
		const TArray<TSharedPtr<FJsonValue>>* PosArr;
		if (Params->TryGetArrayField(TEXT("position"), PosArr) && PosArr->Num() >= 2)
		{
			Position.X = (*PosArr)[0]->AsNumber();
			Position.Y = (*PosArr)[1]->AsNumber();
		}
		Slot->SetPosition(Position);

		FVector2D Size(100.0f, 100.0f);
		const TArray<TSharedPtr<FJsonValue>>* SizeArr;
		if (Params->TryGetArrayField(TEXT("size"), SizeArr) && SizeArr->Num() >= 2)
		{
			Size.X = (*SizeArr)[0]->AsNumber();
			Size.Y = (*SizeArr)[1]->AsNumber();
		}
		Slot->SetSize(Size);
	}

	WB->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WB);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	return Result;
}

// ---------------------------------------------------------------------------
// add_progress_bar_to_widget
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddProgressBarToWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UWidgetBlueprint* WB = LoadWidgetBP(Params, Error);
	if (!WB) return FUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WB->WidgetTree->RootWidget);
	if (!RootCanvas)
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root widget is not a Canvas Panel"));

	UProgressBar* Bar = WB->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), *WidgetName);
	if (!Bar)
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create ProgressBar widget"));

	// Optional initial percent (0.0 - 1.0)
	float Percent = 0.0f;
	Params->TryGetNumberField(TEXT("percent"), Percent);
	Bar->SetPercent(Percent);

	UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(Bar);
	if (Slot)
	{
		FVector2D Position(0.0f, 0.0f);
		const TArray<TSharedPtr<FJsonValue>>* PosArr;
		if (Params->TryGetArrayField(TEXT("position"), PosArr) && PosArr->Num() >= 2)
		{
			Position.X = (*PosArr)[0]->AsNumber();
			Position.Y = (*PosArr)[1]->AsNumber();
		}
		Slot->SetPosition(Position);

		FVector2D Size(200.0f, 20.0f);
		const TArray<TSharedPtr<FJsonValue>>* SizeArr;
		if (Params->TryGetArrayField(TEXT("size"), SizeArr) && SizeArr->Num() >= 2)
		{
			Size.X = (*SizeArr)[0]->AsNumber();
			Size.Y = (*SizeArr)[1]->AsNumber();
		}
		Slot->SetSize(Size);
	}

	WB->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WB);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetNumberField(TEXT("percent"), Percent);
	return Result;
}

// ---------------------------------------------------------------------------
// add_horizontal_box_to_widget
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddHorizontalBoxToWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UWidgetBlueprint* WB = LoadWidgetBP(Params, Error);
	if (!WB) return FUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WB->WidgetTree->RootWidget);
	if (!RootCanvas)
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root widget is not a Canvas Panel"));

	UHorizontalBox* HBox = WB->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *WidgetName);
	if (!HBox)
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create HorizontalBox widget"));

	UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(HBox);
	if (Slot)
	{
		FVector2D Position(0.0f, 0.0f);
		const TArray<TSharedPtr<FJsonValue>>* PosArr;
		if (Params->TryGetArrayField(TEXT("position"), PosArr) && PosArr->Num() >= 2)
		{
			Position.X = (*PosArr)[0]->AsNumber();
			Position.Y = (*PosArr)[1]->AsNumber();
		}
		Slot->SetPosition(Position);

		FVector2D Size(400.0f, 100.0f);
		const TArray<TSharedPtr<FJsonValue>>* SizeArr;
		if (Params->TryGetArrayField(TEXT("size"), SizeArr) && SizeArr->Num() >= 2)
		{
			Size.X = (*SizeArr)[0]->AsNumber();
			Size.Y = (*SizeArr)[1]->AsNumber();
		}
		Slot->SetSize(Size);
	}

	WB->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WB);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	return Result;
}

// ---------------------------------------------------------------------------
// add_vertical_box_to_widget
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddVerticalBoxToWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UWidgetBlueprint* WB = LoadWidgetBP(Params, Error);
	if (!WB) return FUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WB->WidgetTree->RootWidget);
	if (!RootCanvas)
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root widget is not a Canvas Panel"));

	UVerticalBox* VBox = WB->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *WidgetName);
	if (!VBox)
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create VerticalBox widget"));

	UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(VBox);
	if (Slot)
	{
		FVector2D Position(0.0f, 0.0f);
		const TArray<TSharedPtr<FJsonValue>>* PosArr;
		if (Params->TryGetArrayField(TEXT("position"), PosArr) && PosArr->Num() >= 2)
		{
			Position.X = (*PosArr)[0]->AsNumber();
			Position.Y = (*PosArr)[1]->AsNumber();
		}
		Slot->SetPosition(Position);

		FVector2D Size(100.0f, 400.0f);
		const TArray<TSharedPtr<FJsonValue>>* SizeArr;
		if (Params->TryGetArrayField(TEXT("size"), SizeArr) && SizeArr->Num() >= 2)
		{
			Size.X = (*SizeArr)[0]->AsNumber();
			Size.Y = (*SizeArr)[1]->AsNumber();
		}
		Slot->SetSize(Size);
	}

	WB->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WB);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	return Result;
}

// ---------------------------------------------------------------------------
// set_widget_visibility
// Params: blueprint_name, widget_name, visibility (Visible/Hidden/Collapsed/HitTestInvisible/SelfHitTestInvisible)
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetWidgetVisibility(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UWidgetBlueprint* WB = LoadWidgetBP(Params, Error);
	if (!WB) return FUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));

	FString VisibilityStr = TEXT("Visible");
	Params->TryGetStringField(TEXT("visibility"), VisibilityStr);

	UWidget* Widget = WB->WidgetTree->FindWidget(*WidgetName);
	if (!Widget)
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));

	ESlateVisibility Vis = ESlateVisibility::Visible;
	if (VisibilityStr == TEXT("Hidden")) Vis = ESlateVisibility::Hidden;
	else if (VisibilityStr == TEXT("Collapsed")) Vis = ESlateVisibility::Collapsed;
	else if (VisibilityStr == TEXT("HitTestInvisible")) Vis = ESlateVisibility::HitTestInvisible;
	else if (VisibilityStr == TEXT("SelfHitTestInvisible")) Vis = ESlateVisibility::SelfHitTestInvisible;

	Widget->SetVisibility(Vis);
	WB->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WB);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetStringField(TEXT("visibility"), VisibilityStr);
	return Result;
}

// ---------------------------------------------------------------------------
// set_widget_anchor
// Params: blueprint_name, widget_name, min_x, min_y, max_x, max_y
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetWidgetAnchor(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UWidgetBlueprint* WB = LoadWidgetBP(Params, Error);
	if (!WB) return FUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));

	UWidget* Widget = WB->WidgetTree->FindWidget(*WidgetName);
	if (!Widget)
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CanvasSlot)
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget '%s' is not in a Canvas Panel"), *WidgetName));

	float MinX = 0.0f, MinY = 0.0f, MaxX = 0.0f, MaxY = 0.0f;
	Params->TryGetNumberField(TEXT("min_x"), MinX);
	Params->TryGetNumberField(TEXT("min_y"), MinY);
	Params->TryGetNumberField(TEXT("max_x"), MaxX);
	Params->TryGetNumberField(TEXT("max_y"), MaxY);

	FAnchors Anchors(MinX, MinY, MaxX, MaxY);
	CanvasSlot->SetAnchors(Anchors);

	WB->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WB);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetNumberField(TEXT("min_x"), MinX);
	Result->SetNumberField(TEXT("min_y"), MinY);
	Result->SetNumberField(TEXT("max_x"), MaxX);
	Result->SetNumberField(TEXT("max_y"), MaxY);
	return Result;
}

// ---------------------------------------------------------------------------
// update_text_block_text
// Params: blueprint_name, widget_name, text
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleUpdateTextBlockText(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UWidgetBlueprint* WB = LoadWidgetBP(Params, Error);
	if (!WB) return FUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));

	FString NewText;
	if (!Params->TryGetStringField(TEXT("text"), NewText))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'text' parameter"));

	UTextBlock* TB = Cast<UTextBlock>(WB->WidgetTree->FindWidget(*WidgetName));
	if (!TB)
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("TextBlock '%s' not found"), *WidgetName));

	TB->SetText(FText::FromString(NewText));
	WB->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WB);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetStringField(TEXT("text"), NewText);
	return Result;
}

// ---------------------------------------------------------------------------
// get_widget_tree
// Returns a JSON tree of all widgets in the blueprint
// ---------------------------------------------------------------------------
static TSharedPtr<FJsonObject> BuildWidgetNode(UWidget* Widget)
{
	TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
	if (!Widget) return Node;
	Node->SetStringField(TEXT("name"), Widget->GetName());
	Node->SetStringField(TEXT("type"), Widget->GetClass()->GetName());

	// Visibility
	static const UEnum* VisEnum = StaticEnum<ESlateVisibility>();
	if (VisEnum)
	{
		Node->SetStringField(TEXT("visibility"), VisEnum->GetNameStringByValue((int64)Widget->Visibility));
	}

	// Position/size if in canvas slot
	if (UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		FVector2D Pos = CSlot->GetPosition();
		FVector2D Sz = CSlot->GetSize();
		TSharedPtr<FJsonObject> SlotInfo = MakeShared<FJsonObject>();
		SlotInfo->SetNumberField(TEXT("x"), Pos.X);
		SlotInfo->SetNumberField(TEXT("y"), Pos.Y);
		SlotInfo->SetNumberField(TEXT("w"), Sz.X);
		SlotInfo->SetNumberField(TEXT("h"), Sz.Y);
		Node->SetObjectField(TEXT("slot"), SlotInfo);
	}

	// Children (panel widgets)
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		TArray<TSharedPtr<FJsonValue>> ChildArr;
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			UWidget* Child = Panel->GetChildAt(i);
			if (Child)
			{
				ChildArr.Add(MakeShared<FJsonValueObject>(BuildWidgetNode(Child)));
			}
		}
		Node->SetArrayField(TEXT("children"), ChildArr);
	}

	return Node;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleGetWidgetTree(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UWidgetBlueprint* WB = LoadWidgetBP(Params, Error);
	if (!WB) return FUnrealMCPCommonUtils::CreateErrorResponse(Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	if (WB->WidgetTree && WB->WidgetTree->RootWidget)
	{
		Result->SetObjectField(TEXT("root"), BuildWidgetNode(WB->WidgetTree->RootWidget));
	}
	else
	{
		Result->SetBoolField(TEXT("empty"), true);
	}
	return Result;
}