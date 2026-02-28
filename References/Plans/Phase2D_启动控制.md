# Phase 2D — 启动控制（待实现）

> 完整设计见 `history/Phase2_自愈诊断计划.md`。本文件只跟踪未完成部分。

**前置状态**：Phase 2A/2B/2C 已完成（117 个命令，含 DiagnosticsCommands / TestCommands / MaterialCommands）

---

## 目标

AI 能自动启动 / 重启编辑器，无需人工干预。

**验收标准**：AI 检测到编辑器崩溃后，能自动完成「kill → 清理 → UBT 预编 → 启动 → wait_for_ready」并继续任务。

---

## 待实现清单

### Python — `system_tools.py` 扩展

- [ ] `launch_editor(project_path)` — 引擎路径自动探测（Launcher 注册表 + 源码工程两种模式）
- [ ] `restart_editor(project_path)` — kill → 删 `Saved/Autosaves/PackageRestoreData.json` → UBT 预编 → 启动 → `wait_for_editor_ready`
- [ ] 引擎路径探测工具函数 — 读 `HKEY_LOCAL_MACHINE\SOFTWARE\EpicGames\Unreal Engine\<ver>` + 常见路径 fallback
- [ ] PID 文件管理 — 启动后写 `.mcp_editor.pid`，重启前检查

### 集成测试

- [ ] `debug_runner.py` 冒烟测试加入 restart 场景
- [ ] Phase 2C `run_level_validation` 在重启后自动重跑

---

## 实现约束

- 清理 `PackageRestoreData.json` 是防止 "Recover Packages?" 弹框的关键步骤（⚠️ 问题5）
- UBT 预编译命令格式见 `References/SOP/Plugin_Dev_SOP.md`
- Windows 下 kill 用 PowerShell（bash 的 `taskkill /F /IM` 无效，见 ⚠️ 问题9）
