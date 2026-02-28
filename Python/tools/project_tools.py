"""
Project Tools for Unreal MCP.

This module provides tools for managing project-wide settings and configuration.
"""

import logging
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context
from tools.base import send_unreal_command

logger = logging.getLogger("UnrealMCP")


def register_project_tools(mcp: FastMCP):
    """Register project tools with the MCP server."""

    @mcp.tool()
    def create_input_mapping(
        ctx: Context,
        action_name: str,
        key: str,
        input_type: str = "Action"
    ) -> Dict[str, Any]:
        """Create an input mapping for the project.

        Args:
            action_name: Name of the input action
            key: Key to bind (SpaceBar, LeftMouseButton, etc.)
            input_type: Type of input mapping (Action or Axis)
        """
        return send_unreal_command("create_input_mapping", {
            "action_name": action_name,
            "key": key,
            "input_type": input_type,
        })

    @mcp.tool()
    def run_console_command(ctx: Context, command: str) -> Dict[str, Any]:
        """Execute an Unreal Engine console command.

        Useful as a universal escape hatch for operations not covered by
        dedicated MCP commands.

        Args:
            command: Console command string (e.g. 'r.SetRes 1920x1080').
        """
        return send_unreal_command("run_console_command", {"command": command})

    @mcp.tool()
    def get_project_settings(ctx: Context) -> Dict[str, Any]:
        """Get basic project settings (name, company, version, engine version)."""
        return send_unreal_command("get_project_settings")

    @mcp.tool()
    def create_input_action(
        ctx: Context,
        name: str,
        path: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Create an Enhanced Input action asset.

        Args:
            name: Name of the InputAction asset.
            path: Optional save directory (e.g. '/Game/Input/Actions/').
                  Defaults to '/Game/Input/Actions/'.
        """
        params: Dict[str, Any] = {"name": name}
        if path:
            params["path"] = path
        return send_unreal_command("create_input_action", params)

    @mcp.tool()
    def create_input_mapping_context(
        ctx: Context,
        name: str,
        path: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Create an Enhanced Input mapping context asset.

        Args:
            name: Name of the InputMappingContext asset.
            path: Optional save directory. Defaults to '/Game/Input/'.
        """
        params: Dict[str, Any] = {"name": name}
        if path:
            params["path"] = path
        return send_unreal_command("create_input_mapping_context", params)

    @mcp.tool()
    def add_input_mapping(
        ctx: Context,
        context_path: str,
        action_path: str,
        key: str,
    ) -> Dict[str, Any]:
        """Add an action→key mapping to an Enhanced Input mapping context.

        Args:
            context_path: Full asset path to the InputMappingContext.
            action_path: Full asset path to the InputAction.
            key: Key identifier (e.g. 'SpaceBar', 'W', 'Gamepad_LeftX').
        """
        return send_unreal_command("add_input_mapping", {
            "context_path": context_path,
            "action_path": action_path,
            "key": key,
        })

    @mcp.tool()
    def set_input_action_type(
        ctx: Context,
        action_path: str,
        value_type: str,
    ) -> Dict[str, Any]:
        """Set the value type of an Enhanced Input action.

        Args:
            action_path: Full asset path to the InputAction.
            value_type: One of 'Digital', 'Axis1D', 'Axis2D', 'Axis3D'.
        """
        return send_unreal_command("set_input_action_type", {
            "action_path": action_path,
            "value_type": value_type,
        })

    logger.info("Project tools registered successfully")
