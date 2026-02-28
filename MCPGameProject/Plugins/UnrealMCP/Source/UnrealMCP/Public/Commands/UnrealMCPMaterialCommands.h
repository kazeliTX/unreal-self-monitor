#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "MCPCommandRegistry.h"

/**
 * Handler class for Material creation and editing MCP commands.
 * Provides node-graph-level control over UMaterial assets via UMaterialEditingLibrary.
 *
 * Supported expression types (add_material_expression):
 *   Constant, Constant3Vector, Constant4Vector, Fresnel, Multiply, Add, Lerp
 *
 * Material pins (connect_material_property):
 *   BaseColor, Metallic, Roughness, Specular, Opacity, OpacityMask,
 *   EmissiveColor, Normal, Refraction, WorldPositionOffset, AmbientOcclusion
 */
class UNREALMCP_API FUnrealMCPMaterialCommands
{
public:
    FUnrealMCPMaterialCommands();

    /** Register all material commands into the central registry. */
    void RegisterCommands(FMCPCommandRegistry& Registry);

private:
    /** Create a new UMaterial asset at the given content-browser path. */
    TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);

    /**
     * Set a top-level material property.
     * Supported property names:
     *   blend_mode   (string): Opaque | Translucent | Masked | Additive | AlphaComposite | ThinTranslucent
     *   shading_model (string): DefaultLit | Unlit | Subsurface | ThinTranslucent | ClearCoat | Hair | Cloth | Eye
     *   two_sided    (bool)
     */
    TSharedPtr<FJsonObject> HandleSetMaterialProperty(const TSharedPtr<FJsonObject>& Params);

    /**
     * Add an expression node to the material graph.
     * Params:
     *   asset_path   (string) – material asset path
     *   type         (string) – expression type (Constant, Constant3Vector, Fresnel, ...)
     *   node_name    (string) – unique label used for later lookup
     *   value        (float)  – for Constant
     *   r/g/b/a      (float)  – for Constant3Vector / Constant4Vector
     *   exponent     (float)  – for Fresnel
     *   base_reflect_fraction (float) – for Fresnel (default 0.04)
     *   pos_x/pos_y  (int)    – node position in graph (optional)
     */
    TSharedPtr<FJsonObject> HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params);

    /**
     * Connect an expression node output to a material input pin.
     * Params: asset_path, node_name, output_name (use "" for single-output nodes), material_pin
     */
    TSharedPtr<FJsonObject> HandleConnectMaterialProperty(const TSharedPtr<FJsonObject>& Params);

    /**
     * Connect one expression node to another expression's input.
     * Params: asset_path, from_node, from_output, to_node, to_input
     */
    TSharedPtr<FJsonObject> HandleConnectMaterialExpressions(const TSharedPtr<FJsonObject>& Params);

    /** Recompile the material and mark the package dirty. */
    TSharedPtr<FJsonObject> HandleCompileMaterial(const TSharedPtr<FJsonObject>& Params);

    // ── helpers ──────────────────────────────────────────────────────────────

    /** Find a previously-created expression node by its Desc label. */
    class UMaterialExpression* FindExprByName(class UMaterial* Material, const FString& Name) const;

    /** Resolve a user string like "Translucent" to EBlendMode. Returns -1 on failure. */
    int32 ParseBlendMode(const FString& Str) const;

    /** Resolve a user string like "DefaultLit" to EMaterialShadingModel. Returns -1 on failure. */
    int32 ParseShadingModel(const FString& Str) const;

    /** Resolve a pin name string to EMaterialProperty. Returns MP_MAX on failure. */
    int32 ParseMaterialPin(const FString& Str) const;
};
