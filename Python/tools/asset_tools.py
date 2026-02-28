"""
Asset Tools for Unreal MCP.

This module provides tools for browsing, creating, and managing assets
in the Unreal Engine Content Browser, including DataTable operations.
"""

import logging
from typing import Dict, Any, List, Optional
from mcp.server.fastmcp import FastMCP, Context
from tools.base import send_unreal_command

logger = logging.getLogger("UnrealMCP")


def register_asset_tools(mcp: FastMCP):
    """Register asset management tools with the MCP server."""

    # ------------------------------------------------------------------
    # Asset browsing
    # ------------------------------------------------------------------

    @mcp.tool()
    def list_assets(
        ctx: Context,
        path: str = "/Game/",
        recursive: bool = True,
        filter_class: Optional[str] = None,
    ) -> Dict[str, Any]:
        """List assets in a Content Browser directory.

        Args:
            path: Content Browser directory to search (e.g. '/Game/').
            recursive: Whether to search subdirectories.
            filter_class: Optional class filter (e.g. 'StaticMesh', 'Blueprint').
        """
        params: Dict[str, Any] = {"path": path, "recursive": recursive}
        if filter_class:
            params["filter_class"] = filter_class
        return send_unreal_command("list_assets", params)

    @mcp.tool()
    def find_asset(ctx: Context, name: str, path: str = "/Game/") -> Dict[str, Any]:
        """Find an asset by name in the Content Browser.

        Args:
            name: Asset name to search for (partial match supported).
            path: Root directory to search within.
        """
        return send_unreal_command("find_asset", {"name": name, "path": path})

    @mcp.tool()
    def does_asset_exist(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """Check whether an asset exists at the given path.

        Args:
            asset_path: Full asset path (e.g. '/Game/Meshes/MyMesh').
        """
        return send_unreal_command("does_asset_exist", {"asset_path": asset_path})

    @mcp.tool()
    def get_asset_info(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """Get metadata about an asset (class, size, dependencies).

        Args:
            asset_path: Full asset path.
        """
        return send_unreal_command("get_asset_info", {"asset_path": asset_path})

    # ------------------------------------------------------------------
    # Folder management
    # ------------------------------------------------------------------

    @mcp.tool()
    def create_folder(ctx: Context, path: str) -> Dict[str, Any]:
        """Create a new folder in the Content Browser.

        Args:
            path: Full folder path to create (e.g. '/Game/MyFolder').
        """
        return send_unreal_command("create_folder", {"path": path})

    @mcp.tool()
    def list_folders(ctx: Context, path: str = "/Game/") -> Dict[str, Any]:
        """List sub-folders in a Content Browser directory.

        Args:
            path: Parent directory to list folders from.
        """
        return send_unreal_command("list_folders", {"path": path})

    @mcp.tool()
    def delete_folder(ctx: Context, path: str) -> Dict[str, Any]:
        """Delete a Content Browser folder and all its contents.

        Args:
            path: Full folder path to delete.
        """
        return send_unreal_command("delete_folder", {"path": path})

    # ------------------------------------------------------------------
    # Asset operations
    # ------------------------------------------------------------------

    @mcp.tool()
    def duplicate_asset(
        ctx: Context, source_path: str, dest_path: str
    ) -> Dict[str, Any]:
        """Duplicate an asset to a new location.

        Args:
            source_path: Full path to the source asset.
            dest_path: Full path for the duplicated asset.
        """
        return send_unreal_command(
            "duplicate_asset", {"source_path": source_path, "dest_path": dest_path}
        )

    @mcp.tool()
    def rename_asset(
        ctx: Context, asset_path: str, new_name: str
    ) -> Dict[str, Any]:
        """Rename an asset in the Content Browser.

        Args:
            asset_path: Full path to the asset.
            new_name: New base name for the asset.
        """
        return send_unreal_command(
            "rename_asset", {"asset_path": asset_path, "new_name": new_name}
        )

    @mcp.tool()
    def delete_asset(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """Delete an asset from the Content Browser.

        Args:
            asset_path: Full path to the asset to delete.
        """
        return send_unreal_command("delete_asset", {"asset_path": asset_path})

    @mcp.tool()
    def save_asset(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """Save a specific asset.

        Args:
            asset_path: Full path to the asset to save.
        """
        return send_unreal_command("save_asset", {"asset_path": asset_path})

    @mcp.tool()
    def save_all_assets(ctx: Context) -> Dict[str, Any]:
        """Save all unsaved assets in the project."""
        return send_unreal_command("save_all_assets")

    # ------------------------------------------------------------------
    # DataTable operations
    # ------------------------------------------------------------------

    @mcp.tool()
    def create_data_table(
        ctx: Context,
        name: str,
        row_struct: str,
        path: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Create a new DataTable asset.

        Args:
            name: Name of the DataTable asset.
            row_struct: Name of the struct that defines each row
                        (e.g. 'MyRowStruct').
            path: Optional save directory (e.g. '/Game/Data/').
                  Defaults to '/Game/DataTables/'.
        """
        params: Dict[str, Any] = {"name": name, "row_struct": row_struct}
        if path:
            params["path"] = path
        return send_unreal_command("create_data_table", params)

    @mcp.tool()
    def add_data_table_row(
        ctx: Context,
        table_path: str,
        row_name: str,
        row_data: Dict[str, Any],
    ) -> Dict[str, Any]:
        """Add or replace a row in a DataTable.

        Args:
            table_path: Full asset path of the DataTable.
            row_name: Unique row identifier.
            row_data: Dict of column values for this row.
        """
        return send_unreal_command(
            "add_data_table_row",
            {"table_path": table_path, "row_name": row_name, "row_data": row_data},
        )

    @mcp.tool()
    def get_data_table_rows(ctx: Context, table_path: str) -> Dict[str, Any]:
        """Get all rows from a DataTable as JSON.

        Args:
            table_path: Full asset path of the DataTable.
        """
        return send_unreal_command("get_data_table_rows", {"table_path": table_path})

    logger.info("Asset tools registered successfully")
