# UnrealMCP 任务执行 SOP

> 每次执行任务的标准流程。Git 操作见 `Git_SOP.md`，新增命令开发见 `Plugin_Dev_SOP.md`。

---

## 流程总览

```
用户需求 → ⓪ 会话初始化 → ① Plan & 能力预检 → ② 实现缺口（如有）→ ③ 执行 → ④ 问题沉淀 → ⑤ 收尾
```

---

## ⓪ 会话初始化

> **每次会话执行一次，结果全程复用，不重复查询。**

```python
# 单次 batch 获取所有环境信息
send_cmd("batch", {"commands": [
    {"type": "ping",                  "params": {}},
    {"type": "get_capabilities",      "params": {}},
    {"type": "get_current_level_name","params": {}}
]})
```

缓存内容：TCP 协议字段 `"type"`、可用命令集、编辑器状态。

**编辑器未运行时**：先完成所有 C++ 变更和编译，再启动。禁止「先启动 → 发现缺命令 → 关闭重启」。

---

## ① Plan & 能力预检

### 1-A. Git 确认
询问是否启用 Git 管控。是 → 见 `Git_SOP.md`；否 → 继续。

### 1-B. 能力预检 + 发散预测

| 用户需求 | 发散预测应额外准备的能力 |
|---------|----------------------|
| 打开动画蓝图 | create_montage / add_anim_notify / edit_anim_curve |
| 创建角色 BP | SkeletalMesh / 输入绑定 / 碰撞 |
| 创建关卡 | 光照 / SkyAtmosphere / NavMesh |
| 创建 Widget | 事件绑定 / 数据绑定 |

**原则**：宁可多实现一个不用的命令，也不要在执行中途因缺命令而重启编辑器。

### 1-C. 创建 Plan 文档
`References/Plans/YYYY-MM-DD_任务名.md`，含目标、缺口表、执行步骤、验收标准。用户确认后执行。

---

## ② 实现缺失能力

铁律：所有缺口在任务执行前全部实现，一次编译。

编译层级决策（快速判定）：
- 涉及 `RegisterCommands`/ `.h` / 新类 / `Build.cs` → **直接 Tier 4（full_rebuild）**
- 仅修改已有函数体 → Tier 3（trigger_hot_reload）
- 仅 Python 变更 → Tier 2（reload_python_tool）

full_rebuild 步骤见 `Plugin_Dev_SOP.md`。

---

## ③ 执行任务

**批处理优先**：独立只读查询必须合并为 `batch`：
```python
send_cmd("batch", {"commands": [
    {"type": "get_actors_in_level",   "params": {}},
    {"type": "get_current_level_name","params": {}},
    {"type": "get_world_settings",    "params": {}}
]})
```

**会话缓存**：Step 0 缓存的协议/命令集/路径全程复用，不重复探测。

---

## ④ 问题沉淀

遇到错误 → 运行 `/ue-note` → 保存到 `References/Notes/Known_Issues.md` 或新建文件。

---

## ⑤ 收尾

1. 更新 CLAUDE.md 实现进度
2. 若 Git 已启用 → 见 `Git_SOP.md`

---

## 快速参考

**反模式清单**（禁止）：

| 禁止 | 正确做法 |
|------|---------|
| 先启动编辑器再发现命令缺失 | Step 0 预检，编译后再启动 |
| 逐级评估 LiveCoding | 有注册/头文件变更 → 直接 Tier 4 |
| 多次单独 MCP 调用可合并的查询 | 合并为 batch |
| 重复探测已知协议细节 | Step 0 缓存复用 |
| 分多次编译 | 合并所有变更，一次 full_rebuild |
| bash 下 `taskkill` 终止 UE | 改用 PowerShell `Stop-Process` |
