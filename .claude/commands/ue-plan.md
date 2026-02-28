Create a new execution plan document for an Unreal Engine automation task.

Follow the SOP at References/SOP/UE_Automation_SOP.md.

## Step 0: Session Init (if not already done this session)

Run a single batch call to verify editor state and cache the command set:
```python
send_cmd("batch", {"commands": [
    {"type": "ping", "params": {}},
    {"type": "get_capabilities", "params": {}}
]})
```
Cache result: TCP protocol = `"type"` field; available command list for gap analysis below.
If editor is not running, note that — capability gaps must be fixed BEFORE the editor is launched.

## Step 1-A: Git Management

Ask the user ONE question before anything else:
"本次流程是否启用自动 Git 管控？（是/否）"
- If YES → execute the git init sequence below
- If NO  → skip to Step 1-B

**Git init sequence (only if user says YES):**
1. Run `git log --oneline -10` to read recent change history
2. Run `git status` — if there are uncommitted changes, warn the user
3. Derive a short branch name from the task (English, lowercase, hyphens):
   format: `ue/YYYY-MM-DD-task-name`
4. Run `git checkout -b ue/YYYY-MM-DD-task-name`

## Step 1-B: Capability Pre-Check & Forward Projection

Before writing the plan, perform TWO analyses:

### Direct Requirements
List every MCP command needed to complete this exact task.
Compare against the cached command set → identify missing commands.

### Forward Projection
Think 1-2 steps ahead: what related tasks might the user want next?
Add those commands to the gap analysis proactively.

Examples:
- Open AnimBlueprint → likely next: create montage, add anim notify, edit curves
- Create character BP → likely next: add mesh component, bind input, set collision
- Create level → likely next: add lighting, atmosphere, fog, NavMesh
- Create widget → likely next: bind events, data binding, animations

**Rule**: It is better to implement one extra unused command than to shut down and restart
the editor mid-task due to a missing capability.

## Step 1-C: Plan Document

Create plan file at: References/Plans/YYYY-MM-DD_<task-name>.md

```markdown
# 计划：[任务名称]

**日期**：YYYY-MM-DD
**目标**：[1-2句描述]
**Git 分支**：ue/YYYY-MM-DD-task-name（或「未启用」）

## 涉及模块
- C++ 命令：[列举]
- Python 工具：[列举]

## 能力缺口（预检结果）
| 命令/工具 | 状态 | 实现计划 |
|---------|------|---------|
| xxx | ❌ 缺失（直接需求） | 本次实现 |
| yyy | ❓ 发散预测 | 本次实现（防止中途中断） |
| zzz | ✅ 已有 | 直接调用 |

## 构建计划
- 涉及 C++ 变更？→ [ ] 统一实现所有缺失命令 → 一次 full_rebuild（关闭编辑器前完成）
- 仅 Python 变更？→ [ ] reload_python_tool

## 执行步骤
- [ ] Step 0：会话初始化 & 能力预检（已完成）
- [ ] Step 1：实现缺失命令（若有）→ full_rebuild → 启动编辑器
- [ ] Step 2：...

## 验收标准
- [ ] ...

## 风险点
- ...
```

Show the plan to the user and wait for confirmation before proceeding.

The task/context: $ARGUMENTS
