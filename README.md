<div align="center">

# Model Context Protocol for Unreal Engine
<span style="color: #555555">unreal-mcp</span>

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.5%2B-orange)](https://www.unrealengine.com)
[![Python](https://img.shields.io/badge/Python-3.10%2B-yellow)](https://www.python.org)
[![Status](https://img.shields.io/badge/Status-Experimental-red)](https://github.com/kazeliTX/unreal-self-monitor)

</div>

This project enables AI assistant clients like Cursor, Windsurf and Claude to control Unreal Engine through natural language using the Model Context Protocol (MCP).

## ⚠️ Experimental Status

This project is currently in an **EXPERIMENTAL** state. Breaking changes may occur without notice, and production use is not recommended.

---

## 🌟 Capabilities

| Category | Commands |
|----------|----------|
| **Actor Management** | Spawn/delete actors, set transforms, query properties, select/duplicate, attach/detach, tags, labels, WorldSettings |
| **Blueprint Development** | Create BPs, add components, set properties/physics/collision, compile, query variables/functions/components |
| **Blueprint Node Graph** | Add event/function/variable/math/branch/cast/sequence nodes, connect nodes, create custom functions |
| **UMG / UI** | Create Widget Blueprints, add TextBlock/Button/Image/ProgressBar/HBox/VBox, bind events, set visibility/anchors |
| **Level Management** | New/open/save levels, get current level name |
| **Asset Management** | List/find/duplicate/rename/delete/save assets, create folders, DataTable CRUD |
| **Project Settings** | Enhanced Input actions/mappings/contexts, console commands, project settings |
| **Diagnostics** | Viewport camera info, actor screen position, highlight actor, hot reload, Live Coding status, source file read/write |
| **Editor Lifecycle** | Check editor status, launch/restart editor, wait for ready |
| **Log Analysis** | Read/tail/search/analyze UE log files (crash-safe, no TCP dependency) |

**Total**: ~100 C++ registered commands + 41 Python MCP tools across 11 tool modules.

---

## 🧩 Architecture

```
AI Client (Claude / Cursor / Windsurf)
    │  MCP Protocol (stdio)
    ▼
Python MCP Server  [FastMCP]
    │  JSON over TCP  port 55557
    ▼
UnrealMCPBridge  [UEditorSubsystem]
    └── FMCPCommandRegistry
         ├── EditorCommands
         ├── BlueprintCommands
         ├── BlueprintNodeCommands
         ├── UMGCommands
         ├── ProjectCommands
         ├── LevelCommands
         ├── AssetCommands
         └── DiagnosticsCommands
```

Key design principles:
- **Command registry pattern** — new commands register without touching routing logic
- **Python auto-discovery** — any `xxx_tools.py` with `register_xxx_tools(mcp)` is loaded automatically
- **Stateless connections** — each command uses an independent TCP connection
- **Four-tier compile system** — Blueprint → Python hot reload → C++ Live Coding → UBT full build

---

## 📂 Directory Structure

```
unreal-mcp-main/
├── MCPGameProject/
│   └── Plugins/UnrealMCP/            ← C++ UE plugin
│       └── Source/UnrealMCP/
│           ├── Public/Commands/      ← Command handler headers
│           └── Private/Commands/     ← Command implementations
├── Python/
│   ├── unreal_mcp_server.py          ← MCP server entry point
│   ├── tools/                        ← Auto-discovered tool modules
│   │   ├── base.py                   ← send_unreal_command() / make_error()
│   │   ├── editor_tools.py
│   │   ├── blueprint_tools.py
│   │   ├── node_tools.py
│   │   ├── umg_tools.py
│   │   ├── project_tools.py
│   │   ├── level_tools.py
│   │   ├── asset_tools.py
│   │   ├── log_tools.py
│   │   ├── diagnostics_tools.py
│   │   ├── compile_tools.py
│   │   └── system_tools.py
│   └── scripts/
│       ├── debug_runner.py           ← Batch command runner / smoke tests
│       ├── compile_watch.py          ← Compilation status monitor
│       └── session_end_reminder.py   ← Stop hook script
├── References/                       ← Knowledge base
│   ├── Plans/                        ← Task execution plans
│   ├── Notes/                        ← Debug logs & solutions
│   ├── Architecture/                 ← Architecture docs
│   └── SOP/                          ← Standard operating procedure
├── Docs/                             ← Tool reference documentation
├── .claude/
│   ├── commands/                     ← Claude slash commands
│   │   ├── ue-plan.md                ← /ue-plan: create task plan + git branch
│   │   ├── ue-note.md                ← /ue-note: save debug note
│   │   ├── ue-sop.md                 ← /ue-sop: view workflow
│   │   └── ue-status.md              ← /ue-status: check editor + MCP status
│   └── settings.local.json           ← Stop hook configuration
└── mcp.json                          ← MCP client configuration example
```

---

## 🚀 Quick Start

### Prerequisites
- Unreal Engine 5.5+
- Python 3.10+
- MCP Client (Claude, Cursor, or Windsurf)

### 1. Set Up the UE Project

**Option A — Use the sample project (fastest):**
1. Right-click `MCPGameProject/MCPGameProject.uproject` → Generate Visual Studio project files
2. Open `.sln`, select `Development Editor`, build
3. Open the project in UE — the plugin starts the TCP server automatically

**Option B — Add to your existing project:**
1. Copy `MCPGameProject/Plugins/UnrealMCP` into your project's `Plugins/` folder
2. Enable via **Edit → Plugins → UnrealMCP**
3. Regenerate and rebuild

### 2. Set Up the Python Server

```bash
# Install uv if not already installed
pip install uv

# From the Python/ directory — uv handles the virtualenv automatically
cd Python
uv run unreal_mcp_server.py
```

**Dependencies** (managed by uv):
```
mcp[cli]          # required — FastMCP framework
psutil>=5.9.0     # optional — editor process detection
Pillow>=10.0.0    # optional — advanced screenshot processing
```

### 3. Configure Your MCP Client

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory",
        "<absolute/path/to/Python>",
        "run",
        "unreal_mcp_server.py"
      ]
    }
  }
}
```

| MCP Client | Config File Location |
|------------|----------------------|
| Claude Desktop | `%USERPROFILE%\.config\claude-desktop\mcp.json` |
| Cursor | `.cursor/mcp.json` (project root) |
| Windsurf | `%USERPROFILE%\.config\windsurf\mcp.json` |

A ready-to-edit example is at `mcp.json` in the project root.

---

## 🤖 AI Automation Workflow (Claude)

This project includes a structured workflow for Claude-driven automation:

### Slash Commands
| Command | Purpose |
|---------|---------|
| `/ue-plan` | Analyze a task, optionally create a git branch, generate a Plan document |
| `/ue-note` | Save a debug note / solution to `References/Notes/` |
| `/ue-status` | Check editor process, TCP port, ping, and world load status |
| `/ue-sop` | Display the full Standard Operating Procedure |

### Standard Workflow
1. `/ue-plan` → confirm git branch + plan document
2. Capability check → implement missing commands if needed
3. Execute → verify after each key operation
4. `/ue-note` → document any errors encountered
5. Update `References/Plans/` checkboxes + commit

See [`References/SOP/UE_Automation_SOP.md`](References/SOP/UE_Automation_SOP.md) for full details.

---

## 🛠 Debugging

```bash
# Smoke test — verify core commands are working
python Python/scripts/debug_runner.py --smoke-test

# Watch compilation status
python Python/scripts/compile_watch.py --interval 3

# Run a custom command batch
python Python/scripts/debug_runner.py my_commands.json
```

**Smoke test script format:**
```json
[
  {"type": "ping", "params": {}},
  {"type": "get_capabilities", "params": {}},
  {"type": "get_current_level_name", "params": {}}
]
```

---

## 📋 Implementation Status

| Phase | Status | Content |
|-------|--------|---------|
| Architecture Refactor | ✅ Complete | FMCPCommandRegistry, auto_register_tools, base.py |
| Phase 1 | ✅ Complete | Level/Asset/Actor/WorldSettings/Collision/Blueprint query/Node graph/UMG/Enhanced Input |
| Phase 2A | ✅ Complete | DiagnosticsCommands + log/diagnostics/compile/system Python tools |
| Phase 2B | Pending | C++ TestCommands (validate_blueprint / run_level_validation) |
| Phase 2C/D | Pending | Animation Blueprint / Material / Niagara / PIE |

---

## License
MIT

## Questions

For questions, reach out on GitHub: [@kazeli](https://github.com/kazeliTX)
