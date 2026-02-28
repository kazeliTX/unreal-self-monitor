# UnrealMCP 自愈与诊断系统规划 v2.0

> **定位**：在 Phase 1（~100 个命令）基础上，赋予 AI「感知 → 定位 → 修复 → 验证」闭环能力。
> **创建**：2026-02-27
> **目标版本**：Phase 2（全部完成后约 158 个命令 + 41 个 Python 工具）

---

## 一、背景与核心目标

### 1.1 痛点

在长线自动化流程中，AI 通过 UnrealMCP 控制编辑器时，以下问题会反复出现：

| 问题类型 | 典型场景 | 当前能力 |
|---------|---------|---------|
| 命令执行失败 | Blueprint 编译错误、资产路径错误 | ❌ 只能返回错误字符串，无上下文 |
| 视觉效果偏差 | 摆放位置/材质/灯光与预期不符 | ❌ 截图存在但 AI 无法直接读取 |
| 代码 Bug | C++ 插件/Python 工具有缺陷 | ❌ 需手动修改文件后重启 |
| 编辑器崩溃 | 复杂操作后 UE 进程退出 | ❌ 需人工重启并等待 |
| 服务状态未知 | 不知道 MCP/Editor 当前是否就绪 | ❌ 只能在命令失败后才知道 |

### 1.2 五大能力模块

```
┌─────────────────────────────────────────────────────────────┐
│                   UnrealMCP 自愈系统                         │
│                                                             │
│  M1 日志分析     M2 视觉感知     M3 自动化测试              │
│  ──────────     ──────────     ──────────                   │
│  读取/压缩/      Viewport        创建/执行                   │
│  分析 UE 日志    截图分析        Debug 脚本                  │
│                                                             │
│  M4 编译管理                  M5 启动状态管理               │
│  ──────────                  ──────────────                 │
│  修改代码 +                   编辑器/MCP                    │
│  触发热重载/UBT               启动检测与控制                 │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、系统架构

### 2.1 总体数据流

```
AI Client (Claude / Cursor)
    │
    │  MCP Protocol (stdio)
    ▼
Python MCP Server  ─────────────── 文件系统直接读写
    │  tools/log_tools.py           (日志文件、源码文件、截图)
    │  tools/diagnostics_tools.py
    │  tools/compile_tools.py
    │  tools/system_tools.py
    │  scripts/compile_watch.py
    │
    │  TCP 127.0.0.1:55557
    ▼
UE Editor Plugin (C++)
    │  UnrealMCPDiagnosticsCommands    ← M1/M2/M4 热重载
    │  UnrealMCPTestCommands           ← M3 自动化测试
    │
    ├── UE 日志系统 (GLog)
    ├── Viewport / Screenshot
    ├── UE Automation Framework
    └── Live Coding Module
```

### 2.2 新增文件清单

**C++ 新文件（2个）：**
```
Public/Commands/UnrealMCPDiagnosticsCommands.h
Private/Commands/UnrealMCPDiagnosticsCommands.cpp
Public/Commands/UnrealMCPTestCommands.h
Private/Commands/UnrealMCPTestCommands.cpp
```

**Python 新文件（4个工具 + 2个脚本）：**
```
Python/tools/log_tools.py           # M1: 日志分析（直接读文件）
Python/tools/diagnostics_tools.py   # M2: 视觉感知 + 综合诊断
Python/tools/compile_tools.py       # M4: 编译管理 + 代码修改
Python/tools/system_tools.py        # M5: 启动状态管理
Python/scripts/compile_watch.py     # UBT 编译状态监控
Python/scripts/debug_runner.py      # 调试脚本批量执行器
```

**Build.cs 新增依赖：**
```csharp
"LiveCoding",            // M4: 热重载
"AutomationController",  // M3: 自动化测试框架
```

---

## 三、模块详细设计

---

### M1 — 日志分析系统

#### 3.1.1 UE 日志格式解析

```
[2024.01.15-10:30:01:456][  1]LogBlueprint: Error: Pin type mismatch
[2024.01.15-10:30:02:789][  2]LogUnrealMCP: Display: Command executed: compile_blueprint
格式：[时间戳][帧号]类别: 级别: 消息
```

日志路径优先级：
1. `{ProjectDir}/Saved/Logs/{ProjectName}.log`（如 MCPGameProject.log）
2. `{ProjectDir}/Saved/Logs/UnrealEditor.log`

> **设计决策：** M1 完全在 Python 端实现（直接读文件），不经过 TCP 通道，原因：
> - 编辑器崩溃时 TCP 不可用，但日志文件仍然可读
> - 避免增加 C++ 代码量
> - 大量日志传输不适合 TCP 消息体

#### 3.1.2 Python 工具（log_tools.py）

| 函数 | 参数 | 说明 |
|------|------|------|
| `read_ue_log` | `lines=100, filter_level="All", filter_category=None, since_seconds=None` | 读取并解析日志末尾 N 行 |
| `get_log_errors` | `lines=500` | 只返回 Error 级别条目列表 |
| `get_log_warnings` | `lines=500` | 只返回 Warning 级别条目列表 |
| `get_log_summary` | `lines=500` | AI 友好摘要：错误数/警告数/关键错误列表 |
| `search_log` | `keyword, lines=1000` | 关键词全文搜索 |
| `get_mcp_activity_log` | `lines=200` | 仅过滤 LogUnrealMCP 类别 |
| `set_log_path` | `path` | 手动覆盖日志路径（用于非标准安装） |

**get_log_summary 返回格式（AI 友好）：**
```json
{
  "log_path": "/path/to/MCPGameProject.log",
  "scan_range_lines": 500,
  "error_count": 3,
  "warning_count": 12,
  "errors": [
    "LogBlueprint: Pin type mismatch on node 'Add'",
    "LogUnrealMCP: Failed to find Blueprint 'MyBP'"
  ],
  "warnings": ["..."],
  "mcp_commands_executed": ["compile_blueprint", "create_blueprint"],
  "last_timestamp": "2024.01.15-10:35:00:000"
}
```

#### 3.1.3 关键实现细节

```python
# 高效读取日志尾部（避免加载大文件）
def tail_log_file(path: str, n: int) -> list[str]:
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        # 使用 deque 保证 O(1) 空间
        from collections import deque
        return list(deque(f, maxlen=n))

# 日志条目解析正则
LOG_PATTERN = re.compile(
    r'\[(\d{4}\.\d{2}\.\d{2}-\d{2}:\d{2}:\d{2}:\d{3})\]\[\s*(\d+)\]'
    r'(\w+(?:\.\w+)*): (?:(\w+): )?(.*)'
)
```

---

### M2 — 视觉感知系统

#### 3.2.1 已有基础

Phase 1 已有 `take_screenshot`（返回文件路径），但 AI 无法直接读取图像内容。

#### 3.2.2 C++ 新命令（UnrealMCPDiagnosticsCommands）

**take_viewport_screenshot**（强化版）
```
Params:
  filename: string         可选，文件名（不含扩展名）
  resolution: [W, H]       可选，默认 1920×1080
  show_ui: bool            可选，默认 false
Returns:
  { file_path, width, height, timestamp, file_size_bytes }
```

**get_viewport_camera_info**
```
Returns:
  { location:[x,y,z], rotation:[p,y,r], fov, is_perspective,
    ortho_width, near_clip, far_clip }
```

**get_actor_screen_position**
```
Params: name (actor name or label)
Returns:
  { screen_x, screen_y, is_on_screen, is_occluded,
    world_location:[x,y,z], distance_to_camera }
```

**highlight_actor**（调试辅助，临时高亮）
```
Params:
  name: string
  color: [R,G,B,A]    可选，默认 [1,0,0,1]（红色）
  duration: float      可选，默认 5.0 秒
Returns: { success, actor_name }
```

#### 3.2.3 Python 工具（diagnostics_tools.py）

| 函数 | 说明 |
|------|------|
| `take_and_read_screenshot(filename=None)` | 截图 + 读取为 base64，供 AI 多模态分析 |
| `capture_actor_focused(actor_name, margin=1.2)` | 自动聚焦 Actor → 截图 → 返回 base64 |
| `get_scene_overview()` | Actor 列表 + 截图 + 相机信息的综合快照 |
| `get_actor_visual_info(actor_name)` | 屏幕坐标 + 高亮 + 截图 三合一 |
| `compare_screenshots(path1, path2)` | 像素差分（需 Pillow），返回差异率 |
| `get_latest_screenshot_path()` | 获取最新截图路径 |

**take_and_read_screenshot 内部流程：**
```
1. send_unreal_command("take_viewport_screenshot", {...})
2. 从返回的 file_path 读取图像文件
3. 压缩/缩放（可选，降低 AI token 消耗）
4. Base64 编码后返回
```

**AI 使用模式：**
```python
result = take_and_read_screenshot()
# result["image_base64"] 可直接传给多模态 AI 分析
# result["file_path"] 用于后续 compare_screenshots
```

---

### M3 — 自动化测试系统

#### 3.3.1 三层测试架构

```
Layer 3: UE Automation Framework（run_automation_test）
  └── 注册的 C++ / Python 测试用例，批量执行，XML 报告

Layer 2: Blueprint 结构验证（validate_blueprint / run_level_validation）
  └── 编译状态 + 节点连接完整性 + 资产引用有效性

Layer 1: MCP 连接 / 命令快速冒烟测试（Python 端，无需 UE）
  └── TCP 连通性 + ping/pong + 基本命令响应格式
```

#### 3.3.2 C++ 新命令（UnrealMCPTestCommands）

**validate_blueprint**
```
Params: blueprint_name, path (可选)
Returns:
  { is_valid, compile_status, error_count, warning_count,
    errors: [string], warnings: [string], node_count, variable_count }
```

**run_level_validation**
```
Params: 无
Returns:
  { total_actors, actors_with_issues: [{name, issue_type, detail}],
    broken_asset_refs: [string], uncompiled_blueprints: [string],
    orphaned_components: [string] }
```

**run_automation_test**
```
Params:
  test_filter: string    测试名过滤（支持通配符，如 "UnrealMCP.*"）
  timeout: float         默认 60
Returns:
  { total, passed, failed, skipped, duration_seconds,
    results: [{name, status, errors: [string], duration}] }
```

**get_automation_test_list**
```
Returns:
  { tests: [{name, full_path, flags, estimated_duration}] }
```

**execute_python_in_ue**（依赖 UE Python 插件）
```
Params:
  script: string        Python 代码字符串（互斥 script_path）
  script_path: string   项目相对路径（互斥 script）
Returns:
  { success, output: string, error: string, execution_time_ms }
```

#### 3.3.3 Python 工具（diagnostics_tools.py 扩展 + scripts/debug_runner.py）

| 函数 | 说明 |
|------|------|
| `run_mcp_connection_test()` | Layer 1：TCP + ping + get_capabilities |
| `run_blueprint_smoke_test(bp_name)` | Layer 2：创建→编译→验证→清理 |
| `run_level_health_check()` | Layer 2：run_level_validation + 日志检查 |
| `run_full_diagnostic()` | 1+2 层综合，返回结构化诊断报告 |
| `generate_test_report(results)` | 生成 Markdown 格式测试报告 |

**run_full_diagnostic 返回格式：**
```json
{
  "timestamp": "2024-01-15T10:30:00",
  "overall_status": "WARNING",
  "checks": {
    "tcp_connection": { "status": "OK", "latency_ms": 12 },
    "editor_process": { "status": "OK", "pid": 12345 },
    "level_loaded": { "status": "OK", "level": "/Game/Maps/TestLevel" },
    "log_errors": { "status": "WARNING", "count": 2, "errors": ["..."] },
    "blueprint_health": { "status": "OK", "checked": 5, "failed": 0 }
  },
  "recommendations": ["修复 BlueprintA 的编译错误", "检查 Mesh 资产路径"]
}
```

---

### M4 — 编译管理系统

#### 3.4.1 编译方式层次与选型

```
方式 A: Blueprint 编译（已有 compile_blueprint）
  速度：即时（<1s）
  适用：Blueprint 图逻辑修改

方式 B: Python 工具热重载（无需重启 MCP）
  速度：即时（importlib.reload）
  适用：修改 Python .py 工具文件

方式 C: UE Live Coding / Hot Reload（~5-60秒）
  速度：快
  适用：C++ 函数体修改，不涉及类结构/头文件变化

方式 D: UnrealBuildTool 完整编译（~2-10分钟）
  速度：慢，需重启编辑器
  适用：头文件变更、新增类/模块
```

#### 3.4.2 C++ 新命令（UnrealMCPDiagnosticsCommands）

**get_source_file**
```
Params:
  file_path: string    相对于项目根目录（如 "Plugins/UnrealMCP/Source/.../MyFile.cpp"）
Returns:
  { content, absolute_path, line_count, last_modified_utc, file_size_bytes }
```

**modify_source_file**（带自动备份）
```
Params:
  file_path: string    相对于项目根目录
  content: string      新文件完整内容
  create_backup: bool  默认 true（备份到 file_path.bak.{timestamp}）
Returns:
  { success, file_path, backup_path, bytes_written }
```

**trigger_hot_reload**
```
Params:
  wait_seconds: float   等待时间（0=触发后立即返回，默认0）
Returns:
  { triggered, note }
备注：触发后通过 get_live_coding_status 轮询状态
```

**get_live_coding_status**
```
Returns:
  { is_live_coding_enabled, is_compiling, last_result: "Success"|"Failed"|"None",
    pending_changes_count, last_compile_duration_seconds }
```

#### 3.4.3 Python 工具（compile_tools.py）

**文件读写（直接操作文件系统）：**

| 函数 | 说明 |
|------|------|
| `read_cpp_file(relative_path)` | 读取 C++ 源文件内容 |
| `write_cpp_file(relative_path, content, backup=True)` | 写入 C++ 文件（自动备份） |
| `read_python_tool(tool_name)` | 读取 `Python/tools/{tool_name}.py` |
| `write_python_tool(tool_name, content, backup=True)` | 写入 Python 工具文件 |
| `list_source_files(pattern="*.cpp")` | 列出源文件（支持 glob） |
| `get_file_diff(path, new_content)` | 返回 unified diff（修改前预览） |

**编译控制：**

| 函数 | 说明 |
|------|------|
| `trigger_hot_reload()` | 触发 Live Coding（方式 C） |
| `wait_for_hot_reload(timeout=60)` | 轮询 get_live_coding_status 直到完成 |
| `reload_python_tool(tool_name)` | importlib 热重载 Python 工具（方式 B） |
| `run_ubt_build(target="Development")` | 调用 Build.bat（方式 D） |
| `get_build_log(last_n_lines=100)` | 读取 UBT 输出日志 |
| `get_engine_path()` | 自动探测 UE 引擎路径 |
| `get_project_path()` | 自动探测 .uproject 路径 |

**run_ubt_build 内部调用（Windows）：**
```python
# {EngineDir}/Build/BatchFiles/Build.bat {ProjectName} Win64 Development {ProjectPath} -WaitMutex
cmd = [
    str(engine_path / "Build/BatchFiles/Build.bat"),
    project_name, "Win64", target, str(project_uproject),
    "-WaitMutex", "-FromMsBuild"
]
result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
```

**Python 工具修改+热重载工作流：**
```python
def patch_python_tool(tool_name: str, new_content: str) -> dict:
    """修改 Python 工具文件并立即热重载，无需重启 MCP Server"""
    write_python_tool(tool_name, new_content)     # 写文件
    return reload_python_tool(tool_name)           # importlib.reload
```

---

### M5 — 启动状态管理系统

#### 3.5.1 状态检测矩阵

| 检测项 | 检测方法 | 就绪判断 |
|--------|---------|---------|
| UE 编辑器进程 | `psutil.process_iter(['name'])` | `UnrealEditor.exe` 进程存在 |
| UE TCP 服务端口 | `socket.create_connection(("127.0.0.1", 55557), 2)` | 连接成功 |
| MCP 服务就绪 | TCP + 发送 ping | 返回 `{result: {message: "pong"}}` |
| 编辑器完全加载 | ping + get_current_level_name | 有效关卡名（非空字符串） |
| Python MCP Server | `psutil` 进程名 + PID 文件 | `python unreal_mcp_server.py` 运行中 |

#### 3.5.2 Python 工具（system_tools.py）

| 函数 | 说明 |
|------|------|
| `check_editor_status()` | 综合检测编辑器状态（进程+TCP+关卡） |
| `check_mcp_server_status()` | 检测 Python MCP Server 进程状态 |
| `check_full_system_status()` | 所有检测项的完整报告 |
| `launch_editor(project_path=None, extra_args=[])` | 启动 UE 编辑器（subprocess Popen） |
| `wait_for_editor_ready(timeout=120, poll_interval=5)` | 轮询等待 TCP 就绪 + ping 确认 |
| `restart_editor(project_path=None)` | 终止旧进程 → launch → wait |
| `start_mcp_server(server_script=None)` | 启动 Python MCP 服务（subprocess） |
| `kill_editor()` | 强制终止 UE 编辑器进程 |

**check_editor_status 返回格式：**
```json
{
  "process_running": true,
  "process_pid": 12345,
  "tcp_port_open": true,
  "mcp_ping_ok": true,
  "level_loaded": true,
  "current_level": "/Game/Maps/TestLevel",
  "overall": "READY"
}
```

**overall 状态枚举：**
- `READY`：所有检测通过，可以发送命令
- `LOADING`：进程运行 + TCP 通但关卡未加载
- `STARTING`：进程存在但 TCP 尚未开放
- `NOT_RUNNING`：进程不存在
- `ERROR`：检测过程异常

#### 3.5.3 编辑器自动启动完整流程

```
check_full_system_status()
    │
    ├── overall == "READY" → 直接使用
    │
    ├── overall == "NOT_RUNNING" →
    │       launch_editor(project_path)
    │           ├── 查找 UnrealEditor.exe 路径
    │           │   （读取 .uproject 的 EngineAssociation → 注册表查找引擎路径）
    │           ├── subprocess.Popen([editor_exe, uproject_path])
    │           └── 记录 PID 到 .mcp_editor.pid
    │       wait_for_editor_ready(timeout=180)
    │           ├── 每 5 秒 socket.create_connection(55557)
    │           ├── 连接成功 → 发送 ping
    │           └── pong 返回 → 继续 wait level load
    │       返回最终状态
    │
    └── overall == "LOADING" →
            wait_for_editor_ready(timeout=60)  # 只等关卡加载
```

#### 3.5.4 UE 引擎路径自动探测

```python
def get_engine_path() -> str:
    """
    策略（Windows）：
    1. 读取 .uproject 中的 EngineAssociation（如 "5.5"）
    2. 查询注册表 HKEY_LOCAL_MACHINE\SOFTWARE\EpicGames\Unreal Engine\{ver}\InstalledDirectory
    3. 备用：搜索常见路径 C:\Program Files\Epic Games\UE_{ver}\
    """
```

---

## 四、自愈工作流示例

### 场景 A：Blueprint 编译错误自愈

```
1. create_blueprint("MyBP", "Actor")
2. 添加节点 → compile_blueprint("MyBP")
   → 返回失败

3. get_blueprint_compile_errors("MyBP")
   → "Pin type mismatch on node Add"

4. get_log_summary(lines=200)
   → 更多 LogBlueprint 上下文

5. AI 分析错误 → 修正节点连接参数
6. compile_blueprint("MyBP")
   → 成功 ✓
```

### 场景 B：C++ 插件修改 + 热重载

```
1. get_source_file("Plugins/UnrealMCP/.../UnrealMCPEditorCommands.cpp")
   → 获取当前代码

2. AI 分析代码，定位 Bug，生成新代码
3. modify_source_file(path, new_content)
   → 自动备份 + 写入

4. trigger_hot_reload()
5. 每 10 秒 get_live_coding_status()，直到 last_result == "Success"

6. get_log_summary(lines=50, filter_category="LogLiveCoding")
   → 确认无编译错误

7. 测试修复后的命令 ✓
```

### 场景 C：编辑器崩溃自动恢复

```
1. 发送某命令 → TCP 连接拒绝

2. check_editor_status()
   → { overall: "NOT_RUNNING" }

3. get_log_errors(lines=100)
   → 分析崩溃前最后错误（如访问空指针）

4. launch_editor(project_path="/Game/MCPGameProject.uproject")
   → PID: 99999

5. wait_for_editor_ready(timeout=180)
   → waited_seconds: 47, ready: true

6. check_editor_status()
   → { overall: "READY", level: "/Game/Maps/TestLevel" }

7. 继续之前的任务 ✓
```

### 场景 D：视觉验证与修正

```
1. 创建场景，spawn_actor("Cube01", "StaticMeshActor", [0,0,0])
2. focus_viewport(target="Cube01")
3. take_and_read_screenshot()
   → { image_base64: "...", file_path: "..." }

4. AI 多模态分析截图
   → "Cube 位置偏左，应在画面中心"

5. get_actor_screen_position("Cube01")
   → { screen_x: 780, screen_y: 540 }  (viewport 1920×1080，期望 960, 540)

6. set_actor_transform("Cube01", location=[180, 0, 0])
   → 修正位置

7. take_and_read_screenshot()
   → AI 确认位置正确 ✓
```

### 场景 E：Python 工具 Bug 修复

```
1. 发现 add_image_to_widget 参数传递有误
2. read_python_tool("umg_tools")
   → 获取完整 Python 代码

3. AI 定位 Bug（texture 参数未传递）
4. AI 生成修复后代码
5. write_python_tool("umg_tools", fixed_content)
   → 自动备份原文件

6. reload_python_tool("umg_tools")
   → importlib.reload，立即生效（无需重启 MCP）

7. 测试 add_image_to_widget ✓
```

### 场景 F：完整 UBT 重编译（头文件变更后）

```
1. AI 需要在 .h 头文件中新增公共方法
2. read_cpp_file("Plugins/UnrealMCP/.../UnrealMCPEditorCommands.h")
3. modify_source_file(h_path, new_header_content)
4. modify_source_file(cpp_path, new_impl_content)

5. check_editor_status()
   → { overall: "READY" }

6. run_ubt_build(target="Development")
   → 后台运行 Build.bat（subprocess，timeout=600）

7. get_build_log(last_n_lines=50)
   → 检查编译输出

8. 若成功：重启编辑器
   restart_editor(project_path)
   wait_for_editor_ready(timeout=180)

9. 测试新功能 ✓
```

---

## 五、新增命令汇总

### C++ 命令（UnrealMCPDiagnosticsCommands）

| 命令 | 模块 | 优先级 |
|------|------|--------|
| `take_viewport_screenshot` | M2 | P0 |
| `get_viewport_camera_info` | M2 | P0 |
| `get_actor_screen_position` | M2 | P0 |
| `highlight_actor` | M2 | P1 |
| `trigger_hot_reload` | M4 | P0 |
| `get_live_coding_status` | M4 | P0 |
| `get_source_file` | M4 | P0 |
| `modify_source_file` | M4 | P0 |

### C++ 命令（UnrealMCPTestCommands）

| 命令 | 模块 | 优先级 |
|------|------|--------|
| `validate_blueprint` | M3 | P0 |
| `run_level_validation` | M3 | P0 |
| `execute_python_in_ue` | M3 | P1 |
| `run_automation_test` | M3 | P2 |
| `get_automation_test_list` | M3 | P2 |

**新增 C++ 命令：13 个**

### Python 工具函数

| 文件 | 函数数 | 模块 |
|------|--------|------|
| `log_tools.py` | 7 | M1 |
| `diagnostics_tools.py` | 10 | M2 + M3 |
| `compile_tools.py` | 10 | M4 |
| `system_tools.py` | 8 | M5 |

**新增 Python 工具：35 个**

---

## 六、实现计划

### Phase 2A — 基础感知（P0，预计 2天）

**目标**：AI 能读取日志 + 截图，了解当前状态

- [ ] `log_tools.py`（纯文件读取，无 C++ 依赖）
- [ ] `system_tools.py`（check_editor_status, launch_editor, wait_for_editor_ready）
- [ ] C++: `take_viewport_screenshot`（强化版）、`get_viewport_camera_info`
- [ ] Python: `diagnostics_tools.py` 基础版（take_and_read_screenshot）

**验收**：AI 能执行「读日志 → 识别错误 → 截图查看 → 描述当前场景状态」

---

### Phase 2B — 编译管理（P0，预计 2天）

**目标**：AI 能修改代码并触发重编译

- [ ] `compile_tools.py`（read/write_cpp_file, read/write_python_tool, reload_python_tool, run_ubt_build）
- [ ] C++: `get_source_file`, `modify_source_file`
- [ ] C++: `trigger_hot_reload`, `get_live_coding_status`
- [ ] `scripts/compile_watch.py`（轮询编译状态的独立脚本）

**验收**：AI 能执行「读 C++ 文件 → 修改 → 热重载 → 确认成功 → 测试新功能」

---

### Phase 2C — 测试与验证（P1，预计 2天）

**目标**：AI 能创建测试并获取结构化报告

- [x] C++: `validate_blueprint`, `run_level_validation`
- [ ] C++: `execute_python_in_ue`
- [ ] Python: `diagnostics_tools.py` 测试部分（run_full_diagnostic, generate_test_report）
- [ ] `scripts/debug_runner.py`

**验收**：AI 能执行「创建 BP → 验证 → 运行全诊断 → 生成报告 → 修复问题」

---

### Phase 2D — 启动控制（P1，预计 1天）

**目标**：AI 能自动启动/重启编辑器

- [ ] `system_tools.py` 完整实现（launch_editor, restart_editor, start_mcp_server）
- [ ] 引擎路径自动探测（注册表 + 常见路径）
- [ ] PID 文件管理
- [ ] Phase 2C 自动化测试覆盖整个重启流程

**验收**：AI 能执行「检测崩溃 → 分析崩溃日志 → 重启编辑器 → 等待就绪 → 继续任务」

---

## 七、依赖与风险

### 7.1 新增 Python 依赖（requirements.txt）

```
psutil>=5.9.0        # M5: 进程检测（is_editor_running）
Pillow>=10.0.0       # M2: 图像处理（compare_screenshots 像素差分）
```

### 7.2 新增 Build.cs 依赖

```csharp
// Phase 2B
"LiveCoding",              // trigger_hot_reload / get_live_coding_status
// Phase 2C（可选）
"AutomationController",    // run_automation_test（UE 自动化框架）
```

### 7.3 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| `modify_source_file` 写入错误代码导致编译失败 | 中 | 高 | 自动备份 `.bak.{timestamp}` + 编译失败时 AI 可恢复备份 |
| Live Coding 失败导致编辑器不稳定 | 中 | 中 | 失败时自动降级：提示改用 run_ubt_build + restart_editor |
| 截图 Base64 体积过大占用 token | 中 | 低 | 默认 720p 压缩；AI 可指定 `max_width` 参数 |
| 编辑器启动超时（首次 shader 编译） | 高 | 低 | wait_for_editor_ready 默认 timeout=180，可配置 |
| UE Python 插件未启用（execute_python_in_ue） | 中 | 低 | 优雅降级：返回说明如何启用插件的指引 |
| psutil 未安装 | 低 | 低 | try/except 降级：仅 TCP 检测，不做进程检测 |

---

## 八、命令总数统计

| 阶段 | C++ 命令 | Python 工具 | 累计命令 |
|------|---------|------------|---------|
| Phase 1（已实现） | ~65 | ~35 | ~100 |
| Phase 2A | +4 | +15 | ~119 |
| Phase 2B | +4 | +10 | ~133 |
| Phase 2C | +5 | +8 | ~146 |
| Phase 2D | +0 | +2 | ~148 |
| **Phase 2 合计** | **+13** | **+35** | **~148** |

---

## 九、目录结构（Phase 2 完成后）

```
unreal-mcp-main/
├── MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/
│   ├── Public/Commands/
│   │   ├── UnrealMCPDiagnosticsCommands.h    ← Phase 2 新增
│   │   └── UnrealMCPTestCommands.h           ← Phase 2 新增
│   └── Private/Commands/
│       ├── UnrealMCPDiagnosticsCommands.cpp  ← Phase 2 新增
│       └── UnrealMCPTestCommands.cpp         ← Phase 2 新增
│
├── Python/
│   ├── tools/
│   │   ├── base.py
│   │   ├── editor_tools.py          ← Phase 1 扩展完成
│   │   ├── blueprint_tools.py       ← Phase 1 扩展完成
│   │   ├── node_tools.py            ← Phase 1 扩展完成
│   │   ├── umg_tools.py             ← Phase 1 扩展完成
│   │   ├── project_tools.py         ← Phase 1 扩展完成
│   │   ├── level_tools.py           ← Phase 1 新增
│   │   ├── asset_tools.py           ← Phase 1 新增
│   │   ├── log_tools.py             ← Phase 2A 新增
│   │   ├── diagnostics_tools.py     ← Phase 2A/C 新增
│   │   ├── compile_tools.py         ← Phase 2B 新增
│   │   └── system_tools.py          ← Phase 2D 新增
│   └── scripts/
│       ├── compile_watch.py         ← Phase 2B 新增
│       └── debug_runner.py          ← Phase 2C 新增
│
├── 虚幻AI计划.md                    ← Phase 1 规划
└── 虚幻AI2_0.md                    ← 本文档（Phase 2 规划）
```

---

*文档版本：v2.0*
*创建日期：2026-02-27*
*基于代码库：MCPGameProject/Plugins/UnrealMCP（Phase 1 ~100命令基础上）*
