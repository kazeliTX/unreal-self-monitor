# 问题：LyraStarterGame MCP 连接调试 + open_asset_editor 命令实现

**日期**：2026-03-01
**影响模块**：AssetCommands（C++）、TCP 协议、系统工具（进程管理）
**严重程度**：中

---

## 现象

1. **MCP 命令超时**：端口 55557 已开放，Python 发送命令后收不到响应，一直超时。
2. **Live Coding 阻断 UBT**：`taskkill /F /IM` 命令杀不掉编辑器进程，导致 UBT 报错 `Unable to build while Live Coding is active`。
3. **无法打开资产编辑器**：MCP 命令集中没有 `open_asset_editor`，无法通过自动化在编辑器中打开动画蓝图。

---

## 根因

### 问题 1：JSON 字段名错误（`command` vs `type`）

Python 测试脚本发送：
```json
{"command": "ping", "params": {}}
```
而 C++ `MCPServerRunnable::Run()` 读取的字段是 `"type"`：
```cpp
if (JsonObject->TryGetStringField(TEXT("type"), CommandType))
```
字段缺失 → JSON 解析成功但命令未执行 → 无响应 → 超时。

### 问题 2：`taskkill /F /PID` 在 bash shell 下无效

bash 环境下 `taskkill /F /PID 12345` 参数格式不被 Windows 识别（正斜杠被转义或忽略）。
必须通过 PowerShell `Stop-Process -Name xxx -Force` 才能强制终止。

### 问题 3：UBT 目标名称错误

LyraStarterGame 的编辑器 target 是 `LyraEditor`（`Source/LyraEditor.Target.cs`），
不是 `LyraStarterGameEditor`。

### 问题 4：功能缺失

MCP 没有 `open_asset_editor` 命令，无法通过协议在编辑器中打开任意资产。

---

## 解决方案

### 1. 正确的 Python MCP 协议格式

```python
# 正确：使用 "type" 字段
payload = json.dumps({"type": "ping", "params": {}}).encode("utf-8")

# 错误：不能用 "command"
payload = json.dumps({"command": "ping", "params": {}}).encode("utf-8")
```

unreal_mcp_server.py 的 `send_command()` 已正确使用 `"type"`，直接调 Python 工具没问题，手写测试脚本时要注意。

### 2. 强制终止 UE 进程：用 PowerShell

```python
# 通过 Python subprocess 调用 PowerShell（可绕过 bash 参数解析问题）
import subprocess
subprocess.run([
    "powershell", "-Command",
    "Stop-Process -Name UnrealEditor -Force -ErrorAction SilentlyContinue; "
    "Stop-Process -Name LiveCodingConsole -Force -ErrorAction SilentlyContinue"
], capture_output=True)
```

或 bash 中：
```bash
powershell -Command "Stop-Process -Name UnrealEditor -Force -ErrorAction SilentlyContinue"
```

### 3. LyraEditor UBT 正确调用

```bash
"<engine_root>/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
    LyraEditor Win64 Development \
    -Project="<project_root>/LyraStarterGame.uproject" \
    -WaitMutex
```

关键：target 名 = `LyraEditor`（不是 `LyraStarterGameEditor`）

### 4. 实现 open_asset_editor 命令

**头文件** `UnrealMCPAssetCommands.h`：
```cpp
TSharedPtr<FJsonObject> HandleOpenAssetEditor(const TSharedPtr<FJsonObject>& Params);
```

**实现** `UnrealMCPAssetCommands.cpp`：
```cpp
// include
#if ENGINE_MAJOR_VERSION >= 5
#include "Subsystems/AssetEditorSubsystem.h"
#else
#include "Toolkits/AssetEditorManager.h"
#endif

// handler
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleOpenAssetEditor(...)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return CreateErrorResponse(TEXT("asset_path required"));

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) return CreateErrorResponse(TEXT("Asset not found"));

    bool bOpened = false;
#if ENGINE_MAJOR_VERSION >= 5
    auto* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (Sub) bOpened = Sub->OpenEditorForAsset(Asset);
#else
    bOpened = FAssetEditorManager::Get().OpenEditorForAsset(Asset);
#endif

    // build result...
}
```

**调用示例**：
```python
send_cmd("open_asset_editor", {
    "asset_path": "/Game/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base"
})
# → {"status": "success", "result": {"opened": true, "asset_class": "AnimBlueprint", ...}}
```

---

## 已发现的全部 LyraStarterGame 动画蓝图（19 个）

| 名称 | 路径 | 类型 |
|------|------|------|
| ABP_Mannequin_Base | /Game/Characters/Heroes/Mannequin/Animations/ | 主角色动画 BP（**入口**） |
| ABP_Mannequin_CopyPose | 同上 | CopyPose 重定向辅助 |
| ABP_Mannequin_Retarget | 同上 | 重定向 |
| ABP_Mannequin_TopDown | 同上 | 俯视角变体 |
| ABP_ItemAnimLayersBase | .../LinkedLayers/ | Linked Layer 基类 |
| ABP_PistolAnimLayers / _Feminine | .../Locomotion/Pistol/ | 手枪动画层 |
| ABP_RifleAnimLayers / _Feminine | .../Locomotion/Rifle/ | 步枪动画层 |
| ABP_ShotgunAnimLayers / _Feminine | .../Locomotion/Shotgun/ | 霰弹枪动画层 |
| ABP_UnarmedAnimLayers / _Feminine | .../Locomotion/Unarmed/ | 徒手动画层 |
| ABP_Manny_PostProcess | .../Rig/ | Manny 后处理 |
| ABP_Quinn_PostProcess | .../Rig/ | Quinn 后处理 |
| ABP_UE4_Mannequin_Retarget | .../Mannequin_UE4/Animations/ | UE4 Mannequin 重定向 |
| ABP_Weap_Pistol/Rifle/Shotgun | /Game/Weapons/.../Animations/ | 武器动画 BP |

---

## 预防措施 / 经验提炼

1. **手写 MCP 测试脚本必须用 `"type"` 字段**，不能用 `"command"`
2. **终止 UE 进程必须用 PowerShell `Stop-Process -Force`**，bash 的 `taskkill /F /IM` 在本环境失效
3. **Lyra 项目 UBT target 名是 `LyraEditor`**，不是 `LyraStarterGameEditor`
4. **新增 MCP 命令必须 full_rebuild**（Tier 4），LiveCoding 热重载对 RegisterCommands 注册无效
5. **`open_asset_editor` 已实装**，后续可直接调用打开任意 UE 资产编辑器
