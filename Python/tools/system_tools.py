"""
System Tools for Unreal MCP.

Provides editor / MCP server lifecycle management:
  - Detect whether the UE editor process is running
  - Detect whether the MCP TCP server (port 55557) is accepting connections
  - Launch the editor
  - Wait for editor readiness (5-dimensional check)
  - Restart editor gracefully

Requires: psutil (pip install psutil)
"""

import logging
import os
import socket
import subprocess
import sys
import time
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context
from tools.base import send_unreal_command, make_error

logger = logging.getLogger("UnrealMCP")

MCP_HOST = "127.0.0.1"
MCP_PORT = 55557
_EDITOR_PROCESS_NAMES = {"UE5Editor.exe", "UnrealEditor.exe", "UE4Editor.exe"}


def _is_port_open(host: str, port: int, timeout: float = 1.0) -> bool:
    """Return True if TCP port is accepting connections."""
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def _get_editor_process():
    """Return the psutil.Process for the editor, or None if not running."""
    try:
        import psutil
        for proc in psutil.process_iter(["name", "pid"]):
            if proc.info["name"] in _EDITOR_PROCESS_NAMES:
                return proc
    except ImportError:
        logger.warning("psutil not installed; process detection unavailable")
    except Exception as e:
        logger.warning(f"psutil error: {e}")
    return None


def register_system_tools(mcp: FastMCP):
    """Register system / lifecycle tools with the MCP server."""

    @mcp.tool()
    def check_editor_status(ctx: Context) -> Dict[str, Any]:
        """Return a 5-dimensional readiness matrix for the Unreal Editor + MCP server.

        Checks:
          1. editor_process_running – OS-level process detected (requires psutil)
          2. mcp_port_open         – TCP port 55557 accepting connections
          3. mcp_ping              – ping command acknowledged by UnrealMCPBridge
          4. editor_world_loaded   – get_current_level_name succeeds
          5. overall_ready         – all of the above are True
        """
        result: Dict[str, Any] = {
            "success": True,
            "editor_process_running": False,
            "mcp_port_open": False,
            "mcp_ping": False,
            "editor_world_loaded": False,
            "overall_ready": False,
            "details": {},
        }

        # 1. Process check
        proc = _get_editor_process()
        if proc:
            result["editor_process_running"] = True
            result["details"]["pid"] = proc.pid
        else:
            try:
                import psutil  # noqa: F401
                result["details"]["process_note"] = "Editor process not found"
            except ImportError:
                result["details"]["process_note"] = "psutil not installed; process check skipped"

        # 2. Port check
        result["mcp_port_open"] = _is_port_open(MCP_HOST, MCP_PORT)

        # 3. Ping
        if result["mcp_port_open"]:
            ping = send_unreal_command("ping", {})
            result["mcp_ping"] = ping.get("status") != "error"
            result["details"]["ping_response"] = ping

        # 4. World loaded
        if result["mcp_ping"]:
            level = send_unreal_command("get_current_level_name", {})
            world_ok = level.get("status") != "error" and bool(level.get("level_name"))
            result["editor_world_loaded"] = world_ok
            result["details"]["level_name"] = level.get("level_name", "")

        # 5. Overall
        result["overall_ready"] = (
            result["mcp_port_open"]
            and result["mcp_ping"]
            and result["editor_world_loaded"]
        )

        return result

    @mcp.tool()
    def launch_editor(
        ctx: Context,
        project_file: Optional[str] = None,
        editor_exe: Optional[str] = None,
        extra_args: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Launch the Unreal Editor as a background process.

        The editor is started detached; use wait_for_editor_ready to poll
        until the MCP server is accepting commands.

        Args:
            project_file: Absolute path to the .uproject file.
                          Auto-detected from engine path when omitted.
            editor_exe: Absolute path to the editor executable.
                        Auto-detected when omitted.
            extra_args: Additional command-line arguments string.
        """
        # Try to auto-detect paths via engine info if already running
        if not project_file or not editor_exe:
            paths = send_unreal_command("get_engine_path", {})
            if paths.get("status") != "error":
                if not project_file:
                    project_file = paths.get("project_file", "")
                if not editor_exe:
                    engine_dir = paths.get("engine_dir", "")
                    if engine_dir:
                        for candidate in [
                            os.path.join(engine_dir, "Binaries", "Win64", "UnrealEditor.exe"),
                            os.path.join(engine_dir, "Binaries", "Win64", "UE5Editor.exe"),
                        ]:
                            if os.path.isfile(candidate):
                                editor_exe = candidate
                                break

        if not editor_exe or not os.path.isfile(editor_exe):
            return make_error(
                "Editor executable not found. Provide 'editor_exe' parameter."
            )

        cmd = [editor_exe]
        if project_file and os.path.isfile(project_file):
            cmd.append(project_file)
        if extra_args:
            cmd += extra_args.split()

        logger.info(f"Launching editor: {' '.join(cmd)}")
        try:
            proc = subprocess.Popen(
                cmd,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                close_fds=True,
            )
            return {
                "success": True,
                "pid": proc.pid,
                "command": " ".join(cmd),
                "message": "Editor launched. Use wait_for_editor_ready to poll readiness.",
            }
        except Exception as e:
            return make_error(f"Failed to launch editor: {e}")

    @mcp.tool()
    def wait_for_editor_ready(
        ctx: Context,
        timeout_seconds: int = 120,
        poll_interval: float = 3.0,
    ) -> Dict[str, Any]:
        """Poll the editor status until it is fully ready or timeout elapses.

        Args:
            timeout_seconds: Maximum seconds to wait (default 120).
            poll_interval: Seconds between polls (default 3.0).
        """
        deadline = time.time() + timeout_seconds
        attempts = 0

        while time.time() < deadline:
            attempts += 1
            port_open = _is_port_open(MCP_HOST, MCP_PORT, timeout=1.0)
            if port_open:
                ping = send_unreal_command("ping", {})
                if ping.get("status") != "error":
                    level = send_unreal_command("get_current_level_name", {})
                    if level.get("status") != "error" and level.get("level_name"):
                        return {
                            "success": True,
                            "ready": True,
                            "attempts": attempts,
                            "elapsed_seconds": round(timeout_seconds - (deadline - time.time()), 1),
                            "level_name": level.get("level_name"),
                        }
            time.sleep(poll_interval)

        return {
            "success": False,
            "ready": False,
            "attempts": attempts,
            "elapsed_seconds": timeout_seconds,
            "message": f"Editor not ready after {timeout_seconds}s.",
        }

    @mcp.tool()
    def restart_editor(
        ctx: Context,
        project_file: Optional[str] = None,
        editor_exe: Optional[str] = None,
        wait_ready: bool = True,
        timeout_seconds: int = 180,
    ) -> Dict[str, Any]:
        """Gracefully terminate the editor (if running) and relaunch it.

        Args:
            project_file: Optional .uproject file path.
            editor_exe: Optional editor executable path.
            wait_ready: If True, wait until editor is fully ready before returning.
            timeout_seconds: Max seconds to wait for readiness.
        """
        # Terminate existing editor
        terminate_result: Dict[str, Any] = {"terminated": False}
        proc = _get_editor_process()
        if proc:
            try:
                proc.terminate()
                proc.wait(timeout=30)
                terminate_result["terminated"] = True
                terminate_result["pid"] = proc.pid
            except Exception as e:
                terminate_result["terminate_error"] = str(e)

        # Wait briefly for port to close
        time.sleep(3)

        # Launch
        launch_result = send_unreal_command("get_engine_path", {})
        if not project_file and launch_result.get("status") != "error":
            project_file = launch_result.get("project_file", "")
        if not editor_exe and launch_result.get("status") != "error":
            engine_dir = launch_result.get("engine_dir", "")
            if engine_dir:
                for candidate in [
                    os.path.join(engine_dir, "Binaries", "Win64", "UnrealEditor.exe"),
                    os.path.join(engine_dir, "Binaries", "Win64", "UE5Editor.exe"),
                ]:
                    if os.path.isfile(candidate):
                        editor_exe = candidate
                        break

        if not editor_exe or not os.path.isfile(editor_exe):
            return make_error("Cannot restart: editor executable not found.")

        cmd = [editor_exe]
        if project_file and os.path.isfile(project_file):
            cmd.append(project_file)

        try:
            new_proc = subprocess.Popen(
                cmd,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                close_fds=True,
            )
        except Exception as e:
            return make_error(f"Failed to relaunch editor: {e}")

        result: Dict[str, Any] = {
            "success": True,
            "new_pid": new_proc.pid,
            "terminate_info": terminate_result,
        }

        if wait_ready:
            deadline = time.time() + timeout_seconds
            attempts = 0
            ready = False
            while time.time() < deadline:
                attempts += 1
                if _is_port_open(MCP_HOST, MCP_PORT, timeout=1.0):
                    ping = send_unreal_command("ping", {})
                    if ping.get("status") != "error":
                        level = send_unreal_command("get_current_level_name", {})
                        if level.get("status") != "error" and level.get("level_name"):
                            ready = True
                            result["level_name"] = level.get("level_name")
                            break
                time.sleep(3)
            result["ready"] = ready
            result["wait_attempts"] = attempts

        return result

    logger.info("System tools registered successfully")
