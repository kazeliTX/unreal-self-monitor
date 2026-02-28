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
4. **Git 提交前必须检查**：`git diff --staged` 或 `git show` 确认暂存内容不含敏感信息后再执行 `git commit`

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
`new_level`、`open_level`、`save_current_level`、`save_all_levels`、`get_current_level_name`、`get_level_dirty_state`

### AssetCommands
**浏览**：`list_assets`、`find_asset`、`does_asset_exist`、`get_asset_info`
**文件夹**：`create_folder`、`list_folders`、`delete_folder`
**操作**：`duplicate_asset`、`rename_asset`、`delete_asset`、`save_asset`、`save_all_assets`
**DataTable**：`create_data_table`、`add_data_table_row`、`get_data_table_rows`
**编辑器**：`open_asset_editor`（在编辑器中打开任意资产，参数 `asset_path`）

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
| `level_tools.py` | 关卡管理（含 `get_level_dirty_state`、`safe_switch_level`） |
| `asset_tools.py` | 资产/DataTable 管理 |
| `log_tools.py` | UE 日志读取/分析（直接读文件，crash-safe） |
| `diagnostics_tools.py` | 截图/相机/Actor 屏幕坐标/场景快照 |
| `compile_tools.py` | C++/Python 文件读写/热重载（4层）/UBT/`kill_editor`/`full_rebuild` |
| `system_tools.py` | 编辑器进程生命周期管理 |
| `project_info_tools.py` | `get_project_info` / `get_engine_install_info` / `check_mcp_compatibility` |

**编译四层**：Tier1 `compile_blueprint`（即时）→ Tier2 `reload_python_tool`（即时）→ Tier3 `trigger_hot_reload`（5-60s）→ Tier4 `run_ubt_build`（数分钟）

---

## 执行效率原则

> 以下原则来自实战优化，每次任务执行时必须遵守。

### 原则 1：能力预检优先，编译后再启动编辑器

**禁止「先启动编辑器 → 发现命令缺失 → 关闭重启」的反模式。**

正确流程：
1. 用 `get_capabilities` 检查可用命令（对比 CLAUDE.md 命令列表）
2. 发散预测：除直接需求外，预测接下来 1-2 步可能用到的命令
3. 全部缺失命令统一实现 → 一次 full_rebuild → 再启动编辑器

| 用户需求 | 发散预测应额外准备的能力 |
|---------|----------------------|
| 打开动画蓝图 | create_montage / add_anim_notify / edit_anim_curve |
| 创建角色 BP | SkeletalMesh 组件 / 输入绑定 / 碰撞设置 |
| 创建关卡 | 光照 / SkyAtmosphere / NavMesh |
| 创建 Widget | 事件绑定 / 数据绑定 / 动画 |

### 原则 2：编译层级快速判定（跳过 LiveCoding 评估）

新增 MCP 命令 ≈ 必然涉及 `RegisterCommands()` 注册，**直接走 Tier 4，不需要逐级评估**：

```
涉及 RegisterCommands / .h 变更 / 新类 / Build.cs？
  → 是 → 直接 full_rebuild（Tier 4），跳过 LiveCoding 考虑
  → 否（仅函数体） → trigger_hot_reload（Tier 3）
```

多处 C++ 变更合并为**一次** full_rebuild，不分多轮编译。

### 原则 3：会话缓存，不重复探测

Step 0 执行一次后，以下内容全程复用，不再重复查询：
- TCP 协议字段：`"type"`（不是 `"command"`）
- 可用命令集：来自 `get_capabilities` 的缓存
- 项目路径 / UBT target 名

### 原则 4：批处理优先

多个独立的只读 MCP 查询必须合并为一次 `batch` 调用：
```python
# 正确：1次往返
send_cmd("batch", {"commands": [
    {"type": "get_current_level_name", "params": {}},
    {"type": "get_world_settings", "params": {}},
    {"type": "get_actors_in_level", "params": {}}
]})
```

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

---

## 已知问题与经验（实战记录）

> 详见 `References/Notes/2026-02-28_处理虚幻编译弹窗等问题.md`

### ⚠️ 问题1：new_level / open_level 触发 "Save current changes?" 弹框

**现象**：`new_level`/`open_level` 在切换关卡时，若当前关卡有未保存修改，UE 弹出模态保存对话框。
**解决方案**：C++ 侧在切换前调用 `SilentSaveAllDirtyPackages()`（`bPromptUserToSave=false`），自动静默保存所有 map + content 包。`/Temp/` 包无磁盘路径时自动跳过。
**我们的状态**：`LevelCommands` 已有此机制，外部项目已验证有效。

### ⚠️ 问题2：save_current_level 对临时关卡触发 "Save As" 弹框

**现象**：`new_level`（无 `asset_path`）创建 `/Temp/Untitled` 关卡后，`save_current_level` 触发模态 Save As 对话框，期间所有 MCP TCP 请求超时（约 15s）。
**解决方案**：
1. C++ 侧检测 `/Temp/` 路径并立即返回错误（不调用 SaveCurrentLevel）
2. Python 侧：`get_level_dirty_state` 检查 `is_temp`，若是临时关卡跳过保存
3. 推荐：用 `safe_switch_level` 替代直接调用 `new_level`/`open_level`

### ⚠️ 问题3：spawn_blueprint_actor 重名 Actor 导致编辑器 Crash

**现象**：`actor_name` 在关卡中已存在时，UE 在 `LevelActor.cpp:585` 触发 Fatal Error。
**修复**：在 `HandleSpawnBlueprintActor` 中添加与 `HandleSpawnActor` 相同的重名检查。
**Python 防御**：`actor_name` 加时间戳或序号后缀保证唯一性。

### ⚠️ 问题4：启动编辑器时出现 "Rebuild modules?" 弹框

**现象**：修改 C++ 源码后直接启动编辑器，若 .dll 比源码旧，UE 弹出 GUI 重编译询问弹框。
**解决方案**：先用 UBT 预编译（命令行静默无弹框），再启动编辑器：
```bash
dotnet "<engine_root>/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" \
    <ProjectName>Editor Win64 Development \
    -Project="<project_root>/<project>.uproject" -WaitMutex
```

### ⚠️ 问题5：full_rebuild 后出现 "Recover Packages?" 弹框

**现象**：强制杀掉编辑器后重启，UE 检测到 `PackageRestoreData.json` 并弹出恢复对话框。
**解决方案**：Kill 编辑器后、启动前，删除：
- `<project_root>/Saved/Autosaves/PackageRestoreData.json`（停止弹框的关键）
- `<project_root>/Saved/Autosaves/Temp/`（不可恢复的临时关卡自动保存）

### ⚠️ 问题6：新增 MCP 命令后 LiveCoding 热重载无效

**现象**：`RegisterCommands()` 在模块初始化时调用，LiveCoding 仅更新函数体，不重新注册命令。新增命令必须 `full_rebuild`。
**四层编译策略**：
- Tier1 `compile_blueprint`（即时）→ 蓝图逻辑变更
- Tier2 `reload_python_tool`（即时）→ Python 工具变更
- Tier3 `trigger_hot_reload`（5-60s）→ C++ 函数体变更（**不适用于新命令注册**）
- Tier4 `full_rebuild`（数分钟）→ **新 MCP 命令、.h 头文件、类结构、Build.cs 变更**

### ⚠️ 问题7：UE5.6 + VS2022 14.44 编译失败（三处兼容性问题）

**现象**：安装到 UE5.6 / LyraStarterGame 时 UBT 编译失败。
**根因与修复**：
1. `Engine/SkyAtmosphere.h` 不存在 → 改用 `Components/SkyAtmosphereComponent.h`（`ASkyAtmosphere` 定义在此）
2. `Engine/WorldSettings.h` 在 UE5.6 不存在 → 删除版本条件，统一用 `GameFramework/WorldSettings.h`
3. VS2022 14.44 将 `StringConv.h` 中 C4459（`BufferSize` 遮蔽全局）升级为错误 → 在 `MCPServerRunnable.cpp` 头部加 `#pragma warning(disable: 4459)`；同时把 `TCHAR_TO_UTF8(*Response), Response.Len()` 改为 `FTCHARToUTF8 u(*Response); u.Get(), u.Length()` 修正字节长度
4. `TActorIterator` 缺少 `#include "EngineUtils.h"` → 已补全

**详见**：`References/Notes/2026-02-28_UE5.6-编译兼容性修复.md`

### ⚠️ 问题8：手写 TCP 测试脚本超时——字段名 `"type"` 不是 `"command"`

**现象**：Python 直接发送 `{"command": "ping", "params": {}}` 后连接超时，无响应。
**根因**：`MCPServerRunnable::Run()` 解析 `"type"` 字段（`TryGetStringField(TEXT("type"), ...)`），`"command"` 字段被忽略，服务器不发送任何回复。
**解决方案**：手写测试脚本必须用 `"type"` 字段：
```python
payload = json.dumps({"type": "ping", "params": {}}).encode("utf-8")
```
`unreal_mcp_server.py` 的 `send_command()` 已正确使用 `"type"`，Python 工具层无需改动。

### ⚠️ 问题9：Windows bash 下 `taskkill /F /IM` 无法杀死 UE 进程

**现象**：bash 中运行 `taskkill /F /IM UnrealEditor.exe` 命令执行不报错，但进程依然存在。
**根因**：bash 环境（Git Bash / MinGW）对 `/F`、`/IM` 等 Windows 路径式参数存在解析差异。
**解决方案**：改用 PowerShell：
```bash
powershell -Command "Stop-Process -Name UnrealEditor -Force -ErrorAction SilentlyContinue; Stop-Process -Name LiveCodingConsole -Force -ErrorAction SilentlyContinue"
```

### ⚠️ 问题10：外部项目 UBT target 名必须与 Target.cs 文件名一致

**现象**：对 LyraStarterGame 运行 `LyraStarterGameEditor` 目标时 UBT 报错 `Couldn't find target rules file`。
**根因**：UBT target 名来自 `Source/XxxEditor.Target.cs` 的文件名，LyraStarterGame 的编辑器 target 是 `LyraEditor`（`Source/LyraEditor.Target.cs`）。
**解决方案**：安装前先检查 `<project>/Source/` 下的 `*.Target.cs` 文件，使用正确的目标名：
```bash
ls "<project_root>/Source/" | grep "Editor.Target"
# → LyraEditor.Target.cs → target name: LyraEditor
```

**详见**：`References/Notes/2026-03-01_LyraStarterGame-MCP连接与open-asset-editor.md`

---

## Python MCP 工具编写规范

> 来自上游项目 copilot-instructions（已验证最佳实践）

所有带 `@mcp.tool()` 装饰器的函数必须遵守：

1. **禁止的参数类型**：`Any`、`object`、`Optional[T]`、`Union[T]`
2. **有默认值的参数**：用 `x: T = None`，不用 `x: T | None = None`，在函数体内处理默认值
3. **必须有 docstring**：包含参数说明和有效输入示例

```python
# ✅ 正确
@mcp.tool()
def my_tool(ctx: Context, name: str, count: int = 0) -> Dict[str, Any]:
    """Do something. Args: name: Actor name (e.g. 'Cube_01'). count: repeat count."""
    if count == 0:
        count = 1  # handle default in body
    ...

# ❌ 错误
@mcp.tool()
def my_tool(ctx: Context, name: str, count: Optional[int] = None) -> Dict[str, Any]:
    ...
```

---

## spawn_actor 支持的 actor_type

除基础类型外，以下类型已验证（外部项目实战）：

| actor_type | 类 | 备注 |
|------------|-----|------|
| `StaticMeshActor` | AStaticMeshActor | 默认 |
| `PointLight` | APointLight | 点光源 |
| `SpotLight` | ASpotLight | 聚光灯 |
| `DirectionalLight` | ADirectionalLight | 平行光 |
| `CameraActor` | ACameraActor | 摄像机 |
| `SkyLight` | ASkyLight | 天空光 |
| `SkyAtmosphere` | ASkyAtmosphere | 大气散射（UE 4.26+） |
| `ExponentialHeightFog` | AExponentialHeightFog | 指数高度雾 |

**关键**：`spawn_actor` 创建 `DirectionalLight` 后，必须显式设置 `bAtmosphereSunLight=true`，否则 SkyAtmosphere 不连接到方向光，场景渲染为黑色：
```python
set_actor_property(name="Sun_Light", property_name="bAtmosphereSunLight", property_value=True)
```

---

## set_actor_property 组件属性回落机制

`HandleSetActorProperty` 已实现：先在 Actor 本身查找属性，找不到时自动遍历所有 `UActorComponent` 查找。响应中包含 `"component": "ComponentName"` 字段（如 `"LightComponent0"`）。

---

## Skill 系统

在 `.claude/skills/<skill-name>/SKILL.md` 中定义可复用工作流。触发词在文件顶部 YAML frontmatter 中指定。当前 Skill：

| Skill | 触发词 |
|-------|--------|
| `create-basic-lighting-level` | "新建关卡并添加基础光照"、"set up basic lighting"、"create outdoor scene" |

---

## safe_switch_level 推荐用法

当需要切换关卡时，**始终优先使用 `safe_switch_level`** 而非直接调用 `new_level`/`open_level`：

```python
# 推荐：自动处理 dirty 状态，防止弹框
safe_switch_level(asset_path="/Game/Maps/MyLevel")  # 切换到指定关卡
safe_switch_level()  # 创建新空白关卡

# 不推荐（除非明确知道当前关卡已保存）
new_level(asset_path="/Game/Maps/MyLevel")
```
