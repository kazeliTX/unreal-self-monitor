"""
Blueprint Tools for Unreal MCP.

This module provides tools for creating and manipulating Blueprint assets in Unreal Engine.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context
from tools.base import send_unreal_command, make_error

logger = logging.getLogger("UnrealMCP")


def register_blueprint_tools(mcp: FastMCP):
    """Register Blueprint tools with the MCP server."""

    @mcp.tool()
    def create_blueprint(
        ctx: Context,
        name: str,
        parent_class: str,
        path: str = None,
    ) -> Dict[str, Any]:
        """Create a new Blueprint class.

        Args:
            name: Name of the new Blueprint
            parent_class: Parent class (e.g. 'Actor', 'Pawn', 'Character')
            path: Optional save directory (e.g. '/Game/Blueprints/').
                  Defaults to '/Game/Blueprints/' when omitted.
        """
        params = {"name": name, "parent_class": parent_class}
        if path:
            params["path"] = path
        return send_unreal_command("create_blueprint", params)

    @mcp.tool()
    def add_component_to_blueprint(
        ctx: Context,
        blueprint_name: str,
        component_type: str,
        component_name: str,
        location: List[float] = [],
        rotation: List[float] = [],
        scale: List[float] = [],
        component_properties: Dict[str, Any] = {}
    ) -> Dict[str, Any]:
        """Add a component to a Blueprint.

        Args:
            blueprint_name: Name of the target Blueprint
            component_type: Type of component to add (class name without U prefix)
            component_name: Name for the new component
            location: [X, Y, Z] coordinates for component's position
            rotation: [Pitch, Yaw, Roll] values for component's rotation
            scale: [X, Y, Z] values for component's scale
            component_properties: Additional properties to set on the component
        """
        params = {
            "blueprint_name": blueprint_name,
            "component_type": component_type,
            "component_name": component_name,
            "location": location or [0.0, 0.0, 0.0],
            "rotation": rotation or [0.0, 0.0, 0.0],
            "scale": scale or [1.0, 1.0, 1.0],
        }
        for param_name in ("location", "rotation", "scale"):
            val = params[param_name]
            if not isinstance(val, list) or len(val) != 3:
                return make_error(f"Invalid {param_name} format. Must be a list of 3 float values.")
            params[param_name] = [float(v) for v in val]
        if component_properties:
            params["component_properties"] = component_properties

        return send_unreal_command("add_component_to_blueprint", params)

    @mcp.tool()
    def set_static_mesh_properties(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        static_mesh: str = "/Engine/BasicShapes/Cube.Cube"
    ) -> Dict[str, Any]:
        """Set static mesh properties on a StaticMeshComponent.

        Args:
            blueprint_name: Name of the target Blueprint
            component_name: Name of the StaticMeshComponent
            static_mesh: Path to the static mesh asset
        """
        return send_unreal_command("set_static_mesh_properties", {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "static_mesh": static_mesh,
        })

    @mcp.tool()
    def set_component_property(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """Set a property on a component in a Blueprint."""
        return send_unreal_command("set_component_property", {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "property_name": property_name,
            "property_value": property_value,
        })

    @mcp.tool()
    def set_physics_properties(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        simulate_physics: bool = True,
        gravity_enabled: bool = True,
        mass: float = 1.0,
        linear_damping: float = 0.01,
        angular_damping: float = 0.0
    ) -> Dict[str, Any]:
        """Set physics properties on a component."""
        return send_unreal_command("set_physics_properties", {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "simulate_physics": simulate_physics,
            "gravity_enabled": gravity_enabled,
            "mass": float(mass),
            "linear_damping": float(linear_damping),
            "angular_damping": float(angular_damping),
        })

    @mcp.tool()
    def compile_blueprint(ctx: Context, blueprint_name: str) -> Dict[str, Any]:
        """Compile a Blueprint."""
        return send_unreal_command("compile_blueprint", {"blueprint_name": blueprint_name})

    @mcp.tool()
    def set_blueprint_property(
        ctx: Context,
        blueprint_name: str,
        property_name: str,
        property_value
    ) -> Dict[str, Any]:
        """Set a property on a Blueprint class default object.

        Args:
            blueprint_name: Name of the target Blueprint
            property_name: Name of the property to set
            property_value: Value to set the property to
        """
        return send_unreal_command("set_blueprint_property", {
            "blueprint_name": blueprint_name,
            "property_name": property_name,
            "property_value": property_value,
        })

    # ------------------------------------------------------------------
    # Blueprint query commands
    # ------------------------------------------------------------------

    @mcp.tool()
    def get_blueprint_variables(
        ctx: Context, blueprint_name: str
    ) -> Dict[str, Any]:
        """Get all variables defined in a Blueprint.

        Args:
            blueprint_name: Name of the Blueprint asset.
        """
        return send_unreal_command("get_blueprint_variables", {
            "blueprint_name": blueprint_name
        })

    @mcp.tool()
    def get_blueprint_functions(
        ctx: Context, blueprint_name: str
    ) -> Dict[str, Any]:
        """Get all functions (graphs) defined in a Blueprint.

        Args:
            blueprint_name: Name of the Blueprint asset.
        """
        return send_unreal_command("get_blueprint_functions", {
            "blueprint_name": blueprint_name
        })

    @mcp.tool()
    def get_blueprint_components(
        ctx: Context, blueprint_name: str
    ) -> Dict[str, Any]:
        """Get all components in a Blueprint's construction script.

        Args:
            blueprint_name: Name of the Blueprint asset.
        """
        return send_unreal_command("get_blueprint_components", {
            "blueprint_name": blueprint_name
        })

    @mcp.tool()
    def list_blueprints(
        ctx: Context,
        path: str = "/Game/",
        filter_type: str = "Blueprint",
    ) -> Dict[str, Any]:
        """List Blueprint assets in the Content Browser.

        Args:
            path: Root path to search.
            filter_type: 'Blueprint', 'AnimBlueprint', or 'WidgetBlueprint'.
        """
        return send_unreal_command("list_blueprints", {
            "path": path,
            "filter_type": filter_type,
        })

    @mcp.tool()
    def get_blueprint_compile_errors(
        ctx: Context, blueprint_name: str
    ) -> Dict[str, Any]:
        """Compile a Blueprint and return any errors.

        Args:
            blueprint_name: Name of the Blueprint asset.
        """
        return send_unreal_command("get_blueprint_compile_errors", {
            "blueprint_name": blueprint_name
        })

    # ------------------------------------------------------------------
    # Collision commands
    # ------------------------------------------------------------------

    @mcp.tool()
    def set_component_collision_profile(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        profile_name: str,
    ) -> Dict[str, Any]:
        """Set the collision profile on a Blueprint component.

        Args:
            blueprint_name: Name of the Blueprint asset.
            component_name: Name of the component.
            profile_name: Collision profile name (e.g. 'BlockAll', 'OverlapAll', 'NoCollision').
        """
        return send_unreal_command("set_component_collision_profile", {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "profile_name": profile_name,
        })

    @mcp.tool()
    def set_component_collision_enabled(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        enabled: bool = True,
    ) -> Dict[str, Any]:
        """Enable or disable collision on a Blueprint component.

        Args:
            blueprint_name: Name of the Blueprint asset.
            component_name: Name of the component.
            enabled: Whether collision should be enabled.
        """
        return send_unreal_command("set_component_collision_enabled", {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "enabled": enabled,
        })

    logger.info("Blueprint tools registered successfully")
