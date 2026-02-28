# UnrealMCP AI 自动化标准操作流程（SOP）

> 适用于所有 UE 工程。Claude 在每次执行自动化任务时须遵循本流程。

---

## 流程总览

```
用户提出需求
    │
    ▼
⓪ 会话初始化  →  验证协议 + 缓存命令集 + 记录编辑器状态
    │
    ▼
① /ue-plan  →  询问是否启用 Git 管控
    │           ├─ 是 → 读取 git log → checkout 新分支
    │           └─ 否 → 跳过
    │           ▼
    │       能力预检（直接需求 + 发散预测）
    │           ▼
    │       缺口汇总 → 产出 Plan 文档，用户确认后继续
    ▼
② 统一实现缺失能力（若有）
    ├─ Python 变更 → reload_python_tool（Tier 2）
    └─ C++ 变更   → 一次 full_rebuild（Tier 4）
         ※ 不分多轮，合并为单次构建
    │
    ▼
③ 执行任务（批处理优先）
    │   优先合并 MCP 查询为 batch；遵循会话缓存，不重复试错
    ▼
④ 问题沉淀（遇到问题时）→ /ue-note → 写入 Notes
    │
    ▼
⑤ 收尾
    ├─ 更新 CLAUDE.md
    └─ Git 已启用 → git commit（含本次所有变更）
```

---

## Step 0：会话初始化（Session Init）

> **每次会话开始时必须执行，结果在整个会话中复用，不重复查询。**

### 0-A. 快速状态探针

```python
# 单次 batch 调用，一次获取所有环境信息
send_cmd("batch", {"commands": [
    {"type": "ping", "params": {}},
    {"type": "get_capabilities", "params": {}},
    {"type": "get_current_level_name", "params": {}}
]})
```

| 检查项 | 缓存内容 |
|--------|---------|
| ping → pong | TCP 协议字段确认为 `"type"`（非 `"command"`） |
| get_capabilities | 可用命令集（对比 CLAUDE.md 判断缺口） |
| get_current_level_name | 编辑器当前状态（是否已就绪） |

### 0-B. 协议常量（会话内固定，不再探测）

```
TCP 字段名：  "type"（不是 "command"）
响应格式：    {"status": "success"/"error", "result": {...}}
连接模式：    每命令独立连接（server 在 recv 后关闭）
端口：        55557
```

### 0-C. 编辑器未运行时的启动流程

若 ping 失败，在启动编辑器**之前**完成所有 C++ 代码变更和编译，再启动。
**严禁「先启动 → 发现缺命令 → 关闭 → 重启」的反模式。**

---

## Step 1：需求分析、Git 确认 & Plan 文档

### 1-A. Git 管控确认

询问用户：**「本次流程是否启用自动 Git 管控？」**

| 用户选择 | 操作 |
|---------|------|
| **是** | 执行下方 Git 初始化流程 |
| **否** | 跳过，直接进入 1-B |

**Git 初始化流程**：
```bash
git log --oneline -10    # 了解上下文
git status               # 确认工作区干净
git checkout -b ue/YYYY-MM-DD-task-name
```

### 1-B. 能力预检 & 发散预测（必须在 Plan 确认前完成）

#### 直接需求分析
列出完成本次任务**直接需要**的所有 MCP 命令，与 Step 0 缓存的命令集对比，找出缺口。

#### 发散预测（Forward Projection）
围绕用户目标，预测**接下来 1-2 步可能需要**的相关能力，一并纳入缺口评估：

| 用户需求示例 | 发散预测的相关能力 |
|------------|-----------------|
| 打开动画蓝图 | 创建蒙太奇、添加动画通知、修改动画曲线、重定向 |
| 创建角色 BP | 添加 SkeletalMesh 组件、绑定输入、设置碰撞、添加动画 |
| 创建关卡 | 添加光照、大气、Fog、NavMesh、碰撞检测 |
| 创建 Widget | 绑定事件、数据绑定、动画、多语言文本 |

**原则**：宁可多实现一个不用的命令，也不要在执行中途因缺命令而重启编辑器。

#### 缺口汇总表（写入 Plan 文档）
```markdown
## 能力缺口
| 命令/工具 | 状态 | 实现位置 |
|---------|------|---------|
| open_asset_editor | ❌ 缺失 | AssetCommands.cpp |
| create_montage    | ❓ 待确认 | 可能需要新增 |
| xxx               | ✅ 已有 | - |
```

### 1-C. 生成 Plan 文档

在 `References/Plans/` 创建：`YYYY-MM-DD_任务名称.md`

**Plan 文档模板**：
```markdown
# 计划：[任务名称]

**日期**：YYYY-MM-DD
**目标**：[1-2句描述]
**Git 分支**：ue/YYYY-MM-DD-task-name（或「未启用」）

## 涉及模块
- C++ 命令：[列举]
- Python 工具：[列举]

## 能力缺口（预检结果）
| 命令/工具 | 状态 | 计划 |
|---------|------|------|
| xxx | ❌ 缺失 | 本次实现 |
| yyy | ❓ 发散预测 | 本次实现（防止中途中断） |

## 执行步骤
- [ ] Step 0：会话初始化 & 能力预检
- [ ] Step 1：统一实现所有缺失命令（若有）→ 一次 full_rebuild
- [ ] Step 2：...（具体任务步骤）

## 验收标准
- [ ] ...

## 风险点
- ...
```

向用户展示计划并确认后再开始执行。

---

## Step 2：统一实现缺失能力

> **铁律：所有缺失命令必须在任务开始执行前全部实现，不允许在执行中途发现缺口后再回头编译。**

### 编译层级决策（快速判定，不逐级评估）

```
变更涉及以下任意一项？
  ├─ 新增 RegisterCommands 注册条目
  ├─ .h 头文件变更
  ├─ 新增类/模块
  └─ Build.cs 变更
      ↓ 是 → 直接 Tier 4（full_rebuild）
              跳过 Tier 1/2/3 评估，不需要考虑 LiveCoding

  仅修改已有函数体（无头文件/注册变更）？
      ↓ 是 → Tier 3（trigger_hot_reload）

  仅 Python 工具变更？
      ↓ 是 → Tier 2（reload_python_tool）

  仅蓝图逻辑？
      ↓ 是 → Tier 1（compile_blueprint）
```

**关键原则**：对于本项目，新增 MCP 命令 ≈ 必然涉及 RegisterCommands 注册，**默认走 Tier 4**，不需要逐级分析。

### 多处变更合并为单次构建

```
❌ 错误：实现 A → 编译 → 测试 → 实现 B → 编译 → 测试
✅ 正确：实现 A + B + C → 一次 full_rebuild → 统一测试
```

### full_rebuild 标准流程

```bash
# 1. 关闭编辑器（PowerShell，bash 下 taskkill 无效）
powershell -Command "Stop-Process -Name UnrealEditor -Force -ErrorAction SilentlyContinue; Stop-Process -Name LiveCodingConsole -Force -ErrorAction SilentlyContinue"

# 2. 清理恢复数据（防止重启弹框）
rm -f "<project_root>/Saved/Autosaves/PackageRestoreData.json"

# 3. UBT 编译（target 名来自 Source/*.Target.cs，不是 ProjectNameEditor）
"<engine_root>/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
    <TargetName> Win64 Development \
    -Project="<project_root>/<project>.uproject" -WaitMutex

# 4. 启动编辑器
start "" "<engine_root>/Engine/Binaries/Win64/UnrealEditor.exe" "<project_root>/<project>.uproject"

# 5. 等待 MCP 端口（轮询 55557）
```

**Lyra 项目特有**：target 名为 `LyraEditor`（不是 `LyraStarterGameEditor`）

---

## Step 3：执行任务

### 批处理优先（Batch-First）

**规则**：凡是多个独立的只读查询，必须合并为一次 `batch` 调用：

```python
# ❌ 低效：3次 TCP 往返
send_cmd("get_actors_in_level", {})
send_cmd("get_current_level_name", {})
send_cmd("get_world_settings", {})

# ✅ 高效：1次 TCP 往返
send_cmd("batch", {"commands": [
    {"type": "get_actors_in_level", "params": {}},
    {"type": "get_current_level_name", "params": {}},
    {"type": "get_world_settings", "params": {}}
]})
```

**适合批处理的场景**：初始状态查询、多资产信息查询、并行验证。
**不适合批处理的场景**：有依赖关系的操作（B 依赖 A 的结果）。

### 会话缓存原则

Step 0 已缓存的信息在整个会话中直接复用：
- ✅ TCP 协议字段 → 直接用 `"type"`，不再验证
- ✅ 可用命令集 → 直接查缓存，不再调 `get_capabilities`
- ✅ 项目路径/target 名 → 记录后直接复用

### 执行检查清单

- 关键操作前先查询当前状态（不假设环境）
- Actor/资产操作后用查询命令确认结果
- 使用 `check_editor_status` 确认编辑器就绪再发送命令

---

## Step 4：问题沉淀

**触发条件**：任何命令失败、报错、或行为与预期不符

**操作**：
1. 在 `References/Notes/` 创建笔记：`YYYY-MM-DD_问题简述.md`
2. 更新 `References/Notes/README.md` 的索引表
3. 提炼的通用经验 → 进入 Step 5 更新 CLAUDE.md

---

## Step 5：收尾

### 5-A. 优化 CLAUDE.md
- 更新「实现进度」表
- 更新命令模块详情
- 新增关键实现细节

### 5-B. Git Commit（仅 Git 已启用时执行）

```bash
# 安全检查：确认无真实路径泄漏
git diff --staged | grep -E "[A-Z]:[/\\]|Users/[^{]"

# 逐类暂存（不用 git add -A）
git add References/Plans/当前任务.md
git add References/Notes/           # 若有新 Note
git add Python/tools/               # 若有 Python 变更
git add MCPGameProject/             # 若有 C++ 变更
git add CLAUDE.md

git commit -m "$(cat <<'EOF'
feat: [任务简述]

[2-3行说明做了什么、为什么]

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## 快速参考

### 分支命名
```
ue/YYYY-MM-DD-task-name
```

### Commit 前缀
| 类型 | 前缀 |
|------|------|
| 新功能 | `feat:` |
| Bug 修复 | `fix:` |
| 文档 | `docs:` |
| 重构 | `refactor:` |

### 反模式清单（禁止）

| 反模式 | 正确做法 |
|--------|---------|
| 先启动编辑器再发现命令缺失 | Step 0 预检，编译后再启动 |
| 逐级评估 LiveCoding 可行性 | 有注册/头文件变更 → 直接 Tier 4 |
| 多次单独 MCP 调用可合并的查询 | 合并为 batch |
| 重复试探已知的协议细节 | Step 0 缓存，全局复用 |
| 分多次编译 | 合并全部变更，一次构建 |
| bash 下用 taskkill 终止 UE 进程 | 改用 PowerShell Stop-Process |

### 文件路径速查
| 目的 | 路径 |
|------|------|
| 执行计划 | `References/Plans/YYYY-MM-DD_xxx.md` |
| 问题记录 | `References/Notes/YYYY-MM-DD_xxx.md` |
| Python 工具 | `Python/tools/xxx_tools.py` |
| C++ 命令 | `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/` |

### 错误响应格式
```
C++ 成功：{"status": "success", "result": {...}}
C++ 失败：{"status": "error", "error": "message"}
Python：  {"success": false, "message": "..."}
```
