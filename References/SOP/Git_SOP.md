# Git 工作流 SOP

---

## 分支命名

```
ue/YYYY-MM-DD-task-name（英文小写，连字符）
```

## 会话开始

```bash
git log --oneline -10   # 了解上下文
git status              # 确认工作区干净
git checkout -b ue/YYYY-MM-DD-task-name
```

## Commit 规范

**前缀**：`feat:` / `fix:` / `docs:` / `refactor:`

**每次 commit 必须含**：
```
Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
```

**安全检查（提交前必做）**：
```bash
git diff --staged | grep -E "[A-Z]:[/\\\\]|Users/[^{]"
# 确认无真实路径（用占位符替代）
```

**逐类暂存（禁止 `git add -A`）**：
```bash
git add References/Plans/当前任务.md
git add References/Notes/           # 若有新 Note
git add Python/tools/               # Python 变更
git add MCPGameProject/             # C++ 变更
git add CLAUDE.md
```

**Commit 消息格式**：
```bash
git commit -m "$(cat <<'EOF'
feat: [任务简述]

[2-3 行说明做了什么、为什么]

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

## 收尾

```bash
# 合并到 main
git checkout main
git merge ue/YYYY-MM-DD-task-name --no-ff

# 删除功能分支
git branch -d ue/YYYY-MM-DD-task-name

# 推送
git push
```
