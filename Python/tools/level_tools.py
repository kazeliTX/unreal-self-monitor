"""
Level Tools for Unreal MCP.

This module provides tools for managing Unreal Engine levels (maps).
"""

import logging
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context
from tools.base import send_unreal_command

logger = logging.getLogger("UnrealMCP")


def register_level_tools(mcp: FastMCP):
    """Register level management tools with the MCP server."""

    @mcp.tool()
    def new_level(ctx: Context, asset_path: Optional[str] = None) -> Dict[str, Any]:
        """Create a new empty level (map).

        Args:
            asset_path: Optional full asset path (e.g. '/Game/Maps/MyMap').
                        When omitted a transient new level is created.
        """
        params = {}
        if asset_path:
            params["asset_path"] = asset_path
        return send_unreal_command("new_level", params)

    @mcp.tool()
    def open_level(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """Open an existing level by asset path.

        Args:
            asset_path: Full asset path of the level (e.g. '/Game/Maps/MyMap').
        """
        return send_unreal_command("open_level", {"asset_path": asset_path})

    @mcp.tool()
    def save_current_level(ctx: Context) -> Dict[str, Any]:
        """Save the currently open level."""
        return send_unreal_command("save_current_level")

    @mcp.tool()
    def save_all_levels(ctx: Context) -> Dict[str, Any]:
        """Save all dirty (unsaved) levels."""
        return send_unreal_command("save_all_levels")

    @mcp.tool()
    def get_current_level_name(ctx: Context) -> Dict[str, Any]:
        """Get the name and path of the currently open level.

        Returns:
            Dict with 'level_name', 'level_path', and 'package_name' fields.
        """
        return send_unreal_command("get_current_level_name")

    logger.info("Level tools registered successfully")
