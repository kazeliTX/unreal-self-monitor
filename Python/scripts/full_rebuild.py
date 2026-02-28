#!/usr/bin/env python3
"""
full_rebuild.py — Standalone full-rebuild pipeline for UnrealMCP.

Runs outside the MCP server (no running editor required at start).
Steps:
  1. Pre-save current level + assets (if editor is online)
  2. Kill all Unreal Editor processes
  3. Delete PackageRestoreData.json / Temp autosaves
  4. Run UBT (dotnet UnrealBuildTool.dll)
  5. Relaunch the editor (unless --no-launch)
  6. Poll until the editor is ready (unless --no-wait)

Usage:
    uv run python scripts/full_rebuild.py [OPTIONS]

Options:
    --project FILE      Path to .uproject file (auto-detected if omitted)
    --ubt FILE          Path to UnrealBuildTool.dll (auto-detected from engine_dir)
    --target NAME       UBT target name (defaults to <ProjectName>Editor)
    --config NAME       Build configuration: Development (default), DebugGame, Shipping
    --no-launch         Do not relaunch the editor after building
    --no-wait           Do not wait for the editor to become ready after relaunch
    --timeout SECS      Seconds to wait for editor ready (default: 180)
    --port PORT         MCP TCP port to probe (default: 55557)
"""

import argparse
import glob
import json
import os
import shutil
import socket
import subprocess
import sys
import time

MCP_PORT = 55557


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _is_port_open(host: str, port: int, timeout: float = 1.0) -> bool:
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def _send_command(command: str, params: dict = None, port: int = MCP_PORT) -> dict:
    """Send a single MCP command over TCP and return parsed JSON response."""
    payload = json.dumps({"command": command, "params": params or {}}).encode()
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=5) as s:
            s.sendall(payload)
            chunks = []
            while True:
                chunk = s.recv(65536)
                if not chunk:
                    break
                chunks.append(chunk)
            return json.loads(b"".join(chunks))
    except Exception as e:
        return {"error": str(e)}


def _kill_editor() -> list:
    """Kill Unreal Editor processes. Returns list of killed names."""
    targets = ["UnrealEditor.exe", "UE5Editor.exe", "UE4Editor.exe", "CrashReportClientEditor.exe"]
    killed = []
    try:
        import psutil
        for proc in psutil.process_iter(["pid", "name"]):
            if proc.info["name"] in targets:
                try:
                    proc.kill()
                    killed.append(proc.info["name"])
                except Exception:
                    pass
    except ImportError:
        for name in targets:
            r = subprocess.run(["taskkill", "/F", "/IM", name], capture_output=True)
            if r.returncode == 0:
                killed.append(name)
    return killed


def _cleanup_autosaves(project_dir: str) -> list:
    """Remove PackageRestoreData.json and Temp autosaves. Returns removed paths."""
    removed = []
    if not project_dir:
        return removed

    restore_json = os.path.join(project_dir, "Saved", "Autosaves", "PackageRestoreData.json")
    if os.path.isfile(restore_json):
        os.remove(restore_json)
        removed.append(restore_json)

    temp_dir = os.path.join(project_dir, "Saved", "Autosaves", "Temp")
    if os.path.isdir(temp_dir):
        shutil.rmtree(temp_dir)
        removed.append(temp_dir)

    return removed


def _find_uproject(start_dir: str) -> str:
    """Search upward from start_dir for a .uproject file."""
    d = start_dir
    for _ in range(5):
        found = glob.glob(os.path.join(d, "*.uproject"))
        if found:
            return found[0]
        parent = os.path.dirname(d)
        if parent == d:
            break
        d = parent
    return ""


def _find_ubt(engine_dir: str) -> str:
    candidate = os.path.join(engine_dir, "Binaries", "DotNET", "UnrealBuildTool", "UnrealBuildTool.dll")
    return candidate if os.path.isfile(candidate) else ""


def _find_editor_exe(engine_dir: str) -> str:
    for name in ("UnrealEditor.exe", "UE5Editor.exe", "UE4Editor.exe"):
        candidate = os.path.join(engine_dir, "Binaries", "Win64", name)
        if os.path.isfile(candidate):
            return candidate
    return ""


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="UnrealMCP full rebuild pipeline")
    parser.add_argument("--project", default="", help="Path to .uproject file")
    parser.add_argument("--ubt", default="", help="Path to UnrealBuildTool.dll")
    parser.add_argument("--target", default="", help="UBT target name")
    parser.add_argument("--config", default="Development", help="Build configuration")
    parser.add_argument("--no-launch", action="store_true", help="Do not relaunch editor")
    parser.add_argument("--no-wait", action="store_true", help="Do not wait for editor ready")
    parser.add_argument("--timeout", type=int, default=180, help="Seconds to wait for ready")
    parser.add_argument("--port", type=int, default=MCP_PORT, help="MCP TCP port")
    args = parser.parse_args()

    port = args.port
    steps = []

    print("=== UnrealMCP Full Rebuild ===")

    # Step 1: Pre-save if editor is online
    if _is_port_open("127.0.0.1", port):
        print("[1/6] Editor online — pre-saving...")
        try:
            _send_command("save_current_level", port=port)
            _send_command("save_all_assets", port=port)
            paths_resp = _send_command("get_engine_path", port=port)
        except Exception as e:
            print(f"      Warning: pre-save failed: {e}")
            paths_resp = {}
        steps.append("pre_save: ok")
    else:
        print("[1/6] Editor offline — skipping pre-save")
        paths_resp = {}
        steps.append("pre_save: skipped")

    # Resolve paths
    engine_dir = paths_resp.get("engine_dir", "") if isinstance(paths_resp, dict) else ""
    project_file = args.project or (paths_resp.get("project_file", "") if isinstance(paths_resp, dict) else "")
    if not project_file:
        project_file = _find_uproject(os.getcwd())
    ubt_dll = args.ubt or _find_ubt(engine_dir)
    editor_exe = _find_editor_exe(engine_dir)
    project_dir = os.path.dirname(project_file) if project_file else ""

    # Step 2: Kill editor
    print("[2/6] Killing editor processes...")
    killed = _kill_editor()
    print(f"      Killed: {killed or 'none'}")
    steps.append(f"kill: {killed}")

    # Step 3: Wait for port close + cleanup
    print("[3/6] Waiting for editor to exit + cleaning autosaves...")
    deadline = time.time() + 30
    while time.time() < deadline and _is_port_open("127.0.0.1", port, 0.5):
        time.sleep(1)
    removed = _cleanup_autosaves(project_dir)
    print(f"      Removed: {removed or 'none'}")
    steps.append(f"cleanup: {removed}")

    # Step 4: Run UBT
    if not ubt_dll or not os.path.isfile(ubt_dll):
        print(f"[4/6] ERROR: UnrealBuildTool.dll not found: '{ubt_dll}'")
        print("      Provide --ubt <path> or ensure engine_dir is available from the editor.")
        sys.exit(1)

    target = args.target
    if not target and project_file:
        target = os.path.splitext(os.path.basename(project_file))[0] + "Editor"
    if not target:
        target = "UnrealEditor"

    ubt_cmd = ["dotnet", ubt_dll, target, "Win64", args.config]
    if project_file:
        ubt_cmd.append(f"-Project={project_file}")
    ubt_cmd += ["-WaitMutex", "-NoHotReloadFromIDE"]

    print(f"[4/6] Running UBT: {' '.join(ubt_cmd)}")
    proc = subprocess.run(ubt_cmd, capture_output=False)  # stream output live
    if proc.returncode != 0:
        print(f"      ERROR: UBT failed with return code {proc.returncode}")
        sys.exit(proc.returncode)
    steps.append(f"ubt: rc={proc.returncode}")
    print("      UBT build succeeded.")

    # Step 5: Relaunch editor
    if args.no_launch or not editor_exe:
        print("[5/6] Skipping editor relaunch.")
        steps.append("relaunch: skipped")
        print(f"\nDone. Steps: {steps}")
        return

    launch_cmd = [editor_exe]
    if project_file:
        launch_cmd.append(project_file)
    print(f"[5/6] Relaunching editor: {editor_exe}")
    subprocess.Popen(launch_cmd)
    steps.append("relaunch: ok")

    # Step 6: Wait for ready
    if args.no_wait:
        print("[6/6] Skipping wait-for-ready.")
        steps.append("wait: skipped")
    else:
        print(f"[6/6] Waiting up to {args.timeout}s for editor to become ready...")
        deadline = time.time() + args.timeout
        ready = False
        while time.time() < deadline:
            time.sleep(3)
            if not _is_port_open("127.0.0.1", port):
                continue
            r = _send_command("ping", port=port)
            if r.get("status") == "ok":
                lvl = _send_command("get_current_level_name", port=port)
                if lvl.get("level_name"):
                    ready = True
                    break
        if ready:
            print("      Editor is ready!")
        else:
            print("      Timeout — editor may still be loading.")
        steps.append(f"wait: {'ready' if ready else 'timeout'}")

    print(f"\nDone. Steps: {steps}")


if __name__ == "__main__":
    main()
