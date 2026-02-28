# 已知问题与解决方案

> 合并自：`2026-02-28_处理虚幻编译弹窗等问题.md`、`2026-02-28_Stop-Hook脚本格式错误.md`、`2026-03-01_LyraStarterGame-MCP连接与open-asset-editor.md`

---

## 编辑器弹框问题

### 问题1：new_level / open_level 触发 "Save current changes?" 弹框

**原因**：切换关卡时当前关卡有未保存修改。
**修复**：C++ 侧在切换前调用 `SilentSaveAllDirtyPackages(bPromptUserToSave=false)`。`/Temp/` 包自动跳过。
**规避**：始终用 `safe_switch_level`，不直接调 `new_level`/`open_level`。

### 问题2：save_current_level 对临时关卡触发 "Save As" 弹框（~15s 超时）

**原因**：`new_level`（无 asset_path）创建 `/Temp/Untitled`，此关卡无法静默保存。
**修复**：C++ 检测 `/Temp/` 路径并立即返回错误；Python 侧用 `get_level_dirty_state` 检查 `is_temp`。
**规避**：用 `safe_switch_level()` 创建新关卡。

### 问题4：启动编辑器时出现 "Rebuild modules?" 弹框

**原因**：修改 C++ 源码后直接启动编辑器，.dll 比源码旧。
**修复**：先 UBT 预编译（命令行静默），再启动编辑器：
```bash
dotnet "<engine_root>/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" \
    <TargetName>Editor Win64 Development \
    -Project="<project>.uproject" -WaitMutex
```

### 问题5：full_rebuild 后出现 "Recover Packages?" 弹框

**原因**：Kill 编辑器后重启，UE 检测到 `PackageRestoreData.json`。
**修复**：Kill 后、启动前删除：
- `<project>/Saved/Autosaves/PackageRestoreData.json`（**关键**）
- `<project>/Saved/Autosaves/Temp/`（可选）

---

## MCP / TCP 协议问题

### 问题8：手写 TCP 脚本超时——字段名 `"type"` 不是 `"command"`

**现象**：发送 `{"command": "ping"}` 后连接超时，无响应。
**根因**：C++ 解析 `"type"` 字段，`"command"` 被忽略，服务器不回复。
**正确格式**：
```python
payload = json.dumps({"type": "ping", "params": {}}).encode("utf-8")
```

### 问题3：spawn_blueprint_actor 重名 Actor 导致编辑器 Crash

**现象**：`actor_name` 在关卡中已存在时，UE Fatal Error。
**修复**：`HandleSpawnBlueprintActor` 已加重名检查。
**防御**：Python 侧 `actor_name` 加时间戳或序号后缀。

### 问题6：新增 MCP 命令后 LiveCoding 热重载无效

**原因**：`RegisterCommands()` 在模块初始化时调用，LiveCoding 仅更新函数体，不重新注册。
**规则**：**新增命令必须 full_rebuild（Tier 4），跳过 LiveCoding 评估。**

---

## 系统 / 进程问题

### 问题9：Windows bash 下 `taskkill /F /IM` 无效

**根因**：bash（Git Bash/MinGW）对 `/F`、`/IM` 参数解析异常。
**正确方法**：
```bash
powershell -Command "Stop-Process -Name UnrealEditor -Force -ErrorAction SilentlyContinue; Stop-Process -Name LiveCodingConsole -Force -ErrorAction SilentlyContinue"
```

### 问题10：外部项目 UBT target 名必须与 Target.cs 文件名一致

**现象**：UBT 报 `Couldn't find target rules file`。
**排查**：
```bash
ls "<project>/Source/" | grep "Editor.Target"
# → LyraEditor.Target.cs → target: LyraEditor（不是 LyraStarterGameEditor）
```

---

## Hook 开发问题

### Stop Hook 三条铁律

1. **stdout 只能是 JSON** — 调试信息用 `sys.stderr`，不能有任何 `print()` 文字
2. **必须检查 `stop_hook_active`** — 为 true 时直接 `sys.exit(0)`，否则无限循环
3. **Windows 路径含空格必须加引号** — `"python \"<path with spaces>/script.py\""`

**最小模板**：
```python
hook_input = json.load(sys.stdin)
if hook_input.get("stop_hook_active"):
    sys.exit(0)
print(json.dumps({"decision": "block", "reason": "..."}))
sys.exit(0)
```
