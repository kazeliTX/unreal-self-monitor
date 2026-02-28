"""
Diagnostics Tools for Unreal MCP.

Provides visual perception and actor inspection via the C++ DiagnosticsCommands module.
Also wraps take_screenshot (from EditorCommands) to return base64-encoded PNG data
so AI clients can analyse viewport content without disk I/O.
"""

import base64
import logging
import os
import tempfile
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context
from tools.base import send_unreal_command, make_error

logger = logging.getLogger("UnrealMCP")


def register_diagnostics_tools(mcp: FastMCP):
    """Register diagnostics tools with the MCP server."""

    @mcp.tool()
    def get_viewport_camera_info(ctx: Context) -> Dict[str, Any]:
        """Get the active editor viewport camera location, rotation, and FOV.

        Returns location (x/y/z), rotation (pitch/yaw/roll), fov, and
        viewport dimensions.
        """
        return send_unreal_command("get_viewport_camera_info", {})

    @mcp.tool()
    def get_actor_screen_position(
        ctx: Context,
        name: str,
    ) -> Dict[str, Any]:
        """Get the screen-space pixel position of an actor in the editor viewport.

        Args:
            name: Actor name or editor label.

        Returns screen_x/screen_y, normalized_x/normalized_y (0–1), and
        is_visible (False when actor is behind camera).
        """
        return send_unreal_command("get_actor_screen_position", {"name": name})

    @mcp.tool()
    def highlight_actor(
        ctx: Context,
        name: str,
    ) -> Dict[str, Any]:
        """Select an actor and move the viewport camera to focus on it.

        Args:
            name: Actor name or editor label to highlight.
        """
        return send_unreal_command("highlight_actor", {"name": name})

    @mcp.tool()
    def take_and_read_screenshot(
        ctx: Context,
        filepath: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Take a viewport screenshot and return its content as a base64-encoded PNG.

        Useful for AI multimodal analysis of the current editor state.

        Args:
            filepath: Optional file path to save the PNG. A temp file is used
                      when omitted. The file is NOT deleted so it can be reused.
        """
        if not filepath:
            tmp = tempfile.NamedTemporaryFile(suffix=".png", delete=False)
            tmp.close()
            filepath = tmp.name

        result = send_unreal_command("take_screenshot", {"filepath": filepath})
        if result.get("status") == "error":
            return result

        # Read PNG and encode to base64
        try:
            with open(filepath, "rb") as f:
                png_bytes = f.read()
            b64 = base64.b64encode(png_bytes).decode("utf-8")
            result["base64_png"] = b64
            result["file_size_bytes"] = len(png_bytes)
        except Exception as e:
            result["base64_error"] = str(e)

        return result

    @mcp.tool()
    def capture_actor_focused(
        ctx: Context,
        name: str,
        filepath: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Highlight an actor, then take a screenshot focused on it.

        Returns base64-encoded PNG plus actor screen position for reference.

        Args:
            name: Actor name or editor label.
            filepath: Optional output PNG path.
        """
        highlight_result = send_unreal_command("highlight_actor", {"name": name})
        if highlight_result.get("status") == "error":
            return highlight_result

        if not filepath:
            tmp = tempfile.NamedTemporaryFile(suffix=".png", delete=False)
            tmp.close()
            filepath = tmp.name

        screenshot_result = send_unreal_command("take_screenshot", {"filepath": filepath})
        if screenshot_result.get("status") == "error":
            return screenshot_result

        try:
            with open(filepath, "rb") as f:
                png_bytes = f.read()
            screenshot_result["base64_png"] = base64.b64encode(png_bytes).decode("utf-8")
            screenshot_result["file_size_bytes"] = len(png_bytes)
        except Exception as e:
            screenshot_result["base64_error"] = str(e)

        screenshot_result["highlighted_actor"] = name
        return screenshot_result

    @mcp.tool()
    def get_scene_overview(ctx: Context) -> Dict[str, Any]:
        """Capture a screenshot and collect camera + level actor info in one call.

        Useful as a single-call 'scene snapshot' for AI situational awareness.
        """
        tmp = tempfile.NamedTemporaryFile(suffix=".png", delete=False)
        tmp.close()

        camera_info = send_unreal_command("get_viewport_camera_info", {})
        screenshot = send_unreal_command("take_screenshot", {"filepath": tmp.name})
        actors = send_unreal_command("get_actors_in_level", {})

        result: Dict[str, Any] = {
            "success": True,
            "camera": camera_info if camera_info.get("success") else {},
            "actor_count": len(actors.get("actors", [])) if actors.get("success") else 0,
            "screenshot_path": tmp.name,
        }

        try:
            with open(tmp.name, "rb") as f:
                result["base64_png"] = base64.b64encode(f.read()).decode("utf-8")
        except Exception as e:
            result["base64_error"] = str(e)

        return result

    logger.info("Diagnostics tools registered successfully")
