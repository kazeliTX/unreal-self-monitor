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
import subprocess
import sys
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context
from tools.base import send_unreal_command, make_error

logger = logging.getLogger("UnrealMCP")


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

    logger.info("Compile tools registered successfully")
