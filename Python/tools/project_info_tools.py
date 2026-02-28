"""
project_info_tools.py — MCP tools for querying UE project and engine information.

Tools:
    get_project_info          — read .uproject: engine version, modules, plugins
    get_engine_install_info   — engine path, version, build type (source/Launcher)
    check_mcp_compatibility   — list supported/unsupported commands for current engine
"""

from __future__ import annotations

import json
import os
import platform
from pathlib import Path
from typing import Any

from mcp.server.fastmcp import FastMCP

# ─── Shared helpers ────────────────────────────────────────────────────────────

def _find_uproject(project_dir: str | None) -> Path | None:
    """Find the first .uproject in a directory (up to 2 levels deep)."""
    if not project_dir:
        return None
    root = Path(project_dir)
    for pattern in ("*.uproject", "*/*.uproject"):
        matches = list(root.glob(pattern))
        if matches:
            return matches[0]
    return None


def _find_engine_root(uproject_path: Path, engine_assoc: str) -> tuple[Path | None, str]:
    """Return (engine_root, source) — mirrors install.py logic (no shutil dep)."""
    # Source-built: sibling Engine/ dir
    if engine_assoc == "":
        sibling = uproject_path.parent.parent / "Engine"
        if (sibling / "Build" / "Build.version").exists():
            return sibling.parent, "sibling"

    # Windows registry
    if platform.system() == "Windows" and engine_assoc:
        try:
            import winreg
            for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
                try:
                    key = winreg.OpenKey(hive,
                        rf"SOFTWARE\EpicGames\Unreal Engine\{engine_assoc}")
                    val, _ = winreg.QueryValueEx(key, "InstalledDirectory")
                    p = Path(val)
                    if (p / "Engine" / "Build" / "Build.version").exists():
                        return p, "registry"
                    if (p / "Build" / "Build.version").exists():
                        return p.parent, "registry"
                except OSError:
                    pass
            # Custom build map
            try:
                key = winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                    r"SOFTWARE\Epic Games\Unreal Engine\Builds")
                i = 0
                while True:
                    try:
                        _name, value, _ = winreg.EnumValue(key, i)
                        p = Path(value)
                        if (p / "Engine" / "Build" / "Build.version").exists():
                            return p, "registry-custom"
                        i += 1
                    except OSError:
                        break
            except OSError:
                pass
        except ImportError:
            pass

    # Environment variable
    if ue_root := os.environ.get("UE_ROOT"):
        p = Path(ue_root)
        if (p / "Engine" / "Build" / "Build.version").exists():
            return p, "env"
        if (p / "Build" / "Build.version").exists():
            return p.parent, "env"

    return None, "unknown"


def _read_build_version(engine_root: Path) -> dict:
    bv_path = engine_root / "Engine" / "Build" / "Build.version"
    if not bv_path.exists():
        return {}
    with bv_path.open() as f:
        return json.load(f)


# ─── Compatibility matrix ─────────────────────────────────────────────────────

# (command_or_feature, min_major, min_minor, note)
COMPAT_MATRIX: list[tuple[str, int, int, str]] = [
    ("UEditorSubsystem",                4, 22, ""),
    ("UToolMenus",                      4, 22, ""),
    ("UDeveloperSettings",              4, 22, ""),
    ("LiveCoding module",               4, 22, ""),
    ("create_input_action",             5,  0, "Enhanced Input - UE5 only"),
    ("create_input_mapping_context",    5,  0, "Enhanced Input - UE5 only"),
    ("add_input_mapping",               5,  0, "Enhanced Input - UE5 only"),
    ("set_input_action_type",           5,  0, "Enhanced Input - UE5 only"),
    ("BlueprintEditorLibrary",          5,  0, "Excluded from Build.cs on UE4"),
    ("FAppStyle (editor style)",        5,  0, "Uses FEditorStyle fallback on UE4 via MCP_STYLE_NAME"),
]


# ─── Tool registration ─────────────────────────────────────────────────────────

def register_project_info_tools(mcp: FastMCP):

    @mcp.tool()
    def get_project_info(project_path: str) -> dict[str, Any]:
        """
        Read a .uproject file and return project metadata.

        Args:
            project_path: Path to the .uproject file or the directory containing it.

        Returns:
            dict with keys: project_name, uproject_path, engine_association,
            engine_type (source|launcher), modules (list), plugins (list),
            unrealmcp_installed (bool)
        """
        p = Path(project_path)
        if p.is_dir():
            uproject = _find_uproject(project_path)
            if not uproject:
                return {"error": f"No .uproject file found in: {project_path}"}
        elif p.suffix.lower() == ".uproject":
            uproject = p
        else:
            return {"error": f"Expected .uproject path or directory, got: {project_path}"}

        if not uproject.exists():
            return {"error": f".uproject not found: {uproject}"}

        with uproject.open(encoding="utf-8") as f:
            data = json.load(f)

        engine_assoc: str = data.get("EngineAssociation", "")
        engine_type = "source" if engine_assoc == "" else "launcher"

        modules = [
            {"name": m.get("Name"), "type": m.get("Type"), "phase": m.get("LoadingPhase")}
            for m in data.get("Modules", [])
        ]
        plugins = [
            {"name": pl.get("Name"), "enabled": pl.get("Enabled", False)}
            for pl in data.get("Plugins", [])
        ]
        unrealmcp_installed = any(pl["name"] == "UnrealMCP" for pl in plugins)

        return {
            "project_name": uproject.stem,
            "uproject_path": str(uproject),
            "engine_association": engine_assoc,
            "engine_type": engine_type,
            "modules": modules,
            "module_count": len(modules),
            "plugins": plugins,
            "plugin_count": len(plugins),
            "unrealmcp_installed": unrealmcp_installed,
        }

    @mcp.tool()
    def get_engine_install_info(project_path: str) -> dict[str, Any]:
        """
        Detect the Unreal Engine installation for a given project.

        Resolves engine root via:
        1. Sibling Engine/ directory (source builds, EngineAssociation="")
        2. Windows registry (Launcher installs)
        3. UE_ROOT environment variable
        4. Returns 'unknown' source if not found

        Args:
            project_path: Path to the .uproject file or directory.

        Returns:
            dict with keys: engine_root, source, major_version, minor_version,
            patch_version, branch_name, build_type (source|launcher)
        """
        p = Path(project_path)
        if p.is_dir():
            uproject = _find_uproject(project_path)
            if not uproject:
                return {"error": f"No .uproject found in: {project_path}"}
        elif p.suffix.lower() == ".uproject":
            uproject = p
        else:
            return {"error": f"Expected .uproject path or directory, got: {project_path}"}

        if not uproject.exists():
            return {"error": f".uproject not found: {uproject}"}

        with uproject.open(encoding="utf-8") as f:
            data = json.load(f)

        engine_assoc: str = data.get("EngineAssociation", "")
        engine_root, source = _find_engine_root(uproject, engine_assoc)

        if not engine_root:
            return {
                "engine_root": None,
                "source": "not_found",
                "error": "Could not detect engine root. Set UE_ROOT env var or use --engine-root.",
            }

        bv = _read_build_version(engine_root)
        build_type = "source" if engine_assoc == "" else "launcher"

        return {
            "engine_root": str(engine_root),
            "source": source,
            "build_type": build_type,
            "engine_association": engine_assoc,
            "major_version": bv.get("MajorVersion"),
            "minor_version": bv.get("MinorVersion"),
            "patch_version": bv.get("PatchVersion"),
            "branch_name": bv.get("BranchName", ""),
            "version_string": f"UE {bv.get('MajorVersion')}.{bv.get('MinorVersion')}.{bv.get('PatchVersion')}",
        }

    @mcp.tool()
    def check_mcp_compatibility(project_path: str) -> dict[str, Any]:
        """
        Check UnrealMCP compatibility for a given UE project.

        Reads the project's engine version and compares against the known
        compatibility matrix to list supported and unsupported features/commands.

        Args:
            project_path: Path to the .uproject file or directory.

        Returns:
            dict with keys: engine_version, supported (list), unsupported (list),
            warnings (list), overall_compatible (bool)
        """
        p = Path(project_path)
        if p.is_dir():
            uproject = _find_uproject(project_path)
            if not uproject:
                return {"error": f"No .uproject found in: {project_path}"}
        elif p.suffix.lower() == ".uproject":
            uproject = p
        else:
            return {"error": f"Expected .uproject path or directory, got: {project_path}"}

        if not uproject.exists():
            return {"error": f".uproject not found: {uproject}"}

        with uproject.open(encoding="utf-8") as f:
            data = json.load(f)

        engine_assoc: str = data.get("EngineAssociation", "")
        engine_root, _ = _find_engine_root(uproject, engine_assoc)

        if not engine_root:
            return {"error": "Cannot determine engine version: engine root not found."}

        bv = _read_build_version(engine_root)
        major = bv.get("MajorVersion")
        minor = bv.get("MinorVersion")

        if not isinstance(major, int) or not isinstance(minor, int):
            return {"error": "Could not read major/minor version from Build.version"}

        # Minimum supported
        if (major, minor) < (4, 22):
            return {
                "engine_version": f"UE {major}.{minor}",
                "overall_compatible": False,
                "error": "UnrealMCP requires UE 4.22 or later.",
                "supported": [],
                "unsupported": [],
                "warnings": [],
            }

        supported = []
        unsupported = []
        warnings = []

        for feat, req_major, req_minor, note in COMPAT_MATRIX:
            if (major, minor) >= (req_major, req_minor):
                supported.append(feat)
            else:
                unsupported.append({"feature": feat, "note": note})
                if note:
                    warnings.append(note)

        return {
            "engine_version": f"UE {major}.{minor}.{bv.get('PatchVersion', 0)}",
            "major_version": major,
            "minor_version": minor,
            "overall_compatible": True,
            "supported": supported,
            "unsupported": unsupported,
            "warning_count": len(warnings),
            "warnings": warnings,
        }
