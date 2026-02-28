#include "Commands/UnrealMCPMaterialCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Factories/MaterialFactoryNew.h"

#if ENGINE_MAJOR_VERSION >= 5
#include "AssetRegistry/AssetRegistryModule.h"
#else
#include "AssetRegistryModule.h"
#endif

// ---------------------------------------------------------------------------
// Constructor & registration
// ---------------------------------------------------------------------------

FUnrealMCPMaterialCommands::FUnrealMCPMaterialCommands()
{
}

void FUnrealMCPMaterialCommands::RegisterCommands(FMCPCommandRegistry& Registry)
{
    Registry.RegisterCommand(TEXT("create_material"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleCreateMaterial(P); });
    Registry.RegisterCommand(TEXT("set_material_property"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleSetMaterialProperty(P); });
    Registry.RegisterCommand(TEXT("add_material_expression"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleAddMaterialExpression(P); });
    Registry.RegisterCommand(TEXT("connect_material_property"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleConnectMaterialProperty(P); });
    Registry.RegisterCommand(TEXT("connect_material_expressions"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleConnectMaterialExpressions(P); });
    Registry.RegisterCommand(TEXT("compile_material"),
        [this](const TSharedPtr<FJsonObject>& P) { return HandleCompileMaterial(P); });
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

UMaterialExpression* FUnrealMCPMaterialCommands::FindExprByName(UMaterial* Material, const FString& Name) const
{
    if (!Material || Name.IsEmpty()) return nullptr;
#if ENGINE_MAJOR_VERSION >= 5
    for (UMaterialExpression* Expr : Material->GetExpressions())
    {
        if (Expr && Expr->Desc == Name) return Expr;
    }
#else
    for (UMaterialExpression* Expr : Material->Expressions)
    {
        if (Expr && Expr->Desc == Name) return Expr;
    }
#endif
    return nullptr;
}

int32 FUnrealMCPMaterialCommands::ParseBlendMode(const FString& Str) const
{
    if (Str.Equals(TEXT("Opaque"),         ESearchCase::IgnoreCase)) return BLEND_Opaque;
    if (Str.Equals(TEXT("Masked"),         ESearchCase::IgnoreCase)) return BLEND_Masked;
    if (Str.Equals(TEXT("Translucent"),    ESearchCase::IgnoreCase)) return BLEND_Translucent;
    if (Str.Equals(TEXT("Additive"),       ESearchCase::IgnoreCase)) return BLEND_Additive;
    if (Str.Equals(TEXT("Modulate"),       ESearchCase::IgnoreCase)) return BLEND_Modulate;
    if (Str.Equals(TEXT("AlphaComposite"), ESearchCase::IgnoreCase)) return BLEND_AlphaComposite;
#if ENGINE_MAJOR_VERSION >= 5
    if (Str.Equals(TEXT("AlphaHoldout"),   ESearchCase::IgnoreCase)) return BLEND_AlphaHoldout;
#endif
    return -1;
}

int32 FUnrealMCPMaterialCommands::ParseShadingModel(const FString& Str) const
{
    if (Str.Equals(TEXT("Unlit"),            ESearchCase::IgnoreCase)) return MSM_Unlit;
    if (Str.Equals(TEXT("DefaultLit"),       ESearchCase::IgnoreCase)) return MSM_DefaultLit;
    if (Str.Equals(TEXT("Subsurface"),       ESearchCase::IgnoreCase)) return MSM_Subsurface;
    if (Str.Equals(TEXT("ClearCoat"),        ESearchCase::IgnoreCase)) return MSM_ClearCoat;
    if (Str.Equals(TEXT("SubsurfaceProfile"),ESearchCase::IgnoreCase)) return MSM_SubsurfaceProfile;
    if (Str.Equals(TEXT("TwoSidedFoliage"), ESearchCase::IgnoreCase)) return MSM_TwoSidedFoliage;
    if (Str.Equals(TEXT("Hair"),             ESearchCase::IgnoreCase)) return MSM_Hair;
    if (Str.Equals(TEXT("Cloth"),            ESearchCase::IgnoreCase)) return MSM_Cloth;
    if (Str.Equals(TEXT("Eye"),              ESearchCase::IgnoreCase)) return MSM_Eye;
#if ENGINE_MAJOR_VERSION >= 5
    if (Str.Equals(TEXT("ThinTranslucent"), ESearchCase::IgnoreCase)) return MSM_ThinTranslucent;
#endif
    return -1;
}

int32 FUnrealMCPMaterialCommands::ParseMaterialPin(const FString& Str) const
{
    // Returns a small index used by ConnectPin helper, NOT EMaterialProperty
    if (Str.Equals(TEXT("BaseColor"),           ESearchCase::IgnoreCase)) return 0;
    if (Str.Equals(TEXT("Metallic"),            ESearchCase::IgnoreCase)) return 1;
    if (Str.Equals(TEXT("Specular"),            ESearchCase::IgnoreCase)) return 2;
    if (Str.Equals(TEXT("Roughness"),           ESearchCase::IgnoreCase)) return 3;
    if (Str.Equals(TEXT("EmissiveColor"),       ESearchCase::IgnoreCase)) return 4;
    if (Str.Equals(TEXT("Opacity"),             ESearchCase::IgnoreCase)) return 5;
    if (Str.Equals(TEXT("OpacityMask"),         ESearchCase::IgnoreCase)) return 6;
    if (Str.Equals(TEXT("Normal"),              ESearchCase::IgnoreCase)) return 7;
    if (Str.Equals(TEXT("Refraction"),          ESearchCase::IgnoreCase)) return 8;
    if (Str.Equals(TEXT("WorldPositionOffset"), ESearchCase::IgnoreCase)) return 9;
    if (Str.Equals(TEXT("AmbientOcclusion"),    ESearchCase::IgnoreCase)) return 10;
    if (Str.Equals(TEXT("PixelDepthOffset"),    ESearchCase::IgnoreCase)) return 11;
    return -1;
}

/** Directly sets the material input FExpressionInput by pin index.
 *  UE5: input pins live on UMaterialEditorOnlyData, accessed via GetEditorOnlyData().
 *  UE4: input pins are direct fields on UMaterial.
 */
static bool ConnectPin(UMaterial* Material, int32 PinIdx, UMaterialExpression* Expr, int32 OutputIdx)
{
    auto SetInput = [&](FExpressionInput& Input) {
        Input.Expression  = Expr;
        Input.OutputIndex = OutputIdx;
        Input.Mask = Input.MaskR = Input.MaskG = Input.MaskB = Input.MaskA = 0;
    };
#if ENGINE_MAJOR_VERSION >= 5
    UMaterialEditorOnlyData* Ed = Material->GetEditorOnlyData();
    if (!Ed) return false;
    switch (PinIdx)
    {
    case 0:  SetInput(Ed->BaseColor);           return true;
    case 1:  SetInput(Ed->Metallic);            return true;
    case 2:  SetInput(Ed->Specular);            return true;
    case 3:  SetInput(Ed->Roughness);           return true;
    case 4:  SetInput(Ed->EmissiveColor);       return true;
    case 5:  SetInput(Ed->Opacity);             return true;
    case 6:  SetInput(Ed->OpacityMask);         return true;
    case 7:  SetInput(Ed->Normal);              return true;
    case 8:  SetInput(Ed->Refraction);          return true;
    case 9:  SetInput(Ed->WorldPositionOffset); return true;
    case 10: SetInput(Ed->AmbientOcclusion);    return true;
    case 11: SetInput(Ed->PixelDepthOffset);    return true;
    default: return false;
    }
#else
    switch (PinIdx)
    {
    case 0:  SetInput(Material->BaseColor);           return true;
    case 1:  SetInput(Material->Metallic);            return true;
    case 2:  SetInput(Material->Specular);            return true;
    case 3:  SetInput(Material->Roughness);           return true;
    case 4:  SetInput(Material->EmissiveColor);       return true;
    case 5:  SetInput(Material->Opacity);             return true;
    case 6:  SetInput(Material->OpacityMask);         return true;
    case 7:  SetInput(Material->Normal);              return true;
    case 8:  SetInput(Material->Refraction);          return true;
    case 9:  SetInput(Material->WorldPositionOffset); return true;
    case 10: SetInput(Material->AmbientOcclusion);    return true;
    case 11: SetInput(Material->PixelDepthOffset);    return true;
    default: return false;
    }
#endif
}

// ---------------------------------------------------------------------------
// create_material
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path required (e.g. /Game/Materials/M_Ice)"));

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset already exists: %s"), *AssetPath));

    FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
    FString AssetName   = FPackageName::GetLongPackageAssetName(AssetPath);

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMaterial::StaticClass(), Factory);
    UMaterial* Material = Cast<UMaterial>(NewAsset);
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to create Material at: %s"), *AssetPath));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("asset_name"), AssetName);
    return Result;
}

// ---------------------------------------------------------------------------
// set_material_property
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path required"));

    UMaterial* Material = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Material not found: %s"), *AssetPath));

    Material->PreEditChange(nullptr);

    FString BlendModeStr;
    if (Params->TryGetStringField(TEXT("blend_mode"), BlendModeStr))
    {
        int32 BM = ParseBlendMode(BlendModeStr);
        if (BM < 0)
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Unknown blend_mode: %s"), *BlendModeStr));
        Material->BlendMode = static_cast<EBlendMode>(BM);
    }

    FString ShadingStr;
    if (Params->TryGetStringField(TEXT("shading_model"), ShadingStr))
    {
        int32 SM = ParseShadingModel(ShadingStr);
        if (SM < 0)
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Unknown shading_model: %s"), *ShadingStr));
        Material->SetShadingModel(static_cast<EMaterialShadingModel>(SM));
    }

    bool bTwoSided = false;
    if (Params->TryGetBoolField(TEXT("two_sided"), bTwoSided))
        Material->TwoSided = bTwoSided;

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

// ---------------------------------------------------------------------------
// add_material_expression   (no UMaterialEditingLibrary)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path required"));

    FString ExprType;
    if (!Params->TryGetStringField(TEXT("type"), ExprType))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("type required"));

    FString NodeName;
    if (!Params->TryGetStringField(TEXT("node_name"), NodeName) || NodeName.IsEmpty())
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("node_name required"));

    UMaterial* Material = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Material not found: %s"), *AssetPath));

    if (FindExprByName(Material, NodeName))
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Node '%s' already exists"), *NodeName));

    // Resolve class
    UClass* ExprClass = nullptr;
    if      (ExprType.Equals(TEXT("Constant"),        ESearchCase::IgnoreCase)) ExprClass = UMaterialExpressionConstant::StaticClass();
    else if (ExprType.Equals(TEXT("Constant3Vector"), ESearchCase::IgnoreCase)) ExprClass = UMaterialExpressionConstant3Vector::StaticClass();
    else if (ExprType.Equals(TEXT("Constant4Vector"), ESearchCase::IgnoreCase)) ExprClass = UMaterialExpressionConstant4Vector::StaticClass();
    else if (ExprType.Equals(TEXT("Fresnel"),         ESearchCase::IgnoreCase)) ExprClass = UMaterialExpressionFresnel::StaticClass();
    else if (ExprType.Equals(TEXT("Multiply"),        ESearchCase::IgnoreCase)) ExprClass = UMaterialExpressionMultiply::StaticClass();
    else if (ExprType.Equals(TEXT("Add"),             ESearchCase::IgnoreCase)) ExprClass = UMaterialExpressionAdd::StaticClass();
    else if (ExprType.Equals(TEXT("Lerp"),            ESearchCase::IgnoreCase)) ExprClass = UMaterialExpressionLinearInterpolate::StaticClass();
    else
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown type: %s. Supported: Constant, Constant3Vector, Constant4Vector, Fresnel, Multiply, Add, Lerp"), *ExprType));

    // Create expression — Material is the outer, so GC won't collect it
    UMaterialExpression* NewExpr = NewObject<UMaterialExpression>(Material, ExprClass, NAME_None, RF_Transactional);
    if (!NewExpr)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("NewObject failed for expression"));

    NewExpr->Material = Material;
    NewExpr->Desc     = NodeName;

    double PosX = 0, PosY = 0;
    Params->TryGetNumberField(TEXT("pos_x"), PosX);
    Params->TryGetNumberField(TEXT("pos_y"), PosY);
    NewExpr->MaterialExpressionEditorX = static_cast<int32>(PosX);
    NewExpr->MaterialExpressionEditorY = static_cast<int32>(PosY);

    // Set type-specific properties
    if (ExprType.Equals(TEXT("Constant"), ESearchCase::IgnoreCase))
    {
        double Val = 0.0;
        Params->TryGetNumberField(TEXT("value"), Val);
        Cast<UMaterialExpressionConstant>(NewExpr)->R = static_cast<float>(Val);
    }
    else if (ExprType.Equals(TEXT("Constant3Vector"), ESearchCase::IgnoreCase))
    {
        double R = 0, G = 0, B = 0;
        Params->TryGetNumberField(TEXT("r"), R);
        Params->TryGetNumberField(TEXT("g"), G);
        Params->TryGetNumberField(TEXT("b"), B);
        Cast<UMaterialExpressionConstant3Vector>(NewExpr)->Constant =
            FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B));
    }
    else if (ExprType.Equals(TEXT("Constant4Vector"), ESearchCase::IgnoreCase))
    {
        double R = 0, G = 0, B = 0, A = 1;
        Params->TryGetNumberField(TEXT("r"), R);
        Params->TryGetNumberField(TEXT("g"), G);
        Params->TryGetNumberField(TEXT("b"), B);
        Params->TryGetNumberField(TEXT("a"), A);
        Cast<UMaterialExpressionConstant4Vector>(NewExpr)->Constant =
            FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A));
    }
    else if (ExprType.Equals(TEXT("Fresnel"), ESearchCase::IgnoreCase))
    {
        double Exp = 5.0, BaseRefl = 0.04;
        Params->TryGetNumberField(TEXT("exponent"), Exp);
        Params->TryGetNumberField(TEXT("base_reflect_fraction"), BaseRefl);
        UMaterialExpressionFresnel* Fr = Cast<UMaterialExpressionFresnel>(NewExpr);
        Fr->Exponent            = static_cast<float>(Exp);
        Fr->BaseReflectFraction = static_cast<float>(BaseRefl);
    }
    else if (ExprType.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase))
    {
        double CA = 0, CB = 1;
        Params->TryGetNumberField(TEXT("const_a"), CA);
        Params->TryGetNumberField(TEXT("const_b"), CB);
        UMaterialExpressionMultiply* Mul = Cast<UMaterialExpressionMultiply>(NewExpr);
        Mul->ConstA = static_cast<float>(CA);
        Mul->ConstB = static_cast<float>(CB);
    }
    else if (ExprType.Equals(TEXT("Add"), ESearchCase::IgnoreCase))
    {
        double CA = 0, CB = 1;
        Params->TryGetNumberField(TEXT("const_a"), CA);
        Params->TryGetNumberField(TEXT("const_b"), CB);
        UMaterialExpressionAdd* Add = Cast<UMaterialExpressionAdd>(NewExpr);
        Add->ConstA = static_cast<float>(CA);
        Add->ConstB = static_cast<float>(CB);
    }

    // Add to material's expression list
#if ENGINE_MAJOR_VERSION >= 5
    Material->GetExpressionCollection().AddExpression(NewExpr);
#else
    Material->Expressions.Add(NewExpr);
#endif
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("node_name"), NodeName);
    Result->SetStringField(TEXT("type"), ExprType);
    return Result;
}

// ---------------------------------------------------------------------------
// connect_material_property   (direct FExpressionInput assignment)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleConnectMaterialProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path required"));

    FString NodeName;
    if (!Params->TryGetStringField(TEXT("node_name"), NodeName))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("node_name required"));

    FString PinStr;
    if (!Params->TryGetStringField(TEXT("material_pin"), PinStr))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("material_pin required (BaseColor, Roughness, Opacity...)"));

    UMaterial* Material = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Material not found: %s"), *AssetPath));

    UMaterialExpression* Expr = FindExprByName(Material, NodeName);
    if (!Expr)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Node '%s' not found"), *NodeName));

    int32 OutputIdx = 0;
    double OIdx = 0;
    if (Params->TryGetNumberField(TEXT("output_index"), OIdx)) OutputIdx = static_cast<int32>(OIdx);

    int32 PinIdx = ParseMaterialPin(PinStr);
    if (PinIdx < 0)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown material_pin: %s"), *PinStr));

    if (!ConnectPin(Material, PinIdx, Expr, OutputIdx))
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to connect to pin: %s"), *PinStr));

    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("from_node"), NodeName);
    Result->SetStringField(TEXT("to_pin"),    PinStr);
    return Result;
}

// ---------------------------------------------------------------------------
// connect_material_expressions  (node-to-node via GetInputs() API)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleConnectMaterialExpressions(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path required"));

    FString FromNode, FromOutput, ToNode, ToInput;
    if (!Params->TryGetStringField(TEXT("from_node"), FromNode))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("from_node required"));
    if (!Params->TryGetStringField(TEXT("to_node"),   ToNode))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("to_node required"));
    if (!Params->TryGetStringField(TEXT("to_input"),  ToInput))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("to_input required (e.g. A, B, Alpha)"));
    Params->TryGetStringField(TEXT("from_output"), FromOutput);

    UMaterial* Material = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Material not found: %s"), *AssetPath));

    UMaterialExpression* FromExpr = FindExprByName(Material, FromNode);
    UMaterialExpression* ToExpr   = FindExprByName(Material, ToNode);
    if (!FromExpr) return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("from_node '%s' not found"), *FromNode));
    if (!ToExpr)   return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("to_node '%s' not found"), *ToNode));

    // Find from_output index
    int32 FromOutputIdx = 0;
    if (!FromOutput.IsEmpty())
    {
        bool bFound = false;
        TArray<FExpressionOutput>& Outputs = FromExpr->Outputs;
        for (int32 i = 0; i < Outputs.Num(); i++)
        {
            if (Outputs[i].OutputName.ToString().Equals(FromOutput, ESearchCase::IgnoreCase))
            {
                FromOutputIdx = i;
                bFound = true;
                break;
            }
        }
        if (!bFound) FromOutputIdx = 0; // fallback to first output
    }

    // Find to_input and connect — use GetInput(i) loop (GetInputsView deprecated in 5.5+)
    bool bConnected = false;
    for (int32 i = 0; ; ++i)
    {
        FExpressionInput* Input = ToExpr->GetInput(i);
        if (!Input) break;
        FName InputName = ToExpr->GetInputName(i);
        if (InputName.ToString().Equals(ToInput, ESearchCase::IgnoreCase) ||
            FString::FromInt(i).Equals(ToInput))
        {
            Input->Expression  = FromExpr;
            Input->OutputIndex = FromOutputIdx;
            bConnected = true;
            break;
        }
    }

    if (!bConnected)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Input '%s' not found on node '%s'"), *ToInput, *ToNode));

    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("from"), FromNode);
    Result->SetStringField(TEXT("to"),   FString::Printf(TEXT("%s.%s"), *ToNode, *ToInput));
    return Result;
}

// ---------------------------------------------------------------------------
// compile_material
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCompileMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path required"));

    UMaterial* Material = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Material not found: %s"), *AssetPath));

    // Notify editor, trigger shader recompile, and save
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();

    UEditorAssetLibrary::SaveAsset(AssetPath, false);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("status"), TEXT("compiled_and_saved"));
    return Result;
}
