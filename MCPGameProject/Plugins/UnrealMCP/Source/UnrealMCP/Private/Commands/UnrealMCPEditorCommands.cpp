#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "ActorEditorUtils.h"

FUnrealMCPEditorCommands::FUnrealMCPEditorCommands()
{
}

void FUnrealMCPEditorCommands::RegisterCommands(FMCPCommandRegistry& Registry)
{
    // Actor manipulation commands
    Registry.RegisterCommand(TEXT("get_actors_in_level"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetActorsInLevel(P); });
    Registry.RegisterCommand(TEXT("find_actors_by_name"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleFindActorsByName(P); });
    Registry.RegisterCommand(TEXT("spawn_actor"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleSpawnActor(P); });
    // create_actor is a deprecated alias for spawn_actor
    Registry.RegisterCommand(TEXT("create_actor"),
        [this](const TSharedPtr<FJsonObject>& P)
        {
            UE_LOG(LogTemp, Warning, TEXT("'create_actor' is deprecated. Use 'spawn_actor' instead."));
            return HandleSpawnActor(P);
        });
    Registry.RegisterCommand(TEXT("delete_actor"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleDeleteActor(P); });
    Registry.RegisterCommand(TEXT("set_actor_transform"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleSetActorTransform(P); });
    Registry.RegisterCommand(TEXT("get_actor_properties"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetActorProperties(P); });
    Registry.RegisterCommand(TEXT("set_actor_property"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleSetActorProperty(P); });
    // Blueprint actor spawning
    Registry.RegisterCommand(TEXT("spawn_blueprint_actor"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleSpawnBlueprintActor(P); });
    // Editor viewport commands
    Registry.RegisterCommand(TEXT("focus_viewport"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleFocusViewport(P); });
    Registry.RegisterCommand(TEXT("take_screenshot"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleTakeScreenshot(P); });

    // Actor selection
    Registry.RegisterCommand(TEXT("select_actor"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleSelectActor(P); });
    Registry.RegisterCommand(TEXT("deselect_all"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleDeselectAll(P); });
    Registry.RegisterCommand(TEXT("get_selected_actors"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetSelectedActors(P); });
    Registry.RegisterCommand(TEXT("duplicate_actor"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleDuplicateActor(P); });

    // Actor label
    Registry.RegisterCommand(TEXT("set_actor_label"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleSetActorLabel(P); });
    Registry.RegisterCommand(TEXT("get_actor_label"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetActorLabel(P); });

    // Actor hierarchy
    Registry.RegisterCommand(TEXT("attach_actor_to_actor"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleAttachActorToActor(P); });
    Registry.RegisterCommand(TEXT("detach_actor"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleDetachActor(P); });

    // Actor tags
    Registry.RegisterCommand(TEXT("add_actor_tag"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleAddActorTag(P); });
    Registry.RegisterCommand(TEXT("remove_actor_tag"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleRemoveActorTag(P); });
    Registry.RegisterCommand(TEXT("get_actor_tags"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetActorTags(P); });

    // World settings
    Registry.RegisterCommand(TEXT("get_world_settings"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleGetWorldSettings(P); });
    Registry.RegisterCommand(TEXT("set_world_settings"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleSetWorldSettings(P); });
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    if (ActorType == TEXT("StaticMeshActor"))
    {
        NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("PointLight"))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
    }

    if (NewActor)
    {
        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Return the created actor's details
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
            Actor->Destroy();
            
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Always return detailed properties for this command
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get property name
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Get property value
    if (!Params->HasField(TEXT("property_value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
    }
    
    TSharedPtr<FJsonValue> PropertyValue = Params->Values.FindRef(TEXT("property_value"));
    
    // Set the property using our utility function
    FString ErrorMessage;
    if (FUnrealMCPCommonUtils::SetObjectProperty(TargetActor, PropertyName, PropertyValue, ErrorMessage))
    {
        // Property set successfully
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("actor"), ActorName);
        ResultObj->SetStringField(TEXT("property"), PropertyName);
        ResultObj->SetBoolField(TEXT("success"), true);
        
        // Also include the full actor details
        ResultObj->SetObjectField(TEXT("actor_details"), FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true));
        return ResultObj;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // Find the blueprint
    if (BlueprintName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint name is empty"));
    }

    // Resolve asset path: caller may supply "asset_path" (full path) or
    // "path" (directory prefix); otherwise fall back to the default location.
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        FString PathPrefix;
        if (!Params->TryGetStringField(TEXT("path"), PathPrefix) || PathPrefix.IsEmpty())
        {
            PathPrefix = TEXT("/Game/Blueprints/");
        }
        if (!PathPrefix.EndsWith(TEXT("/")))
        {
            PathPrefix += TEXT("/");
        }
        AssetPath = PathPrefix + BlueprintName;
    }

    if (!FPackageName::DoesPackageExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Blueprint '%s' not found at path '%s'"), *BlueprintName, *AssetPath));
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Spawn the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));
    SpawnTransform.SetScale3D(Scale);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform, SpawnParams);
    if (NewActor)
    {
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFocusViewport(const TSharedPtr<FJsonObject>& Params)
{
    // Get target actor name if provided
    FString TargetActorName;
    bool HasTargetActor = Params->TryGetStringField(TEXT("target"), TargetActorName);

    // Get location if provided
    FVector Location(0.0f, 0.0f, 0.0f);
    bool HasLocation = false;
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        HasLocation = true;
    }

    // Get distance
    float Distance = 1000.0f;
    if (Params->HasField(TEXT("distance")))
    {
        Distance = Params->GetNumberField(TEXT("distance"));
    }

    // Get orientation if provided
    FRotator Orientation(0.0f, 0.0f, 0.0f);
    bool HasOrientation = false;
    if (Params->HasField(TEXT("orientation")))
    {
        Orientation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("orientation"));
        HasOrientation = true;
    }

    // Get the active viewport
    FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
    if (!ViewportClient)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get active viewport"));
    }

    // If we have a target actor, focus on it
    if (HasTargetActor)
    {
        // Find the actor
        AActor* TargetActor = nullptr;
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
        
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == TargetActorName)
            {
                TargetActor = Actor;
                break;
            }
        }

        if (!TargetActor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *TargetActorName));
        }

        // Focus on the actor
        ViewportClient->SetViewLocation(TargetActor->GetActorLocation() - FVector(Distance, 0.0f, 0.0f));
    }
    // Otherwise use the provided location
    else if (HasLocation)
    {
        ViewportClient->SetViewLocation(Location - FVector(Distance, 0.0f, 0.0f));
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Either 'target' or 'location' must be provided"));
    }

    // Set orientation if provided
    if (HasOrientation)
    {
        ViewportClient->SetViewRotation(Orientation);
    }

    // Force viewport to redraw
    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    // Get file path parameter
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("filepath"), FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'filepath' parameter"));
    }
    
    // Ensure the file path has a proper extension
    if (!FilePath.EndsWith(TEXT(".png")))
    {
        FilePath += TEXT(".png");
    }

    // Get the active viewport
    if (GEditor && GEditor->GetActiveViewport())
    {
        FViewport* Viewport = GEditor->GetActiveViewport();
        TArray<FColor> Bitmap;
        FIntRect ViewportRect(0, 0, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);
        
        if (Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewportRect))
        {
            TArray<uint8> CompressedBitmap;
            FImageUtils::CompressImageArray(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, Bitmap, CompressedBitmap);
            
            if (FFileHelper::SaveArrayToFile(CompressedBitmap, *FilePath))
            {
                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetStringField(TEXT("filepath"), FilePath);
                return ResultObj;
            }
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to take screenshot"));
}

// ---------------------------------------------------------------------------
// Actor selection
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSelectActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    bool bAddToSelection = false;
    if (Params->HasField(TEXT("add_to_selection")))
    {
        bAddToSelection = Params->GetBoolField(TEXT("add_to_selection"));
    }

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    if (!bAddToSelection)
    {
        GEditor->SelectNone(true, true);
    }
    GEditor->SelectActor(TargetActor, true, true);
    GEditor->NoteSelectionChange();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), TargetActor->GetName());
    Result->SetStringField(TEXT("actor_label"), TargetActor->GetActorLabel());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDeselectAll(const TSharedPtr<FJsonObject>& Params)
{
    GEditor->SelectNone(true, true);
    GEditor->NoteSelectionChange();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params)
{
    USelection* Selection = GEditor->GetSelectedActors();
    TArray<TSharedPtr<FJsonValue>> ActorArray;

    for (FSelectionIterator It(*Selection); It; ++It)
    {
        AActor* Actor = Cast<AActor>(*It);
        if (Actor)
        {
            TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
            ActorObj->SetStringField(TEXT("name"), Actor->GetName());
            ActorObj->SetStringField(TEXT("label"), Actor->GetActorLabel());
            ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
            ActorArray.Add(MakeShared<FJsonValueObject>(ActorObj));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("actors"), ActorArray);
    Result->SetNumberField(TEXT("count"), static_cast<double>(ActorArray.Num()));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    AActor* SourceActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            SourceActor = Actor;
            break;
        }
    }

    if (!SourceActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Use EditorActorSubsystem for clean duplication
    UEditorActorSubsystem* EdActorSub = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!EdActorSub)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get EditorActorSubsystem"));
    }

    TArray<AActor*> ToDuplicate = { SourceActor };
    TArray<AActor*> Duplicated = EdActorSub->DuplicateActors(ToDuplicate);
    if (Duplicated.Num() == 0 || !Duplicated[0])
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to duplicate actor"));
    }

    AActor* NewActor = Duplicated[0];

    // Optional: set offset position
    if (Params->HasField(TEXT("location")))
    {
        FVector NewLocation = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        NewActor->SetActorLocation(NewLocation);
    }
    else
    {
        // Default offset so it's visible
        NewActor->SetActorLocation(SourceActor->GetActorLocation() + FVector(50.f, 50.f, 0.f));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), NewActor->GetName());
    Result->SetStringField(TEXT("actor_label"), NewActor->GetActorLabel());
    return Result;
}

// ---------------------------------------------------------------------------
// Actor label
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorLabel(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString NewLabel;
    if (!Params->TryGetStringField(TEXT("label"), NewLabel))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'label' parameter"));
    }

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    TargetActor->SetActorLabel(NewLabel);
    GWorld->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), TargetActor->GetName());
    Result->SetStringField(TEXT("label"), TargetActor->GetActorLabel());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorLabel(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("actor_name"), Actor->GetName());
            Result->SetStringField(TEXT("label"), Actor->GetActorLabel());
            return Result;
        }
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

// ---------------------------------------------------------------------------
// Actor hierarchy
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleAttachActorToActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ChildName;
    if (!Params->TryGetStringField(TEXT("child_name"), ChildName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'child_name' parameter"));
    }

    FString ParentName;
    if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_name' parameter"));
    }

    AActor* ChildActor = nullptr;
    AActor* ParentActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (!Actor) continue;
        if (Actor->GetName() == ChildName || Actor->GetActorLabel() == ChildName) ChildActor = Actor;
        if (Actor->GetName() == ParentName || Actor->GetActorLabel() == ParentName) ParentActor = Actor;
    }

    if (!ChildActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Child actor not found: %s"), *ChildName));
    }
    if (!ParentActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Parent actor not found: %s"), *ParentName));
    }

    FString SocketName;
    FAttachmentTransformRules Rules = FAttachmentTransformRules::KeepWorldTransform;
    if (Params->TryGetStringField(TEXT("socket_name"), SocketName) && !SocketName.IsEmpty())
    {
        ChildActor->AttachToActor(ParentActor, Rules, FName(*SocketName));
    }
    else
    {
        ChildActor->AttachToActor(ParentActor, Rules);
    }

    GWorld->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("child"), ChildActor->GetName());
    Result->SetStringField(TEXT("parent"), ParentActor->GetName());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDetachActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    TargetActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    GWorld->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), TargetActor->GetName());
    return Result;
}

// ---------------------------------------------------------------------------
// Actor tags
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleAddActorTag(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString Tag;
    if (!Params->TryGetStringField(TEXT("tag"), Tag) || Tag.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'tag' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            Actor->Tags.AddUnique(FName(*Tag));
            GWorld->MarkPackageDirty();

            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("actor_name"), Actor->GetName());
            Result->SetStringField(TEXT("tag"), Tag);

            TArray<TSharedPtr<FJsonValue>> TagArray;
            for (const FName& T : Actor->Tags)
            {
                TagArray.Add(MakeShared<FJsonValueString>(T.ToString()));
            }
            Result->SetArrayField(TEXT("all_tags"), TagArray);
            return Result;
        }
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleRemoveActorTag(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString Tag;
    if (!Params->TryGetStringField(TEXT("tag"), Tag) || Tag.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'tag' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            Actor->Tags.Remove(FName(*Tag));
            GWorld->MarkPackageDirty();

            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("actor_name"), Actor->GetName());

            TArray<TSharedPtr<FJsonValue>> TagArray;
            for (const FName& T : Actor->Tags)
            {
                TagArray.Add(MakeShared<FJsonValueString>(T.ToString()));
            }
            Result->SetArrayField(TEXT("all_tags"), TagArray);
            return Result;
        }
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorTags(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            TArray<TSharedPtr<FJsonValue>> TagArray;
            for (const FName& T : Actor->Tags)
            {
                TagArray.Add(MakeShared<FJsonValueString>(T.ToString()));
            }

            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("actor_name"), Actor->GetName());
            Result->SetArrayField(TEXT("tags"), TagArray);
            return Result;
        }
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

// ---------------------------------------------------------------------------
// World settings
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetWorldSettings(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world found"));
    }

    AWorldSettings* WS = World->GetWorldSettings();
    if (!WS)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No WorldSettings found"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("world_gravity_z"), WS->WorldGravityZ);
    Result->SetNumberField(TEXT("global_gravity_z"), WS->bGlobalGravitySet ? WS->GlobalGravityZ : WS->WorldGravityZ);
    Result->SetBoolField(TEXT("global_gravity_override"), WS->bGlobalGravitySet);
    Result->SetStringField(TEXT("game_mode"),
        WS->DefaultGameMode ? WS->DefaultGameMode->GetName() : TEXT("None"));
    Result->SetStringField(TEXT("game_mode_path"),
        WS->DefaultGameMode ? WS->DefaultGameMode->GetPathName() : TEXT(""));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetWorldSettings(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world found"));
    }

    AWorldSettings* WS = World->GetWorldSettings();
    if (!WS)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No WorldSettings found"));
    }

    bool bModified = false;

    if (Params->HasField(TEXT("global_gravity_z")))
    {
        WS->GlobalGravityZ = static_cast<float>(Params->GetNumberField(TEXT("global_gravity_z")));
        WS->bGlobalGravitySet = true;
        bModified = true;
    }

    if (Params->HasField(TEXT("game_mode")))
    {
        FString GameModeClass;
        Params->TryGetStringField(TEXT("game_mode"), GameModeClass);
        UClass* GMClass = FindObject<UClass>(ANY_PACKAGE, *GameModeClass);
        if (!GMClass)
        {
            GMClass = LoadObject<UClass>(nullptr, *GameModeClass);
        }
        if (GMClass && GMClass->IsChildOf(AGameModeBase::StaticClass()))
        {
            WS->DefaultGameMode = GMClass;
            bModified = true;
        }
        else
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("GameMode class '%s' not found or not a GameModeBase subclass"), *GameModeClass));
        }
    }

    if (bModified)
    {
        WS->MarkPackageDirty();
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("global_gravity_z"), WS->bGlobalGravitySet ? WS->GlobalGravityZ : WS->WorldGravityZ);
    Result->SetStringField(TEXT("game_mode"),
        WS->DefaultGameMode ? WS->DefaultGameMode->GetName() : TEXT("None"));
    return Result;
}