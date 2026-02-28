# UnrealMCP AI 自动化标准操作流程（SOP）

> 适用于所有 UE 工程。Claude 在每次执行自动化任务时须遵循本流程。

---

## 流程总览

```
用户提出需求
    │
    ▼
① /ue-plan  →  询问是否启用 Git 管控
    │           ├─ 是 → 读取 git log → checkout 新分支
    │           └─ 否 → 跳过
    │           产出 Plan 文档，用户确认后继续
    ▼
② 能力评估
    ├─ 能力完善 ──────────────────► 直接调用工具执行
    └─ 能力缺失 → 自动完善代码 → 验证运行 → 自检
    │
    ▼
③ 执行任务
    │
    ▼
④ 问题沉淀（遇到问题时）→ /ue-note → 写入 Notes
    │
    ▼
⑤ 收尾
    ├─ 更新 CLAUDE.md
    └─ Git 已启用 → git commit（含本次所有变更）
```

---

## 步骤详解

### Step 1：需求分析、Git 确认 & Plan 文档

**触发条件**：用户描述一个非单步的任务（需要 3+ 操作）

**操作**：

#### 1-A. Git 管控确认（必须在任何文件变更之前）
询问用户：**「本次流程是否启用自动 Git 管控？」**

| 用户选择 | 操作 |
|---------|------|
| **是** | 执行下方 Git 初始化流程 |
| **否** | 跳过，直接进入 1-B |

**Git 初始化流程**（仅在用户确认「是」后执行）：
```bash
# 1. 阅读近期变更历史，了解上下文
git log --oneline -10

# 2. 确认工作区干净（有未提交变更时提示用户）
git status

# 3. 从 main 创建新分支
#    命名规则：ue/YYYY-MM-DD-任务简述（英文小写，空格用连字符）
git checkout -b ue/YYYY-MM-DD-task-name
```

#### 1-B. 生成 Plan 文档
在 `References/Plans/` 创建计划文档：
- 命名：`YYYY-MM-DD_任务名称.md`
- 记录 Git 分支名（若已启用）

**Plan 文档模板**：
```markdown
# 计划：[任务名称]

**日期**：YYYY-MM-DD
**目标**：[1-2句描述]
**Git 分支**：ue/YYYY-MM-DD-task-name（或「未启用」）

## 涉及模块
- C++ 命令：[列举]
- Python 工具：[列举]

## 执行步骤
- [ ] Step 1：...
- [ ] Step 2：...

## 验收标准
- [ ] ...

## 风险点
- ...
```

向用户展示计划并确认后再开始执行。

---

### Step 2：能力评估

**自问清单**：
- 所需 C++ 命令是否已在 `FMCPCommandRegistry` 中注册？
- 所需 Python 工具是否存在对应 `xxx_tools.py`？
- 使用 `get_capabilities` 命令验证运行时可用命令列表

**如果能力缺失**：
1. 确定缺失层级（C++ / Python）
2. 按照 CLAUDE.md「开发规范」实现新命令/工具
3. C++ 变更需触发热重载（Tier 3）或 UBT 构建（Tier 4）
4. 编写冒烟测试验证新功能可用

---

### Step 3：执行任务

**执行原则**：
- 优先使用 `batch` 命令减少 TCP 往返
- 关键操作前先查询当前状态（不假设环境）
- Actor/资产操作后用查询命令确认结果
- 使用 `check_editor_status` 确认编辑器就绪再发送命令

**编译层级选择**：
| 变更类型 | 使用方法 |
|---------|---------|
| Blueprint 逻辑 | `compile_blueprint` |
| Python 工具 | `reload_python_tool` |
| C++ 函数体 | `trigger_hot_reload` |
| C++ 头文件/类结构 | `run_ubt_build` |

---

### Step 4：问题沉淀

**触发条件**：任何命令失败、报错、或行为与预期不符

**操作**：
1. 在 `References/Notes/` 创建笔记
   - 命名：`YYYY-MM-DD_问题简述.md`
   - 使用 Notes/README.md 中的模板
2. 更新 `References/Notes/README.md` 的索引表
3. 提炼的通用经验 → 进入 Step 5

---

### Step 5：收尾

#### 5-A. 优化 CLAUDE.md
**触发条件**：
- 发现 CLAUDE.md 中有错误信息
- 有新的通用规范需要记录
- 完成重大功能扩展（新模块/新命令）

**操作**：
- 更新「实现进度」表
- 更新「C++ 命令模块详情」或「Python 工具模块详情」
- 新增关键实现细节（如有新的坑/规避方案）

#### 5-B. Git Commit（仅 Git 已启用时执行）

```bash
# 1. 查看本次所有变更
git diff --stat

# 2. 暂存相关文件（避免 git add -A，逐类暂存）
git add References/Plans/当前任务.md      # Plan 文档
git add References/Notes/                 # 若有新 Note
git add Python/tools/                     # 若有 Python 变更
git add MCPGameProject/                   # 若有 C++ 变更
git add CLAUDE.md                         # 若有更新

# 3. 提交
git commit -m "$(cat <<'EOF'
feat: [任务简述]

[2-3行说明做了什么、为什么]

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

**Commit message 规范**：
| 类型 | 前缀 | 场景 |
|------|------|------|
| 新功能 | `feat:` | 新增命令/工具/能力 |
| Bug 修复 | `fix:` | 修复错误 |
| 文档 | `docs:` | 更新 CLAUDE.md/Notes/Plans |
| 重构 | `refactor:` | 结构调整，无功能变化 |

---

## 快速参考

### 分支命名规则
```
ue/YYYY-MM-DD-task-name
例：ue/2026-02-28-add-animation-commands
```

### 常用调试命令
```bash
# 检查编辑器状态
python Python/scripts/debug_runner.py --smoke-test

# 监视编译
python Python/scripts/compile_watch.py --interval 3
```

### 错误响应格式
```
C++ 成功：{"success": true, ...}
C++ 失败：{"status": "error", "error": "message"}
Python 统一：{"success": false, "message": "..."}
```

### 文件路径速查
| 目的 | 路径 |
|------|------|
| 执行计划 | `References/Plans/YYYY-MM-DD_xxx.md` |
| 问题记录 | `References/Notes/YYYY-MM-DD_xxx.md` |
| 架构文档 | `References/Architecture/` |
| Python 工具 | `Python/tools/xxx_tools.py` |
| C++ 命令 | `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/` |
