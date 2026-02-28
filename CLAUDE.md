# UnrealMCP — Claude 项目上下文

## 项目概述

**UnrealMCP** 让 AI 客户端通过 MCP 协议以自然语言控制 Unreal Engine 编辑器。

- **协议**：Model Context Protocol（MCP），TCP 端口 `55557`
- **引擎支持**：UE 4.22+ / UE 5.x
- **许可**：MIT

---

## 安全规约（强制）

1. **禁止记录真实路径**：用占位符替代（仓库 → `<repo_root>`，外部项目 → `<project_root>`，引擎 → `<engine_root>`）
2. **禁止记录第三方项目名**：用 `<target_project>`、`<ue4_project>` 替代
3. **例外**：`.claude/settings.local.json` 等功能性配置（已在 `.gitignore`）
4. **Git 提交前必须检查**：`git diff --staged` 确认无真实路径后再 commit

---

## 标准工作流（SOP）

> 完整流程见 `References/SOP/Main_SOP.md`。Git 操作见 `Git_SOP.md`。新增命令见 `Plugin_Dev_SOP.md`。

**核心步骤**：⓪ 会话初始化（batch ping+capabilities）→ ① /ue-plan + 能力预检 → ② 实现缺口（一次 full_rebuild）→ ③ 执行（批处理优先）→ ④ /ue-note 问题沉淀 → ⑤ 收尾（更新 CLAUDE.md + Git commit）

**Slash Commands**：`/ue-plan` / `/ue-note` / `/ue-sop` / `/ue-status`

---

## 技术栈

| 层 | 技术 |
|----|------|
| UE 插件 | C++，`UEditorSubsystem`，TCP Socket |
| MCP 服务器 | Python 3.10+，`FastMCP`（mcp 库） |
| 包管理 | `uv` |
| 通信格式 | JSON over TCP（每条命令独立连接），字段名 `"type"` |

---

## 目录结构

```
unreal-mcp-main/
├── CLAUDE.md
├── References/
│   ├── Plans/          ← 执行计划（/ue-plan 生成）；history/ 子目录禁止自动读取
│   ├── Notes/          ← Known_Issues.md、UE_Compat.md（按需加载）
│   ├── Architecture/   ← Commands.md（命令全表）
│   └── SOP/            ← Main/Git/Plugin_Dev/Install SOP
├── .claude/commands/   ← Slash Commands
├── MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/
│   ├── Public/Commands/
│   └── Private/Commands/
└── Python/tools/       ← MCP 工具（自动发现注册）
```

---

## 架构说明

**C++ 命令分发**：
```
TCP → UnrealMCPBridge::ExecuteCommand()
    ├── ping / get_capabilities / batch  → 内置
    └── 其他 → FMCPCommandRegistry → TMap<FString, FMCPCommandHandler>
```

**Python 工具自动发现**：`unreal_mcp_server.py` 扫描 `tools/` 目录，自动注册所有 `register_*_tools(mcp)` 函数。

**当前命令数**：**117 条**（详见 `References/Architecture/Commands.md`）

命令模块：EditorCommands / BlueprintCommands / BlueprintNodeCommands / UMGCommands / LevelCommands / AssetCommands / MaterialCommands / ProjectCommands / DiagnosticsCommands / TestCommands

---

## 执行效率原则（必须遵守）

### 原则 1：能力预检优先，编译后再启动编辑器

禁止「先启动编辑器 → 发现命令缺失 → 关闭重启」的反模式。

正确流程：`get_capabilities` → 发散预测 → 统一实现缺口 → 一次 full_rebuild → 再启动

### 原则 2：编译层级快速判定

```
涉及 RegisterCommands / .h 变更 / 新类 / Build.cs？
  → 是 → 直接 full_rebuild（Tier 4），跳过 LiveCoding 评估
  → 否（仅函数体） → trigger_hot_reload（Tier 3）
  → 仅 Python → reload_python_tool（Tier 2）
```

多处 C++ 变更合并为**一次** full_rebuild。

### 原则 3：会话缓存复用

Step 0 缓存后全程复用，不重复查询：TCP 协议字段 `"type"`、可用命令集、项目路径 / UBT target 名。

### 原则 4：批处理优先

独立只读查询必须合并为 `batch`：
```python
send_cmd("batch", {"commands": [
    {"type": "get_current_level_name", "params": {}},
    {"type": "get_world_settings",     "params": {}},
    {"type": "get_actors_in_level",    "params": {}}
]})
```

---

## 开发规范

### 新增 C++ 命令
见 `References/SOP/Plugin_Dev_SOP.md`。错误响应：`FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("..."))`

### 新增 Python 工具
创建 `Python/tools/xxx_tools.py`，实现 `register_xxx_tools(mcp: FastMCP)`。无需修改服务器入口。

### UE4/UE5 兼容
样式宏：`MCP_STYLE_NAME`；Enhanced Input 宏：`#if MCP_ENHANCED_INPUT_SUPPORTED`；完整 API 差异见 `References/Notes/UE_Compat.md`。

---

## Hook 开发规范

**三条铁律**：
1. **stdout 只能是 JSON** — 调试信息用 `sys.stderr`
2. **Stop hook 必须检查 `stop_hook_active`** — 为 `true` 时直接 `sys.exit(0)`
3. **Windows 含空格路径必须加引号** — `"python \"<path>/script.py\""`

---

## Python MCP 工具编写规范

```python
# ✅ 正确
@mcp.tool()
def my_tool(ctx: Context, name: str, count: int = 0) -> Dict[str, Any]:
    """Do something. Args: name: Actor name (e.g. 'Cube_01'). count: repeat count."""
    if count == 0: count = 1  # handle default in body

# ❌ 禁止：Optional[T]、Union[T]、Any 参数类型
```

---

## Python 依赖

```
mcp[cli]          # 必需
psutil>=5.9.0     # 可选：进程检测
Pillow>=10.0.0    # 可选：截图处理
```

---

## 实现进度

| 阶段 | 状态 | 内容 |
|------|------|------|
| 架构重构 | ✅ | FMCPCommandRegistry、auto_register_tools |
| Phase 1 | ✅ | 关卡/资产/DataTable/Actor/WorldSettings/BP/节点/UMG/Enhanced Input |
| Phase 2A | ✅ | DiagnosticsCommands + log/diagnostics/compile/system Python 工具 |
| Phase 2B | ✅ | TestCommands（validate_blueprint / run_level_validation） |
| Phase 2C 材质 | ✅ | MaterialCommands（6条命令，实战验证）|
| Phase 2D | ⏳ | 启动控制（见 `References/Plans/Phase2D_启动控制.md`） |
| 可移植性 | ✅ | install.py / UnrealMCPCompat.h / UE4/5 兼容 / 编辑器菜单开关 |
| UE4.24 验证 | ✅ | 自编译项目成功编译 |

---

## 知识库索引

| 需要查询 | 位置 |
|---------|------|
| 命令全表（117条）| `References/Architecture/Commands.md` |
| 已知问题与解决方案 | `References/Notes/Known_Issues.md` |
| UE4/UE5 兼容 API 差异 | `References/Notes/UE_Compat.md` |
| 任务执行流程 | `References/SOP/Main_SOP.md` |
| Git 操作规范 | `References/SOP/Git_SOP.md` |
| 新增命令/full_rebuild | `References/SOP/Plugin_Dev_SOP.md` |
| 插件安装到新项目 | `References/SOP/Install_SOP.md` |
| 未完成计划 | `References/Plans/Phase2D_启动控制.md` |
| Lyra AnimBP 路径 | `memory/MEMORY.md` |
