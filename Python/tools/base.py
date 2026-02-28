"""
Common helpers for Unreal MCP tools.
"""

import logging
from typing import Dict, Any

logger = logging.getLogger("UnrealMCP")


def send_unreal_command(command: str, params: Dict[str, Any] = None) -> Dict[str, Any]:
    """Send a command to Unreal Engine and return the response.

    Handles connection retrieval, None-response guard, and exception logging so
    individual tool functions don't need to repeat this boilerplate.
    """
    from unreal_mcp_server import get_unreal_connection
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        response = unreal.send_command(command, params or {})
        if response is None:
            return {"success": False, "message": "No response from Unreal Engine"}
        return response
    except Exception as e:
        logger.error(f"[{command}] Error: {e}")
        return {"success": False, "message": str(e)}


def make_error(message: str) -> Dict[str, Any]:
    """Return a standard error dict."""
    return {"success": False, "message": message}
