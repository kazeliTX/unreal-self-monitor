"""
Compile Tools for Unreal MCP.

Provides four tiers of compile/reload capability:
  Tier 1 – Blueprint compile:   instant, via BlueprintCommands C++ (compile_blueprint)
  Tier 2 – Python module reload: instant, no MCP restart needed
  Tier 3 – LiveCoding hot reload: 5-60s, for function body changes
  Tier 4 – UBT full build:       minutes, for header/class changes (needs editor restart)
"""

import importlib
import importlib.util
import logging
import os
import socket
import subprocess
import sys
import time
from typing import Dict, Any
from mcp.server.fastmcp import FastMCP, Context
from tools.base import send_unreal_command, make_error

logger = logging.getLogger("UnrealMCP")


def _is_port_open(host: str, port: int, timeout: float = 1.0) -> bool:
    """Return True if the TCP port is accepting connections."""
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def _kill_editor_processes() -> Dict[str, Any]:
    """Kill all running Unreal Editor processes. Returns a result dict."""
    targets = ["UnrealEditor.exe", "UE5Editor.exe", "UE4Editor.exe", "CrashReportClientEditor.exe"]
    killed = []
    errors = []

    try:
        import psutil
        for proc in psutil.process_iter(["pid", "name"]):
            if proc.info["name"] in targets:
                try:
                    proc.kill()
                    killed.append(proc.info["name"])
                except Exception as e:
                    errors.append(str(e))
    except ImportError:
        # Fallback: use taskkill
        for name in targets:
            try:
                result = subprocess.run(
                    ["taskkill", "/F", "/IM", name],
                    capture_output=True, text=True
                )
                if result.returncode == 0:
                    killed.append(name)
            except Exception as e:
                errors.append(str(e))

    return {"killed": killed, "errors": errors}


def _cleanup_autosaves(project_file: str) -> Dict[str, Any]:
    """Delete PackageRestoreData.json and Temp autosaves to prevent recovery dialogs."""
    import shutil
    project_dir = os.path.dirname(project_file) if project_file else ""
    removed = []
    errors = []

    if project_dir:
        restore_json = os.path.join(project_dir, "Saved", "Autosaves", "PackageRestoreData.json")
        if os.path.isfile(restore_json):
            try:
                os.remove(restore_json)
                removed.append(restore_json)
            except Exception as e:
                errors.append(str(e))

        temp_autosaves = os.path.join(project_dir, "Saved", "Autosaves", "Temp")
        if os.path.isdir(temp_autosaves):
            try:
                shutil.rmtree(temp_autosaves)
                removed.append(temp_autosaves)
            except Exception as e:
                errors.append(str(e))

    return {"removed": removed, "errors": errors}


def _resolve_build_paths(paths_result: Dict[str, Any], project_file: str = "", ubt_dll: str = "") -> Dict[str, Any]:
    """Extract engine/project paths from get_engine_path response, with fallbacks."""
    engine_dir = paths_result.get("engine_dir", "")
    resolved_project = project_file or paths_result.get("project_file", "")
    resolved_ubt = ubt_dll

    if not resolved_ubt and engine_dir:
        candidate = os.path.join(
            engine_dir, "Binaries", "DotNET", "UnrealBuildTool", "UnrealBuildTool.dll"
        )
        if os.path.isfile(candidate):
            resolved_ubt = candidate

    editor_exe = ""
    if engine_dir:
        for name in ("UnrealEditor.exe", "UE5Editor.exe", "UE4Editor.exe"):
            candidate = os.path.join(engine_dir, "Binaries", "Win64", name)
            if os.path.isfile(candidate):
                editor_exe = candidate
                break

    return {
        "engine_dir": engine_dir,
        "project_file": resolved_project,
        "ubt_dll": resolved_ubt,
        "editor_exe": editor_exe,
    }


def register_compile_tools(mcp: FastMCP):
    """Register compile / source-file tools with the MCP server."""

    # ------------------------------------------------------------------
    # Source file access (via C++ DiagnosticsCommands)
    # ------------------------------------------------------------------

    @mcp.tool()
    def read_cpp_file(
        ctx: Context,
        path: str,
    ) -> Dict[str, Any]:
        """Read a C++ source file from the UE project.

        Args:
            path: Path to the file. Relative paths are resolved from the
                  project root directory.
        """
        return send_unreal_command("get_source_file", {"path": path})

    @mcp.tool()
    def write_cpp_file(
        ctx: Context,
        path: str,
        content: str,
    ) -> Dict[str, Any]:
        """Write content to a C++ source file (creates an automatic timestamped backup).

        Args:
            path: Path to the file. Relative paths are resolved from the project root.
            content: New file content.
        """
        return send_unreal_command("modify_source_file", {"path": path, "content": content})

    @mcp.tool()
    def read_python_tool(
        ctx: Context,
        tool_name: str,
    ) -> Dict[str, Any]:
        """Read the source of a Python tool file from the tools/ directory.

        Args:
            tool_name: Module name without .py extension (e.g. 'blueprint_tools').
        """
        tools_dir = os.path.join(os.path.dirname(__file__))
        file_path = os.path.join(tools_dir, f"{tool_name}.py")

        if not os.path.isfile(file_path):
            return make_error(f"Tool file not found: {file_path}")

        try:
            with open(file_path, "r", encoding="utf-8") as f:
                content = f.read()
            return {
                "success": True,
                "path": file_path,
                "module": tool_name,
                "content": content,
                "size": len(content),
            }
        except Exception as e:
            return make_error(f"Failed to read tool file: {e}")

    @mcp.tool()
    def write_python_tool(
        ctx: Context,
        tool_name: str,
        content: str,
    ) -> Dict[str, Any]:
        """Write new content to a Python tool file (with automatic backup).

        Args:
            tool_name: Module name without .py extension (e.g. 'blueprint_tools').
            content: New Python source code.
        """
        import shutil
        from datetime import datetime

        tools_dir = os.path.join(os.path.dirname(__file__))
        file_path = os.path.join(tools_dir, f"{tool_name}.py")

        backup_path = ""
        if os.path.isfile(file_path):
            ts = datetime.now().strftime("%Y%m%d_%H%M%S")
            backup_path = file_path + f".bak.{ts}"
            shutil.copy2(file_path, backup_path)

        try:
            with open(file_path, "w", encoding="utf-8") as f:
                f.write(content)
            return {
                "success": True,
                "path": file_path,
                "module": tool_name,
                "bytes_written": len(content),
                "backup_path": backup_path,
            }
        except Exception as e:
            return make_error(f"Failed to write tool file: {e}")

    @mcp.tool()
    def reload_python_tool(
        ctx: Context,
        tool_name: str,
    ) -> Dict[str, Any]:
        """Hot-reload a Python tool module without restarting the MCP server.

        Args:
            tool_name: Module name without .py extension (e.g. 'blueprint_tools').
        """
        full_module = f"tools.{tool_name}"
        try:
            if full_module in sys.modules:
                importlib.reload(sys.modules[full_module])
                return {
                    "success": True,
                    "module": full_module,
                    "action": "reloaded",
                    "message": f"Module {full_module} reloaded successfully.",
                }
            else:
                importlib.import_module(full_module)
                return {
                    "success": True,
                    "module": full_module,
                    "action": "imported",
                    "message": f"Module {full_module} imported (was not previously loaded).",
                }
        except Exception as e:
            return make_error(f"Failed to reload module {full_module}: {e}")

    # ------------------------------------------------------------------
    # LiveCoding / hot-reload (Tier 3, via C++ DiagnosticsCommands)
    # ------------------------------------------------------------------

    @mcp.tool()
    def trigger_hot_reload(ctx: Context) -> Dict[str, Any]:
        """Trigger a LiveCoding compile inside the running Unreal Editor.

        Suitable for C++ function-body changes that don't affect class layout.
        Returns immediately; compilation happens asynchronously in the editor.
        """
        return send_unreal_command("trigger_hot_reload", {})

    @mcp.tool()
    def get_live_coding_status(ctx: Context) -> Dict[str, Any]:
        """Check whether the LiveCoding module is available in the editor."""
        return send_unreal_command("get_live_coding_status", {})

    # ------------------------------------------------------------------
    # Engine path / UBT full build (Tier 4)
    # ------------------------------------------------------------------

    @mcp.tool()
    def get_engine_path(ctx: Context) -> Dict[str, Any]:
        """Return the UE engine directory, project directory, project file path,
        and the path to the UBT Build.bat script."""
        return send_unreal_command("get_engine_path", {})

    @mcp.tool()
    def run_ubt_build(
        ctx: Context,
        target_name: Optional[str] = None,
        configuration: str = "Development",
        platform: str = "Win64",
        extra_args: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Run Unreal Build Tool (UBT) to fully recompile the project.

        WARNING: This is a blocking call that can take several minutes.
        The editor must be closed before running a full UBT build.

        Args:
            target_name: Build target name (defaults to project name + 'Editor').
            configuration: 'Development', 'DebugGame', or 'Shipping'.
            platform: 'Win64' (default) or other platform identifier.
            extra_args: Additional UBT arguments string.
        """
        # Get paths from editor
        paths_result = send_unreal_command("get_engine_path", {})
        if paths_result.get("status") == "error":
            return paths_result

        engine_dir = paths_result.get("engine_dir", "")
        project_file = paths_result.get("project_file", "")
        ubt_script = paths_result.get("ubt_batch_script", "")

        if not ubt_script or not os.path.isfile(ubt_script):
            return make_error(f"UBT Build.bat not found: {ubt_script}")

        if not target_name:
            # Derive target from project file name
            project_base = os.path.splitext(os.path.basename(project_file))[0]
            target_name = f"{project_base}Editor"

        cmd = [ubt_script, target_name, platform, configuration]
        if project_file:
            cmd += [project_file]
        cmd += ["-WaitMutex", "-NoHotReloadFromIDE"]
        if extra_args:
            cmd += extra_args.split()

        logger.info(f"Running UBT: {' '.join(cmd)}")
        try:
            proc = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=600,  # 10 min max
            )
            return {
                "success": proc.returncode == 0,
                "return_code": proc.returncode,
                "command": " ".join(cmd),
                "stdout_tail": proc.stdout[-3000:] if proc.stdout else "",
                "stderr_tail": proc.stderr[-3000:] if proc.stderr else "",
            }
        except subprocess.TimeoutExpired:
            return make_error("UBT build timed out after 10 minutes.")
        except Exception as e:
            return make_error(f"Failed to run UBT: {e}")

    @mcp.tool()
    def kill_editor(ctx: Context) -> Dict[str, Any]:
        """Kill all running Unreal Editor processes.

        Use before a full rebuild. The editor must be closed before UBT can rebuild.
        """
        result = _kill_editor_processes()
        return {
            "success": True,
            "killed": result["killed"],
            "errors": result["errors"],
        }

    @mcp.tool()
    def full_rebuild(
        ctx: Context,
        project_file: str = "",
        ubt_dll: str = "",
        target_name: str = "",
        configuration: str = "Development",
        platform: str = "Win64",
        relaunch_editor: int = 1,
        wait_ready: int = 1,
        wait_timeout: int = 120,
    ) -> Dict[str, Any]:
        """Run the complete full-rebuild pipeline: pre-save → kill → cleanup → UBT → relaunch → wait.

        WARNING: This is a long-running blocking call (several minutes). The editor will be
        closed and reopened as part of this process.

        Args:
            project_file: Path to the .uproject file. Auto-detected from editor if omitted.
            ubt_dll: Path to UnrealBuildTool.dll. Auto-detected from engine_dir if omitted.
            target_name: UBT target (e.g. 'MyProjectEditor'). Defaults to project name + 'Editor'.
            configuration: 'Development' (default), 'DebugGame', or 'Shipping'.
            platform: Build platform, default 'Win64'.
            relaunch_editor: 1 (default) to relaunch the editor after build, 0 to skip.
            wait_ready: 1 (default) to poll until the editor is ready after relaunch, 0 to skip.
            wait_timeout: Seconds to wait for the editor to become ready (default 120).
        """
        steps = []

        # Step 1: Get paths and pre-save while editor is still running
        paths_result = {}
        editor_was_online = _is_port_open("127.0.0.1", 55557)
        if editor_was_online:
            try:
                paths_result = send_unreal_command("get_engine_path", {})
                send_unreal_command("save_current_level", {})
                send_unreal_command("save_all_assets", {})
                steps.append("pre_save: ok")
            except Exception as e:
                steps.append(f"pre_save: warning ({e})")
        else:
            steps.append("pre_save: skipped (editor offline)")

        paths = _resolve_build_paths(paths_result, project_file, ubt_dll)
        resolved_project = paths["project_file"]
        resolved_ubt = paths["ubt_dll"]
        editor_exe = paths["editor_exe"]

        # Step 2: Kill editor
        kill_result = _kill_editor_processes()
        steps.append(f"kill_editor: killed={kill_result['killed']}")

        # Step 3: Wait for port to close + cleanup autosaves
        deadline = time.time() + 30
        while time.time() < deadline and _is_port_open("127.0.0.1", 55557, 0.5):
            time.sleep(1)
        cleanup = _cleanup_autosaves(resolved_project)
        steps.append(f"cleanup: removed={cleanup['removed']}")

        # Step 4: Run UBT
        if not resolved_ubt or not os.path.isfile(resolved_ubt):
            return {
                "success": False,
                "steps": steps,
                "error": f"UnrealBuildTool.dll not found: '{resolved_ubt}'. "
                         "Provide ubt_dll or ensure get_engine_path returns engine_dir.",
            }

        if not target_name:
            project_base = os.path.splitext(os.path.basename(resolved_project))[0] if resolved_project else ""
            target_name = f"{project_base}Editor" if project_base else "UnrealEditor"

        ubt_cmd = ["dotnet", resolved_ubt, target_name, platform, configuration]
        if resolved_project:
            ubt_cmd += [f"-Project={resolved_project}"]
        ubt_cmd += ["-WaitMutex", "-NoHotReloadFromIDE"]

        logger.info(f"UBT command: {' '.join(ubt_cmd)}")
        try:
            proc = subprocess.run(ubt_cmd, capture_output=True, text=True, timeout=600)
            ubt_ok = proc.returncode == 0
            steps.append(f"ubt_build: returncode={proc.returncode}")
            if not ubt_ok:
                return {
                    "success": False,
                    "steps": steps,
                    "error": f"UBT failed (rc={proc.returncode})",
                    "stdout_tail": proc.stdout[-3000:] if proc.stdout else "",
                    "stderr_tail": proc.stderr[-3000:] if proc.stderr else "",
                }
        except subprocess.TimeoutExpired:
            return {"success": False, "steps": steps, "error": "UBT build timed out after 10 minutes."}
        except Exception as e:
            return {"success": False, "steps": steps, "error": f"UBT failed: {e}"}

        # Step 5: Relaunch editor
        if relaunch_editor and editor_exe and os.path.isfile(editor_exe):
            launch_cmd = [editor_exe]
            if resolved_project:
                launch_cmd.append(resolved_project)
            subprocess.Popen(launch_cmd)
            steps.append(f"relaunch: {editor_exe}")
        else:
            steps.append("relaunch: skipped")
            return {"success": True, "steps": steps}

        # Step 6: Poll until ready
        if wait_ready:
            deadline = time.time() + wait_timeout
            ready = False
            while time.time() < deadline:
                time.sleep(3)
                if not _is_port_open("127.0.0.1", 55557):
                    continue
                try:
                    ping = send_unreal_command("ping", {})
                    if ping.get("status") == "ok":
                        lvl = send_unreal_command("get_current_level_name", {})
                        if lvl.get("level_name"):
                            ready = True
                            break
                except Exception:
                    pass
            steps.append(f"wait_ready: {'ready' if ready else 'timeout'}")

        return {"success": True, "steps": steps}

    logger.info("Compile tools registered successfully")
