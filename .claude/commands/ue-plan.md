Create a new execution plan document for an Unreal Engine automation task.

Follow the SOP at References/SOP/UE_Automation_SOP.md Step 1.

Steps:

## Step 1-A: Git Management

Ask the user ONE question before anything else:
"本次流程是否启用自动 Git 管控？（是/否）"
- If YES → execute the git init sequence below
- If NO  → skip to Step 1-B

**Git init sequence (only if user says YES):**
1. Run `git log --oneline -10` to read recent change history
2. Run `git status` — if there are uncommitted changes, warn the user and ask if they want to continue
3. Derive a short branch name from the task (English, lowercase, hyphens):
   format: `ue/YYYY-MM-DD-task-name`
4. Run `git checkout -b ue/YYYY-MM-DD-task-name`
5. Confirm the branch was created

## Step 1-B: Plan Document

1. Understand the task from $ARGUMENTS, or ask the user if not provided
2. Analyze: identify required C++ commands, Python tools, capability gaps (check CLAUDE.md)
3. Create plan file at: References/Plans/YYYY-MM-DD_<task-name>.md using this template:

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

4. Show the plan to the user and wait for confirmation before proceeding with execution

The task/context: $ARGUMENTS
