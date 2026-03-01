# Phase 2D — 启动控制

**前置状态**：Phase 2A/2B/2C 已完成（117 个命令）

---

## 目标

AI 能自动启动 / 重启编辑器，无需人工干预。

**验收标准**：AI 检测到编辑器崩溃后，能自动完成「kill → 清理 → UBT 预编 → 启动 → wait_for_ready」并继续任务。

---

## 实现状态

### Python — `system_tools.py` 扩展

- [x] `launch_editor(project_file)` — 离线引擎路径探测（Windows 注册表 UUID/版本号 + 源码工程父目录遍历 + 常见安装路径 fallback）
- [x] `restart_editor(project_file)` — PowerShell force-kill → 清理 `PackageRestoreData.json` → 离线路径探测 → 重启 → 等待就绪
- [x] `_detect_engine_from_uproject()` — 读 .uproject EngineAssociation → 注册表 / 父目录 / 常见路径三层探测
- [x] PID 文件管理 — `launch_editor`/`restart_editor` 写 `.mcp_editor.pid`，`get_pid_file_info` 查询
- [x] `get_pid_file_info` — 新增工具：返回 PID 文件记录的编辑器进程信息
- [x] `_kill_editor_powershell()` — PowerShell Stop-Process（替代 bash 下无效的 taskkill）
- [x] `_cleanup_restore_data()` — 删除 PackageRestoreData.json（防 "Recover Packages?" 弹框）

### 集成测试

- [ ] `debug_runner.py` 冒烟测试加入 restart 场景
- [ ] Phase 2C `run_level_validation` 在重启后自动重跑

---

## 待激活

Python Tier 2 变更，需运行 `reload_python_tool("system")` 使改动生效（无需 full_rebuild）。
