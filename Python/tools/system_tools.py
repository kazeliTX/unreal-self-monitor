"""
System Tools for Unreal MCP.

Provides editor / MCP server lifecycle management:
  - Detect whether the UE editor process is running
  - Detect whether the MCP TCP server (port 55557) is accepting connections
  - Launch the editor (offline engine-path detection via .uproject + Windows registry)
  - Wait for editor readiness (5-dimensional check)
  - Restart editor gracefully (PowerShell kill + PackageRestoreData cleanup)
  - PID file management

Requires: psutil (pip install psutil)  — optional, falls back to PowerShell
"""

import json
import logging
import os
import socket
import subprocess
import time
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context
from tools.base import send_unreal_command, make_error

logger = logging.getLogger("UnrealMCP")

MCP_HOST = "127.0.0.1"
MCP_PORT = 55557
_EDITOR_PROCESS_NAMES = {"UE5Editor.exe", "UnrealEditor.exe", "UE4Editor.exe"}

# PID file stored next to this module
_PID_FILE = os.path.join(os.path.dirname(__file__), "..", ".mcp_editor.pid")


# ---------------------------------------------------------------------------
# Private helpers
# ---------------------------------------------------------------------------

def _read_pid_file() -> Optional[int]:
    """Return the PID recorded in the PID file, or None."""
    try:
        with open(_PID_FILE) as f:
            return int(f.read().strip())
    except Exception:
        return None


def _write_pid_file(pid: int) -> None:
    """Write the editor PID to the PID file."""
    try:
        with open(_PID_FILE, "w") as f:
            f.write(str(pid))
    except Exception as e:
        logger.warning(f"Failed to write PID file: {e}")


def _delete_pid_file() -> None:
    try:
        os.remove(_PID_FILE)
    except Exception:
        pass


def _detect_engine_from_uproject(uproject_path: str) -> str:
    """Offline engine root detection from a .uproject file.

    Strategy:
      1. Read EngineAssociation from .uproject JSON.
      2. If it looks like a UUID → look up Windows registry (Launcher install).
      3. If empty string → source build; walk up directories for Engine/.
      4. If version string ("5.6") → scan common install locations.

    Returns the engine root path, or "" on failure.
    """
    if not uproject_path or not os.path.isfile(uproject_path):
        return ""

    try:
        with open(uproject_path, encoding="utf-8") as f:
            uproject = json.load(f)
    except Exception:
        return ""

    association = uproject.get("EngineAssociation", "")

    # --- Case 1: UUID (Launcher-installed, e.g. "{D4B1...}") ---
    if association.startswith("{") and association.endswith("}"):
        try:
            import winreg
            key_path = rf"SOFTWARE\EpicGames\Unreal Engine\{association}"
            for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
                try:
                    with winreg.OpenKey(hive, key_path) as k:
                        path, _ = winreg.QueryValueEx(k, "InstalledDirectory")
                        if path and os.path.isdir(path):
                            return path
                except FileNotFoundError:
                    continue
        except ImportError:
            pass

    # --- Case 2: Empty = source build, engine is an ancestor ---
    if association == "":
        candidate = os.path.dirname(uproject_path)
        for _ in range(6):
            candidate = os.path.dirname(candidate)
            engine_bin = os.path.join(candidate, "Engine", "Binaries", "Win64")
            if os.path.isdir(engine_bin):
                return candidate
        return ""

    # --- Case 3: Version string ("5.6", "5.3", etc.) ---
    version = association.strip()
    try:
        import winreg
        key_path = rf"SOFTWARE\EpicGames\Unreal Engine\{version}"
        for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
            try:
                with winreg.OpenKey(hive, key_path) as k:
                    path, _ = winreg.QueryValueEx(k, "InstalledDirectory")
                    if path and os.path.isdir(path):
                        return path
            except FileNotFoundError:
                continue
    except ImportError:
        pass

    # Fallback: scan common install roots
    for root in [r"C:\Program Files\Epic Games", r"D:\Program Files\Epic Games",
                 r"D:\Epic Games", r"F:\Unreal Engine"]:
        for sub in [f"UE_{version}", f"UE{version}", version]:
            candidate = os.path.join(root, sub)
            if os.path.isdir(os.path.join(candidate, "Engine")):
                return candidate

    return ""


def _find_editor_exe(engine_root: str) -> str:
    """Return path to UnrealEditor.exe inside engine_root, or ""."""
    for name in ("UnrealEditor.exe", "UE5Editor.exe", "UE4Editor.exe"):
        candidate = os.path.join(engine_root, "Engine", "Binaries", "Win64", name)
        if os.path.isfile(candidate):
            return candidate
    return ""


def _kill_editor_powershell() -> Dict[str, Any]:
    """Force-kill editor processes using PowerShell (reliable on Windows bash)."""
    names = ["UnrealEditor", "UE5Editor", "UE4Editor", "LiveCodingConsole", "CrashReportClientEditor"]
    stop_cmds = "; ".join(
        f"Stop-Process -Name {n} -Force -ErrorAction SilentlyContinue" for n in names
    )
    try:
        subprocess.run(
            ["powershell", "-Command", stop_cmds],
            capture_output=True, timeout=15
        )
        return {"killed": True}
    except Exception as e:
        return {"killed": False, "error": str(e)}


def _cleanup_restore_data(project_file: str) -> Dict[str, Any]:
    """Delete PackageRestoreData.json (prevents 'Recover Packages?' dialog)."""
    if not project_file:
        return {"skipped": True, "reason": "no project_file"}
    project_dir = os.path.dirname(project_file)
    restore_json = os.path.join(project_dir, "Saved", "Autosaves", "PackageRestoreData.json")
    if os.path.isfile(restore_json):
        try:
            os.remove(restore_json)
            return {"removed": restore_json}
        except Exception as e:
            return {"error": str(e)}
    return {"skipped": True, "reason": "file not found"}


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
        project_file: str = "",
        editor_exe: str = "",
        extra_args: str = "",
    ) -> Dict[str, Any]:
        """Launch the Unreal Editor as a background process.

        Engine path is auto-detected from the .uproject EngineAssociation field
        (Windows registry for Launcher installs, parent-dir walk for source builds).
        Falls back to asking the running editor via get_engine_path if available.

        Use wait_for_editor_ready to poll until MCP is accepting commands.

        Args:
            project_file: Absolute path to the .uproject file (required for auto-detect).
            editor_exe:   Absolute path to editor executable (overrides auto-detect).
            extra_args:   Extra command-line arguments (space-separated string).
        """
        detection_log = []

        # --- 1. Offline detection from .uproject ---
        if project_file and not editor_exe:
            engine_root = _detect_engine_from_uproject(project_file)
            if engine_root:
                detection_log.append(f"engine_root from uproject: {engine_root}")
                editor_exe = _find_editor_exe(engine_root)

        # --- 2. Fallback: ask running editor via MCP ---
        if not editor_exe:
            paths = send_unreal_command("get_engine_path", {})
            if paths.get("status") != "error":
                if not project_file:
                    project_file = paths.get("project_file", "")
                engine_dir = paths.get("engine_dir", "")
                if engine_dir:
                    detection_log.append(f"engine_dir from MCP: {engine_dir}")
                    editor_exe = _find_editor_exe(engine_dir)

        if not editor_exe or not os.path.isfile(editor_exe):
            return make_error(
                "Editor executable not found. Provide 'project_file' (for offline detection) "
                "or 'editor_exe' (explicit path)."
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
            _write_pid_file(proc.pid)
            return {
                "success": True,
                "pid": proc.pid,
                "command": " ".join(cmd),
                "detection_log": detection_log,
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
        project_file: str = "",
        editor_exe: str = "",
        wait_ready: bool = True,
        timeout_seconds: int = 180,
    ) -> Dict[str, Any]:
        """Kill the running editor and relaunch it cleanly.

        Full sequence:
          1. Force-kill all editor processes via PowerShell
          2. Delete PackageRestoreData.json (prevents 'Recover Packages?' dialog)
          3. Auto-detect engine path from .uproject (offline) or PID file fallback
          4. Relaunch editor
          5. Optionally wait until MCP is ready

        Args:
            project_file: Absolute path to .uproject (used for engine detection + cleanup).
                          Falls back to previously recorded PID-file project if omitted.
            editor_exe:   Explicit path to editor exe (overrides auto-detect).
            wait_ready:   Block until editor fully ready (default True).
            timeout_seconds: Max wait time in seconds (default 180).
        """
        result: Dict[str, Any] = {"success": False}

        # --- 1. Resolve project_file from PID file if not provided ---
        if not project_file:
            # Try to ask running editor first
            paths = send_unreal_command("get_engine_path", {})
            if paths.get("status") != "error":
                project_file = paths.get("project_file", "")

        # --- 2. Force-kill via PowerShell ---
        kill_result = _kill_editor_powershell()
        result["kill"] = kill_result
        _delete_pid_file()
        time.sleep(3)  # let port release

        # --- 3. Cleanup restore data ---
        result["cleanup"] = _cleanup_restore_data(project_file)

        # --- 4. Resolve editor_exe ---
        if not editor_exe and project_file:
            engine_root = _detect_engine_from_uproject(project_file)
            if engine_root:
                editor_exe = _find_editor_exe(engine_root)

        if not editor_exe or not os.path.isfile(editor_exe):
            return make_error(
                "Cannot restart: editor executable not found. "
                "Provide 'project_file' or 'editor_exe'."
            )

        # --- 5. Relaunch ---
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
            _write_pid_file(new_proc.pid)
            result["new_pid"] = new_proc.pid
        except Exception as e:
            return make_error(f"Failed to relaunch editor: {e}")

        result["success"] = True

        # --- 6. Wait for ready ---
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
            if not ready:
                result["message"] = f"Editor not ready after {timeout_seconds}s."

        return result

    @mcp.tool()
    def get_pid_file_info(ctx: Context) -> Dict[str, Any]:
        """Return the PID recorded in the editor PID file (if any).

        Useful to check if a previously launched editor is still tracked.
        """
        pid = _read_pid_file()
        if pid is None:
            return {"success": True, "pid": None, "pid_file_exists": False}

        running = False
        try:
            import psutil
            running = psutil.pid_exists(pid)
        except ImportError:
            pass  # can't verify without psutil

        return {
            "success": True,
            "pid": pid,
            "pid_file_exists": True,
            "pid_file_path": os.path.abspath(_PID_FILE),
            "process_running": running,
        }

    logger.info("System tools registered successfully")
