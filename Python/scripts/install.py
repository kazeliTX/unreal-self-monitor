"""
install.py — UnrealMCP one-command installer
Supports UE4.22+ / UE5.x (source-built and Launcher installs)

Usage:
    python install.py <target.uproject> [options]

Options:
    --mcp-client    claude | cursor | windsurf | none  (default: claude)
    --engine-root   explicit engine root path (skips auto-detection)
    --no-commands   skip copying .claude/commands/
    --dry-run       preview actions without writing anything
    --force         overwrite existing plugin without prompting
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import sys
from datetime import datetime
from pathlib import Path


# ─── Paths ───────────────────────────────────────────────────────────────────

REPO_ROOT = Path(__file__).resolve().parent.parent.parent  # unreal-mcp-main/
PLUGIN_SRC = REPO_ROOT / "MCPGameProject" / "Plugins" / "UnrealMCP"
COMMANDS_SRC = REPO_ROOT / ".claude" / "commands"
PYTHON_DIR = REPO_ROOT / "Python"


# ─── Compatibility matrix ─────────────────────────────────────────────────────

COMPAT_MATRIX = [
    # (feature, min_major, min_minor, warning_if_below)
    ("UEditorSubsystem",        4, 22, None),
    ("UToolMenus",              4, 22, None),
    ("UDeveloperSettings",      4, 22, None),
    ("LiveCoding module",       4, 22, None),
    ("BlueprintEditorLibrary",  5,  0, "UE4: excluded from Build.cs (conditional)"),
    ("Enhanced Input",          5,  0, "UE4: ProjectCommands Enhanced Input disabled"),
    ("FAppStyle",               5,  0, "UE4: uses FEditorStyle via MCP_STYLE_NAME macro"),
]


# ─── Engine path discovery ────────────────────────────────────────────────────

def find_engine_root(uproject_path: Path, engine_assoc: str,
                     hint: str | None = None) -> tuple[Path | None, str]:
    """Return (engine_root, source) where source ∈ sibling|registry|env|hint|manual."""

    # 0. Explicit hint from CLI
    if hint:
        p = Path(hint)
        if (p / "Engine" / "Build" / "Build.version").exists():
            return p, "hint"
        # Maybe they gave the Engine subfolder itself
        if (p / "Build" / "Build.version").exists():
            return p.parent, "hint"

    # 1. Source-built: EngineAssociation is empty, engine is at ../Engine
    if engine_assoc == "":
        sibling = uproject_path.parent.parent / "Engine"
        if (sibling / "Build" / "Build.version").exists():
            return sibling.parent, "sibling"

    # 2. Windows registry (Launcher installs)
    if platform.system() == "Windows" and engine_assoc:
        try:
            import winreg
            # HKLM path
            for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
                try:
                    key = winreg.OpenKey(hive,
                        rf"SOFTWARE\EpicGames\Unreal Engine\{engine_assoc}")
                    path_str, _ = winreg.QueryValueEx(key, "InstalledDirectory")
                    p = Path(path_str)
                    if (p / "Engine" / "Build" / "Build.version").exists():
                        return p, "registry"
                    if (p / "Build" / "Build.version").exists():
                        return p.parent, "registry"
                except OSError:
                    pass
            # HKCU custom builds map
            try:
                key = winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                    r"SOFTWARE\Epic Games\Unreal Engine\Builds")
                i = 0
                while True:
                    try:
                        name, value, _ = winreg.EnumValue(key, i)
                        p = Path(value)
                        if (p / "Engine" / "Build" / "Build.version").exists():
                            return p, "registry-custom"
                        i += 1
                    except OSError:
                        break
            except OSError:
                pass
        except ImportError:
            pass  # winreg not available (non-Windows or old Python)

    # 3. Environment variable fallback
    if ue_root := os.environ.get("UE_ROOT"):
        p = Path(ue_root)
        if (p / "Engine" / "Build" / "Build.version").exists():
            return p, "env"
        if (p / "Build" / "Build.version").exists():
            return p.parent, "env"

    return None, "manual"


def read_build_version(engine_root: Path) -> dict:
    bv_path = engine_root / "Engine" / "Build" / "Build.version"
    if not bv_path.exists():
        return {}
    with bv_path.open() as f:
        return json.load(f)


# ─── MCP client config generation ────────────────────────────────────────────

MCP_SERVER_ENTRY = {
    "command": "uv",
    "args": [
        "--directory", str(PYTHON_DIR),
        "run", "unreal_mcp_server.py"
    ]
}

MCP_CLIENT_PATHS = {
    "claude": [
        Path.home() / "AppData" / "Roaming" / "Claude" / "claude_desktop_config.json",
        Path.home() / ".config" / "Claude" / "claude_desktop_config.json",
    ],
    "cursor": [
        Path.home() / ".cursor" / "mcp.json",
    ],
    "windsurf": [
        Path.home() / ".codeium" / "windsurf" / "mcp_config.json",
    ],
}


def generate_mcp_config(client: str, target_dir: Path, dry_run: bool) -> Path | None:
    """Write or merge mcp.json for the chosen client. Returns written path."""
    if client == "none":
        return None

    # Also write a local mcp.json in the target project root for reference
    local_mcp = target_dir / "mcp.json"
    cfg = {"mcpServers": {"unrealMCP": MCP_SERVER_ENTRY}}

    candidates = MCP_CLIENT_PATHS.get(client, [])

    written: list[Path] = []
    for cfg_path in candidates:
        if cfg_path.exists():
            with cfg_path.open() as f:
                existing = json.load(f)
            existing.setdefault("mcpServers", {})["unrealMCP"] = MCP_SERVER_ENTRY
            if not dry_run:
                with cfg_path.open("w") as f:
                    json.dump(existing, f, indent=2)
            written.append(cfg_path)
            break
    else:
        # Write to first candidate path (create dirs)
        if candidates:
            target_cfg = candidates[0]
            if not dry_run:
                target_cfg.parent.mkdir(parents=True, exist_ok=True)
                with target_cfg.open("w") as f:
                    json.dump(cfg, f, indent=2)
            written.append(candidates[0])

    # Always write local mcp.json
    if not dry_run:
        with local_mcp.open("w") as f:
            json.dump(cfg, f, indent=2)

    return written[0] if written else local_mcp


# ─── Core installation steps ──────────────────────────────────────────────────

def copy_plugin(target_project_dir: Path, dry_run: bool, force: bool) -> bool:
    """Copy plugin to target project. Returns True on success."""
    target_plugin = target_project_dir / "Plugins" / "UnrealMCP"

    if target_plugin.exists():
        if not force:
            ans = input(f"\n  Plugin already exists at:\n    {target_plugin}\n  Overwrite? [y/N] ").strip().lower()
            if ans != "y":
                print("  Skipped plugin copy.")
                return False
        print(f"  Removing existing plugin...")
        if not dry_run:
            shutil.rmtree(target_plugin)

    print(f"  Copying plugin -> {target_plugin}")
    if not dry_run:
        target_plugin.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(PLUGIN_SRC, target_plugin)
    return True


def patch_uproject(uproject_path: Path, dry_run: bool) -> bool:
    """Inject UnrealMCP plugin entry into .uproject. Returns True if changed."""
    with uproject_path.open(encoding="utf-8") as f:
        data = json.load(f)

    plugins: list[dict] = data.setdefault("Plugins", [])
    if any(p.get("Name") == "UnrealMCP" for p in plugins):
        print("  .uproject already contains UnrealMCP — skipping patch.")
        return False

    plugins.append({"Name": "UnrealMCP", "Enabled": True})
    print(f"  Patching {uproject_path.name} - injecting UnrealMCP plugin entry")
    if not dry_run:
        with uproject_path.open("w", encoding="utf-8") as f:
            json.dump(data, f, indent="\t")
    return True


def copy_commands(target_project_dir: Path, dry_run: bool):
    """Copy .claude/commands/ to target project."""
    if not COMMANDS_SRC.exists():
        print("  No .claude/commands/ found in repo — skipping.")
        return
    dest = target_project_dir / ".claude" / "commands"
    print(f"  Copying .claude/commands/ → {dest}")
    if not dry_run:
        dest.parent.mkdir(parents=True, exist_ok=True)
        if dest.exists():
            shutil.rmtree(dest)
        shutil.copytree(COMMANDS_SRC, dest)


def run_generate_project_files(engine_root: Path, uproject_path: Path, dry_run: bool):
    """Invoke GenerateProjectFiles for source-built engines."""
    gpf = engine_root / "GenerateProjectFiles.bat"
    if not gpf.exists():
        gpf = engine_root / "Engine" / "Build" / "BatchFiles" / "GenerateProjectFiles.bat"
    if not gpf.exists():
        print("  WARNING: GenerateProjectFiles.bat not found — skipping VS project regeneration.")
        return

    cmd = f'"{gpf}" "{uproject_path}"'
    print(f"  Running: {cmd}")
    if not dry_run:
        os.system(cmd)


# ─── Compatibility report ─────────────────────────────────────────────────────

def print_compat_report(uproject_path: Path, engine_assoc: str,
                        engine_root: Path | None, engine_src: str,
                        bv: dict, target_plugin_exists: bool):
    major = bv.get("MajorVersion", "?")
    minor = bv.get("MinorVersion", "?")
    patch = bv.get("PatchVersion", "?")
    branch = bv.get("BranchName", "")

    build_type = "源码编译" if engine_assoc == "" else f"Launcher ({engine_assoc})"
    engine_display = str(engine_root) if engine_root else "未找到 — 需手动指定 --engine-root"

    print()
    print("=" * 55)
    print("  UnrealMCP Compatibility Report")
    print("=" * 55)
    print(f"  Project     : {uproject_path.parent}")
    print(f"  .uproject   : {uproject_path.name}")
    print(f"  Engine type : {build_type}")
    print(f"  Engine ver  : UE {major}.{minor}.{patch}  ({branch})")
    print(f"  Engine root : {engine_display}  [source: {engine_src}]")
    print(f"  Plugin      : {'already installed (will prompt overwrite)' if target_plugin_exists else 'not installed (fresh install)'}")
    print()
    print("  " + "-" * 51)
    print("  Compatibility checks")
    print("  " + "-" * 51)

    if isinstance(major, int) and isinstance(minor, int):
        for feat, req_major, req_minor, warning in COMPAT_MATRIX:
            supported = (major, minor) >= (req_major, req_minor)
            if supported:
                print(f"  [OK]   {feat}")
            else:
                note = f"  [WARN] {feat}"
                if warning:
                    note += f" - {warning}"
                print(note)
    else:
        print("  [WARN] Cannot determine engine version - skipping compat check")

    print("=" * 55)
    print()


# ─── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Install UnrealMCP plugin into a UE4/UE5 project")
    parser.add_argument("uproject", help="Path to target .uproject file")
    parser.add_argument("--mcp-client",
                        choices=["claude", "cursor", "windsurf", "none"],
                        default="claude",
                        help="MCP client to configure (default: claude)")
    parser.add_argument("--engine-root",
                        help="Explicit engine root path (overrides auto-detection)")
    parser.add_argument("--no-commands", action="store_true",
                        help="Skip copying .claude/commands/")
    parser.add_argument("--dry-run", action="store_true",
                        help="Preview actions without writing anything")
    parser.add_argument("--force", action="store_true",
                        help="Overwrite existing plugin without prompting")
    args = parser.parse_args()

    if args.dry_run:
        print("\n  [DRY RUN] No files will be written.\n")

    # ── Step 1: Read .uproject ────────────────────────────────────────────────
    uproject_path = Path(args.uproject).resolve()
    if not uproject_path.exists():
        sys.exit(f"ERROR: .uproject not found: {uproject_path}")
    if uproject_path.suffix.lower() != ".uproject":
        sys.exit(f"ERROR: Expected a .uproject file, got: {uproject_path}")

    with uproject_path.open(encoding="utf-8") as f:
        uproject_data = json.load(f)

    engine_assoc: str = uproject_data.get("EngineAssociation", "")
    existing_plugins = [p.get("Name", "") for p in uproject_data.get("Plugins", [])]
    target_plugin_exists = (uproject_path.parent / "Plugins" / "UnrealMCP").exists()

    # ── Step 2: Locate engine ─────────────────────────────────────────────────
    engine_root, engine_src = find_engine_root(
        uproject_path, engine_assoc, args.engine_root)

    bv: dict = {}
    if engine_root:
        bv = read_build_version(engine_root)

    if not engine_root:
        print("\nWARNING: Could not auto-detect engine root.")
        manual = input("  Enter engine root path (or press Enter to skip): ").strip()
        if manual:
            engine_root = Path(manual)
            engine_src = "manual"
            bv = read_build_version(engine_root)

    # ── Step 3: Compatibility report ──────────────────────────────────────────
    print_compat_report(uproject_path, engine_assoc, engine_root,
                        engine_src, bv, target_plugin_exists)

    major = bv.get("MajorVersion")
    minor = bv.get("MinorVersion")
    if isinstance(major, int) and major < 4 or (major == 4 and isinstance(minor, int) and minor < 22):
        sys.exit("ERROR: UnrealMCP requires UE 4.22 or later.")

    # Confirm before proceeding
    if not args.dry_run:
        ans = input("  Proceed with installation? [Y/n] ").strip().lower()
        if ans == "n":
            sys.exit("Installation cancelled.")

    print()
    print("  Installation steps:")
    print()

    # ── Step 4: Copy plugin ───────────────────────────────────────────────────
    print("  [1/5] Copy plugin")
    copy_plugin(uproject_path.parent, args.dry_run, args.force)

    # ── Step 5: Patch .uproject ───────────────────────────────────────────────
    print("  [2/5] Patch .uproject")
    patch_uproject(uproject_path, args.dry_run)

    # ── Step 6: Generate mcp.json ─────────────────────────────────────────────
    print(f"  [3/5] Generate MCP config  (client: {args.mcp_client})")
    cfg_path = generate_mcp_config(args.mcp_client, uproject_path.parent, args.dry_run)
    if cfg_path:
        print(f"        Written to: {cfg_path}")

    # ── Step 7: Copy .claude/commands/ ───────────────────────────────────────
    print("  [4/5] Copy .claude/commands/")
    if args.no_commands:
        print("        Skipped (--no-commands).")
    else:
        copy_commands(uproject_path.parent, args.dry_run)

    # ── Step 8: Generate VS project files (source builds) ────────────────────
    print("  [5/5] Regenerate VS project files")
    if engine_assoc == "" and engine_root:
        run_generate_project_files(engine_root, uproject_path, args.dry_run)
    else:
        print("        Skipped (Launcher install - regenerate via .uproject right-click).")

    # ── Final report ──────────────────────────────────────────────────────────
    print()
    print("=" * 55)
    print("  Installation complete!")
    print("=" * 55)
    print()
    print("  Next steps:")
    print("  1. Open the project in Unreal Editor")
    print("  2. Compile the UnrealMCP plugin (Build > Compile)")
    print("  3. Start MCP server:  uv --directory Python run unreal_mcp_server.py")
    print("  4. In Editor: Tools > MCP Server Toggle")
    print()

    major = bv.get("MajorVersion")
    if isinstance(major, int) and major < 5:
        print("  UE4 notes:")
        print("  - Enhanced Input commands not available (get_capabilities shows actual set)")
        print("  - BlueprintEditorLibrary excluded from build")
        print()

    if args.dry_run:
        print("  [DRY RUN] No files were written.")
        print()


if __name__ == "__main__":
    main()
