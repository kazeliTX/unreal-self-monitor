"""
Editor Tools for Unreal MCP.

This module provides tools for controlling the Unreal Editor viewport and other editor functionality.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context
from tools.base import send_unreal_command, make_error

logger = logging.getLogger("UnrealMCP")


def register_editor_tools(mcp: FastMCP):
    """Register editor tools with the MCP server."""

    @mcp.tool()
    def get_actors_in_level(ctx: Context) -> List[Dict[str, Any]]:
        """Get a list of all actors in the current level."""
        response = send_unreal_command("get_actors_in_level")
        if response.get("success") is False:
            return []
        if "result" in response and "actors" in response["result"]:
            return response["result"]["actors"]
        return response.get("actors", [])

    @mcp.tool()
    def find_actors_by_name(ctx: Context, pattern: str) -> List[str]:
        """Find actors by name pattern."""
        response = send_unreal_command("find_actors_by_name", {"pattern": pattern})
        return response.get("actors", [])

    @mcp.tool()
    def spawn_actor(
        ctx: Context,
        name: str,
        type: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0]
    ) -> Dict[str, Any]:
        """Create a new actor in the current level.

        Args:
            name: The name to give the new actor (must be unique)
            type: The type of actor to create (e.g. StaticMeshActor, PointLight)
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees

        Returns:
            Dict containing the created actor's properties
        """
        params = {
            "name": name,
            "type": type.upper(),
            "location": location,
            "rotation": rotation,
        }
        for param_name in ("location", "rotation"):
            val = params[param_name]
            if not isinstance(val, list) or len(val) != 3:
                return make_error(f"Invalid {param_name} format. Must be a list of 3 float values.")
            params[param_name] = [float(v) for v in val]

        return send_unreal_command("spawn_actor", params)

    @mcp.tool()
    def delete_actor(ctx: Context, name: str) -> Dict[str, Any]:
        """Delete an actor by name."""
        return send_unreal_command("delete_actor", {"name": name})

    @mcp.tool()
    def set_actor_transform(
        ctx: Context,
        name: str,
        location: List[float] = None,
        rotation: List[float] = None,
        scale: List[float] = None
    ) -> Dict[str, Any]:
        """Set the transform of an actor."""
        params = {"name": name}
        if location is not None:
            params["location"] = location
        if rotation is not None:
            params["rotation"] = rotation
        if scale is not None:
            params["scale"] = scale
        return send_unreal_command("set_actor_transform", params)

    @mcp.tool()
    def get_actor_properties(ctx: Context, name: str) -> Dict[str, Any]:
        """Get all properties of an actor."""
        return send_unreal_command("get_actor_properties", {"name": name})

    @mcp.tool()
    def set_actor_property(
        ctx: Context,
        name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """Set a property on an actor.

        Args:
            name: Name of the actor
            property_name: Name of the property to set
            property_value: Value to set the property to
        """
        return send_unreal_command("set_actor_property", {
            "name": name,
            "property_name": property_name,
            "property_value": property_value,
        })

    @mcp.tool()
    def spawn_blueprint_actor(
        ctx: Context,
        blueprint_name: str,
        actor_name: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0],
        asset_path: str = None,
    ) -> Dict[str, Any]:
        """Spawn an actor from a Blueprint.

        Args:
            blueprint_name: Name of the Blueprint to spawn from
            actor_name: Name to give the spawned actor
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees
            asset_path: Optional full asset path (e.g. '/Game/Characters/MyBP').
                        When omitted, defaults to '/Game/Blueprints/<blueprint_name>'.
        """
        params = {
            "blueprint_name": blueprint_name,
            "actor_name": actor_name,
            "location": location or [0.0, 0.0, 0.0],
            "rotation": rotation or [0.0, 0.0, 0.0],
        }
        for param_name in ("location", "rotation"):
            val = params[param_name]
            if not isinstance(val, list) or len(val) != 3:
                return make_error(f"Invalid {param_name} format. Must be a list of 3 float values.")
            params[param_name] = [float(v) for v in val]
        if asset_path:
            params["asset_path"] = asset_path

        return send_unreal_command("spawn_blueprint_actor", params)

    # ------------------------------------------------------------------
    # Actor selection
    # ------------------------------------------------------------------

    @mcp.tool()
    def select_actor(ctx: Context, name: str) -> Dict[str, Any]:
        """Select an actor in the editor by name or label.

        Args:
            name: Actor name or editor label to select.
        """
        return send_unreal_command("select_actor", {"name": name})

    @mcp.tool()
    def deselect_all(ctx: Context) -> Dict[str, Any]:
        """Clear the current editor selection."""
        return send_unreal_command("deselect_all")

    @mcp.tool()
    def get_selected_actors(ctx: Context) -> Dict[str, Any]:
        """Get the list of currently selected actors."""
        return send_unreal_command("get_selected_actors")

    @mcp.tool()
    def duplicate_actor(
        ctx: Context,
        name: str,
        offset: List[float] = [0.0, 0.0, 0.0],
    ) -> Dict[str, Any]:
        """Duplicate an actor and offset it.

        Args:
            name: Name of the actor to duplicate.
            offset: [X, Y, Z] world offset applied to the duplicate.
        """
        return send_unreal_command("duplicate_actor", {"name": name, "offset": offset})

    # ------------------------------------------------------------------
    # Actor label / tag
    # ------------------------------------------------------------------

    @mcp.tool()
    def set_actor_label(ctx: Context, name: str, label: str) -> Dict[str, Any]:
        """Set the editor display label of an actor.

        Args:
            name: Actor name or current label.
            label: New display label.
        """
        return send_unreal_command("set_actor_label", {"name": name, "label": label})

    @mcp.tool()
    def get_actor_label(ctx: Context, name: str) -> Dict[str, Any]:
        """Get the editor display label of an actor.

        Args:
            name: Actor name.
        """
        return send_unreal_command("get_actor_label", {"name": name})

    @mcp.tool()
    def add_actor_tag(ctx: Context, name: str, tag: str) -> Dict[str, Any]:
        """Add a gameplay tag to an actor.

        Args:
            name: Actor name or label.
            tag: Tag string to add.
        """
        return send_unreal_command("add_actor_tag", {"name": name, "tag": tag})

    @mcp.tool()
    def remove_actor_tag(ctx: Context, name: str, tag: str) -> Dict[str, Any]:
        """Remove a gameplay tag from an actor.

        Args:
            name: Actor name or label.
            tag: Tag string to remove.
        """
        return send_unreal_command("remove_actor_tag", {"name": name, "tag": tag})

    @mcp.tool()
    def get_actor_tags(ctx: Context, name: str) -> Dict[str, Any]:
        """Get all gameplay tags on an actor.

        Args:
            name: Actor name or label.
        """
        return send_unreal_command("get_actor_tags", {"name": name})

    # ------------------------------------------------------------------
    # Actor attachment
    # ------------------------------------------------------------------

    @mcp.tool()
    def attach_actor_to_actor(
        ctx: Context,
        child_name: str,
        parent_name: str,
        socket_name: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Attach one actor to another.

        Args:
            child_name: Name of the actor to attach.
            parent_name: Name of the actor to attach to.
            socket_name: Optional socket name on the parent.
        """
        params: Dict[str, Any] = {
            "child_name": child_name,
            "parent_name": parent_name,
        }
        if socket_name:
            params["socket_name"] = socket_name
        return send_unreal_command("attach_actor_to_actor", params)

    @mcp.tool()
    def detach_actor(ctx: Context, name: str) -> Dict[str, Any]:
        """Detach an actor from its parent.

        Args:
            name: Actor name to detach.
        """
        return send_unreal_command("detach_actor", {"name": name})

    # ------------------------------------------------------------------
    # World settings
    # ------------------------------------------------------------------

    @mcp.tool()
    def get_world_settings(ctx: Context) -> Dict[str, Any]:
        """Get current World Settings (gravity, game mode, etc.)."""
        return send_unreal_command("get_world_settings")

    @mcp.tool()
    def set_world_settings(
        ctx: Context,
        global_gravity_z: Optional[float] = None,
        game_mode: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Modify World Settings.

        Args:
            global_gravity_z: Gravity along the Z axis (default -980 cm/s²).
            game_mode: Class name of the default game mode (e.g. 'MyGameMode').
        """
        params: Dict[str, Any] = {}
        if global_gravity_z is not None:
            params["global_gravity_z"] = global_gravity_z
        if game_mode is not None:
            params["game_mode"] = game_mode
        return send_unreal_command("set_world_settings", params)

    logger.info("Editor tools registered successfully")
