# UnrealMCP — Claude 项目上下文

## 项目概述

**UnrealMCP** 是一个让 AI 客户端（Claude / Cursor / Windsurf）通过 MCP 协议以自然语言控制 Unreal Engine 编辑器的实验性插件。

- **协议**：Model Context Protocol（MCP），TCP 端口 `55557`
- **引擎支持**：UE 4.22+ / UE 5.x（含 UE4/UE5 兼容层）
- **许可**：MIT

---

## 安全规约（强制）

> 适用于所有文档、Notes、计划、commit message 及输出内容。

1. **禁止记录真实路径**：用户名、磁盘符+完整路径、机器名一律用占位符替代
   - 当前仓库路径 → `<repo_root>`
   - 外部项目路径 → `<project_root>`、`<engine_root>`
2. **禁止记录第三方项目名称**：用 `<ue4_project>`、`<target_project>` 等语义占位符替代
   - .uproject 文件名 → `<project>.uproject`
3. **例外**：`.claude/settings.local.json` 等功能性配置文件需要真实路径，但已在 `.gitignore` 中排除

---

## 标准工作流（SOP）

每次执行任务须遵循以下流程：

1. **Git 确认** → `/ue-plan` 时首先询问是否启用 Git 管控；若是，读取 `git log`，checkout 新分支 `ue/YYYY-MM-DD-task-name`
2. **分析需求** → 用 `/ue-plan` 生成计划文档（存入 `References/Plans/`），用户确认后执行
3. **能力评估** → 确认所需 C++ 命令/Python 工具已就位；缺失则先实现并验证
4. **执行任务** → 关键操作前先查状态，操作后查询确认，用 `batch` 合并请求
5. **问题沉淀** → 遇到任何错误/偏差，用 `/ue-note` 保存到 `References/Notes/`
6. **收尾** → 更新 CLAUDE.md；若 Git 已启用，按文件类别逐步暂存并创建 commit；合并后删除功能分支

**Git 规范**：
- 分支：`ue/YYYY-MM-DD-task-name`（英文小写，连字符）
- Commit 前缀：`feat:` / `fix:` / `docs:` / `refactor:`
- 每次 commit 必须附 `Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>`
- 不使用 `git add -A`，按文件类别逐步暂存
- 合并到 main 后立即删除功能分支：`git branch -d <branch>`

**自定义命令（项目级 Slash Commands）**：

| 命令 | 用途 |
|------|------|
| `/ue-plan` | 为新任务生成 Plan 文档 |
| `/ue-note` | 保存 Debug 笔记/解决方案 |
| `/ue-sop` | 查看完整 SOP |
| `/ue-status` | 检查编辑器和 MCP 连接状态 |

---

## 技术栈

| 层 | 技术 |
|----|------|
| UE 插件 | C++，`UEditorSubsystem`，TCP Socket |
| MCP 服务器 | Python 3.10+，`FastMCP`（mcp 库） |
| 包管理 | `uv` |
| 通信格式 | JSON over TCP（每条命令独立连接） |

---

## 项目目录结构

```
unreal-mcp-main/
├── CLAUDE.md                        ← 本文件
├── References/                      ← 文档与知识库
│   ├── Plans/                       ← 执行计划（/ue-plan 生成）
│   ├── Notes/                       ← Debug 日志与解决方案（/ue-note 生成）
│   ├── Architecture/                ← 架构文档
│   └── SOP/                         ← 标准操作流程
├── .claude/
│   ├── commands/                    ← 项目级 Slash Commands（ue-plan/note/sop/status）
│   └── settings.local.json         ← Hook 配置（gitignored，含真实路径）
├── MCPGameProject/
│   └── Plugins/UnrealMCP/Source/UnrealMCP/
│       ├── Public/
│       │   ├── UnrealMCPBridge.h           ← UEditorSubsystem 声明
│       │   ├── UnrealMCPSettings.h         ← UDeveloperSettings（端口/自动启动）
│       │   ├── UnrealMCPCompat.h           ← UE4/5 兼容宏
│       │   ├── MCPCommandRegistry.h        ← 命令注册表
│       │   └── Commands/                   ← 各命令模块头文件
│       ├── Private/
│       │   ├── UnrealMCPBridge.cpp         ← TCP 服务器 + 命令路由 + 编辑器菜单
│       │   └── Commands/                   ← 各命令模块实现
│       └── UnrealMCP.Build.cs
└── Python/
    ├── unreal_mcp_server.py               ← MCP 服务器入口（自动发现 tools/）
    ├── tools/                             ← MCP 工具模块（xxx_tools.py 自动注册）
    └── scripts/
        ├── install.py                     ← 一键迁移安装脚本
        ├── compile_watch.py               ← 编译状态监视器
        ├── debug_runner.py                ← 批量命令执行器/冒烟测试
        └── session_end_reminder.py        ← Stop hook 文档化提醒
```

---

## 架构说明

### C++ 命令分发机制

```
TCP 连接 → UnrealMCPBridge::ExecuteCommand()
    ├── "ping"             → 内置
    ├── "get_capabilities" → 内置（返回所有注册命令名）
    ├── "batch"            → 内置（顺序执行子命令列表）
    └── 其他               → FMCPCommandRegistry::ExecuteCommand()
                                └── TMap<FString, FMCPCommandHandler>
```

**添加新命令模块的步骤**：
1. 创建 `FUnrealMCPXxxCommands` 类，实现 `RegisterCommands(FMCPCommandRegistry&)`
2. 在 `UnrealMCPBridge.h` 中声明 `TSharedPtr<FUnrealMCPXxxCommands> XxxCommands`
3. 在 `UnrealMCPBridge.cpp` 构造函数中实例化并调用 `XxxCommands->RegisterCommands(*CommandRegistry)`
4. 在析构函数中调用 `XxxCommands.Reset()`

### Python 工具自动发现

`unreal_mcp_server.py` 使用 `pkgutil.iter_modules` 扫描 `tools/` 目录，自动注册所有 `register_*_tools(mcp)` 函数。**新增工具文件无需修改服务器入口文件**。

---

## C++ 命令模块

### EditorCommands
**Actor**：`get_actors_in_level`、`find_actors_by_name`、`spawn_actor`、`delete_actor`、`set_actor_transform`、`get_actor_properties`、`set_actor_property`、`spawn_blueprint_actor`、`duplicate_actor`
**视口/选择**：`focus_viewport`、`take_screenshot`、`select_actor`、`deselect_all`、`get_selected_actors`
**标签/层级**：`set_actor_label`、`get_actor_label`、`add_actor_tag`、`remove_actor_tag`、`get_actor_tags`、`attach_actor_to_actor`、`detach_actor`
**世界**：`get_world_settings`、`set_world_settings`

### BlueprintCommands
**管理**：`create_blueprint`、`add_component_to_blueprint`、`set_static_mesh_properties`、`set_physics_properties`、`compile_blueprint`、`set_blueprint_property`、`set_pawn_properties`
**碰撞**：`set_component_collision_profile`、`set_component_collision_enabled`
**查询**：`get_blueprint_variables`、`get_blueprint_functions`、`get_blueprint_components`、`list_blueprints`、`get_blueprint_compile_errors`

### BlueprintNodeCommands
`add_blueprint_event_node`、`add_blueprint_input_action_node`、`add_blueprint_function_node`、`connect_blueprint_nodes`、`add_blueprint_variable`、`add_blueprint_get_self_component_reference`、`add_blueprint_self_reference`、`find_blueprint_nodes`、`add_blueprint_get_variable_node`、`add_blueprint_set_variable_node`、`add_blueprint_branch_node`、`add_blueprint_sequence_node`、`add_blueprint_cast_node`、`add_blueprint_math_node`、`add_blueprint_print_string_node`、`add_blueprint_custom_function`

### UMGCommands
`create_umg_widget_blueprint`、`add_text_block_to_widget`、`add_button_to_widget`、`bind_widget_event`、`add_widget_to_viewport`、`set_text_block_binding`、`add_image_to_widget`、`add_progress_bar_to_widget`、`add_horizontal_box_to_widget`、`add_vertical_box_to_widget`、`set_widget_visibility`、`set_widget_anchor`、`update_text_block_text`、`get_widget_tree`

> **UMG 参数约定**：C++ 侧用 `blueprint_name`（BP 资产名）和 `widget_name`（控件名）；Python 侧第一参数 `widget_name` 发送时映射为 `blueprint_name`。

### ProjectCommands
`create_input_mapping`、`run_console_command`、`get_project_settings`
UE5 专属（Enhanced Input）：`create_input_action`、`create_input_mapping_context`、`add_input_mapping`、`set_input_action_type`

### LevelCommands
`new_level`、`open_level`、`save_current_level`、`save_all_levels`、`get_current_level_name`

### AssetCommands
**浏览**：`list_assets`、`find_asset`、`does_asset_exist`、`get_asset_info`
**文件夹**：`create_folder`、`list_folders`、`delete_folder`
**操作**：`duplicate_asset`、`rename_asset`、`delete_asset`、`save_asset`、`save_all_assets`
**DataTable**：`create_data_table`、`add_data_table_row`、`get_data_table_rows`

### DiagnosticsCommands（Phase 2A）
**视觉感知**：`get_viewport_camera_info`、`get_actor_screen_position`、`highlight_actor`
**热重载**：`trigger_hot_reload`、`get_live_coding_status`
**源文件**：`get_source_file`、`modify_source_file`（自动 `.bak.时间戳` 备份）
**路径**：`get_engine_path`

### TestCommands（Phase 2B）
**Blueprint 验证**：`validate_blueprint` — 编译状态、错误/警告列表、节点数、变量数
**关卡验证**：`run_level_validation` — Blueprint 编译错误、缺失 Mesh、孤立组件、Asset Redirector

---

## Python 工具模块

| 文件 | 主要工具 |
|------|---------|
| `base.py` | `send_unreal_command()`、`make_error()` |
| `editor_tools.py` | Actor/视口/选择/标签/附着/WorldSettings |
| `blueprint_tools.py` | BP 创建/组件/属性/编译/碰撞/查询 |
| `node_tools.py` | 节点图操作 |
| `umg_tools.py` | Widget Blueprint 工具 |
| `project_tools.py` | 输入/控制台命令/项目设置 |
| `level_tools.py` | 关卡管理 |
| `asset_tools.py` | 资产/DataTable 管理 |
| `log_tools.py` | UE 日志读取/分析（直接读文件，crash-safe） |
| `diagnostics_tools.py` | 截图/相机/Actor 屏幕坐标/场景快照 |
| `compile_tools.py` | C++/Python 文件读写/热重载（4层）/UBT |
| `system_tools.py` | 编辑器进程生命周期管理 |
| `project_info_tools.py` | `get_project_info` / `get_engine_install_info` / `check_mcp_compatibility` |

**编译四层**：Tier1 `compile_blueprint`（即时）→ Tier2 `reload_python_tool`（即时）→ Tier3 `trigger_hot_reload`（5-60s）→ Tier4 `run_ubt_build`（数分钟）

---

## 开发规范

### 新增 C++ 命令
1. 在对应 `Commands/*.h` 中声明 `HandleXxx(const TSharedPtr<FJsonObject>& Params)`
2. 在 `RegisterCommands()` 中注册
3. 用 `FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("..."))` 返回错误
4. 若需新模块：在 `UnrealMCPBridge.h/.cpp` 中添加

### 新增 Python 工具
1. 创建 `Python/tools/xxx_tools.py`，实现 `register_xxx_tools(mcp: FastMCP)`
2. 调用 `send_unreal_command()` 或 `make_error()`
3. 无需修改 `unreal_mcp_server.py`（自动发现）

### 错误响应格式
- C++ 失败：`{"success": false, "error": "message"}`
- Python 失败：`make_error(msg)` → `{"success": false, "message": "..."}`

### UE4/UE5 兼容
- 样式宏：`MCP_STYLE_NAME`（`FAppStyle` vs `FEditorStyle`）
- Enhanced Input 宏：`#if MCP_ENHANCED_INPUT_SUPPORTED`
- Build.cs：`Target.Version.MajorVersion >= 5` 条件引入 `BlueprintEditorLibrary`

---

## Hook 开发规范

**三条铁律**：
1. **stdout 只能是 JSON** — 调试信息用 `sys.stderr`
2. **Stop hook 必须检查 `stop_hook_active`** — 为 `true` 时直接 `sys.exit(0)`
3. **Windows 含空格路径必须加引号** — `"python \"<path with spaces>/script.py\""`

**Stop hook 最小模板**：
```python
hook_input = json.load(sys.stdin)
if hook_input.get("stop_hook_active"):
    sys.exit(0)
print(json.dumps({"decision": "block", "reason": "..."}))
sys.exit(0)
```

---

## 调试工具

```bash
python Python/scripts/debug_runner.py --smoke-test   # 冒烟测试
python Python/scripts/compile_watch.py --interval 3  # 监视编译状态
python Python/scripts/install.py <target.uproject> --dry-run  # 预览安装
```

---

## 实现进度

| 阶段 | 状态 | 内容 |
|------|------|------|
| 架构重构 | ✅ 完成 | FMCPCommandRegistry、auto_register_tools、base.py |
| Phase 1 | ✅ 完成 | 关卡/资产/DataTable/Actor/WorldSettings/Collision/蓝图查询/节点/UMG/Enhanced Input |
| Phase 2A | ✅ 完成 | DiagnosticsCommands + log/diagnostics/compile/system Python 工具 |
| Phase 2B | ✅ 完成 | TestCommands（validate_blueprint / run_level_validation） |
| Phase 2C/D | 待实现 | 动画蓝图 / 材质 / Niagara / PIE |
| 可移植性 Phase C | ✅ 完成 | UUnrealMCPSettings + 编辑器 Tools 菜单 MCP 开关 |
| 可移植性 Phase B P1 | ✅ 完成 | UnrealMCPCompat.h + Build.cs UE4/5 分支 + Enhanced Input 条件编译 |
| 可移植性 Phase A | ✅ 完成 | `install.py` 一键迁移脚本（UE4.22+/UE5，源码/Launcher 双支持） |
| 可移植性 Phase P3 | ✅ 完成 | `project_info_tools.py`（get_project_info / check_mcp_compatibility） |
| UE4.24 安装验证 | ✅ 完成 | 已在 UE4.24.2 自编译项目成功编译；所有 API 差异已用 #if ENGINE_MAJOR_VERSION 守卫（详见 References/Notes/2026-02-28_UE4-compat-fixes.md） |

---

## Python 依赖

```
mcp[cli]          # 必需：FastMCP 框架
psutil>=5.9.0     # 可选：system_tools.py 进程检测
Pillow>=10.0.0    # 可选：高级截图处理
```
