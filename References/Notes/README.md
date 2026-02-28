# Notes — Debug 笔记与解决方案

每次遇到问题并解决后，在此目录新建文件记录。
文件命名：`YYYY-MM-DD_问题简述.md`

## 文件结构模板

```
# 问题：[简述]

**日期**：YYYY-MM-DD
**影响模块**：[C++命令模块 / Python工具 / 配置]
**严重程度**：高 / 中 / 低

## 现象
[错误信息、复现步骤]

## 根因
[分析]

## 解决方案
[具体操作步骤]

## 预防措施 / 经验提炼
[今后如何避免，或需要加入 CLAUDE.md 的通用原则]
```

---

## 笔记索引

| 日期 | 问题/主题 | 文件 |
|------|---------|------|
| 2026-02-28 | Stop Hook 脚本 stdout/无限循环/路径引号三项错误 | [2026-02-28_Stop-Hook脚本格式错误.md](./2026-02-28_Stop-Hook脚本格式错误.md) |
| 2026-02-28 | UE4.24.2 源码编译参考工程信息快照（UE4 兼容验证目标） | [2026-02-28_UE4工程信息快照.md](./2026-02-28_UE4工程信息快照.md) |
| 2026-02-28 | UE4 兼容性修复：Build.cs / Include 路径 / API 差异全表 | [2026-02-28_UE4-compat-fixes.md](./2026-02-28_UE4-compat-fixes.md) |
| 2026-02-28 | 外部项目学习：get_level_dirty_state / safe_switch_level / full_rebuild / 关卡弹窗防御 | [2026-02-28_处理虚幻编译弹窗等问题.md](./2026-02-28_处理虚幻编译弹窗等问题.md) |
| 2026-02-28 | UE5.6 + VS2022 14.44 编译失败：SkyAtmosphere 路径、WorldSettings.h、C4459、EngineUtils.h | [2026-02-28_UE5.6-编译兼容性修复.md](./2026-02-28_UE5.6-编译兼容性修复.md) |
| 2026-03-01 | LyraStarterGame MCP 连接（type字段/进程终止/UBT target名）+ open_asset_editor 实现 | [2026-03-01_LyraStarterGame-MCP连接与open-asset-editor.md](./2026-03-01_LyraStarterGame-MCP连接与open-asset-editor.md) |
