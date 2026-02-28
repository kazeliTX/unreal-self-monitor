# UE 版本兼容性修复汇总

> 合并自：`2026-02-28_UE4-compat-fixes.md`、`2026-02-28_UE5.6-编译兼容性修复.md`、`2026-02-28_UE4工程信息快照.md`

---

## 1. UE4 vs UE5 API 差异矩阵

| 功能 | UE4（4.22+） | UE5 | 处理方式 |
|------|------------|-----|---------|
| IWYU 配置 | `bEnforceIWYU = true` | `IWYUSupport = IWYUSupport.Full` | Build.cs 版本分支 |
| 编辑器样式 | `FEditorStyle::GetStyleSetName()` | `FAppStyle::GetAppStyleSetName()` | `UnrealMCPCompat.h` 宏 `MCP_STYLE_NAME` |
| EditorStyle 模块 | 需要 `"EditorStyle"` 依赖 | 使用 `"AppFramework"` | Build.cs `if (Major < 5)` |
| DeveloperSettings 模块 | 在 Engine 内，无需单独依赖 | 独立模块 `"DeveloperSettings"` | Build.cs `if (Major >= 5)` |
| `UEditorActorSubsystem` | 不存在 | 存在 | `#if ENGINE_MAJOR_VERSION >= 5` |
| Actor 复制 | `GEditor->edactDuplicateSelected(...)` | `EdActorSub->DuplicateActors(...)` | 版本守卫 |
| `FAssetData` 资产类型 | `AssetClass.ToString()` | `AssetClassPath.GetAssetName().ToString()` | 版本守卫 |
| 资产标签迭代 | `TPair<FName, FString>` | `TPair<FName, FAssetTagValueRef>` | 版本守卫 |
| `CreatePackage` | `CreatePackage(nullptr, *path)` 2参数 | `CreatePackage(*path)` 1参数 | 版本守卫 |
| `TryGetNumberField` | 只接受 `double&` | 接受 `float&` 或 `double&` | 用 `double` |
| AssetRegistry 头文件 | `AssetRegistryModule.h` | `AssetRegistry/AssetRegistryModule.h` | `#if ENGINE_MAJOR_VERSION >= 5` |
| BlueprintEditorLibrary | 不存在 | 存在 | Build.cs 条件引入 |
| Enhanced Input | 不存在 | 存在 | `#if MCP_ENHANCED_INPUT_SUPPORTED` |
| `TActorIterator` | 需 `#include "EngineUtils.h"` | 同左 | **每个新 .cpp 都要检查** |

---

## 2. UE5.6 + VS2022 14.44 特有问题

### 2.1 头文件路径（UE5.6 实测）

| ❌ 错误路径 | ✅ 正确路径 |
|-----------|-----------|
| `"Engine/SkyAtmosphere.h"` | `"Components/SkyAtmosphereComponent.h"` |
| `"Engine/WorldSettings.h"`（任何版本） | `"GameFramework/WorldSettings.h"` |

### 2.2 VS2022 14.44+ C4459 阴影变量警告升级为错误

**触发条件**：`StringConv.h` 内 `TStringConversion` 模板 `BufferSize` 遮蔽全局声明。

**修复（两层）**：
```cpp
// MCPServerRunnable.cpp 顶部（Launcher/Adaptive Build 有效）
#ifdef _MSC_VER
#pragma warning(disable: 4459)
#endif

// 同时修正字节长度：
FTCHARToUTF8 Utf8Response(*Response);
ClientSocket->Send((const uint8*)Utf8Response.Get(), Utf8Response.Length(), BytesSent);
```

```csharp
// UnrealMCP.Build.cs（源码引擎 Unity Build 必须加）
ShadowVariableWarningLevel = WarningLevel.Off;
// UE4 等效：bEnableShadowVariableWarnings = false;
```

---

## 3. UE4.24 参考工程信息

| 属性 | 值 |
|------|---|
| 引擎版本 | UE 4.24.2（源码编译） |
| EngineAssociation | `""` — 引擎在 `../Engine/` 相对路径 |
| UE4 专属排除项 | Enhanced Input、BlueprintEditorLibrary、FAppStyle、UEditorActorSubsystem |

**install.py 调用（UE4 源码引擎）**：
```bash
python Python/scripts/install.py "<project>.uproject" --engine-root "<engine_source_root>"
```

---

## 4. 通用防坑清单

- `ASkyAtmosphere` 头文件 → 永远用 `Components/SkyAtmosphereComponent.h`
- `WorldSettings.h` → 永远用 `GameFramework/WorldSettings.h`（无需版本条件）
- `TActorIterator` → 每个新 .cpp 都要 `#include "EngineUtils.h"`
- 新安装到外部项目前 → 先检查 `Source/*.Editor.Target.cs` 确认 UBT target 名（见 Known_Issues.md 问题10）
- `cp -r src/ existing_dst/` → 若 dst 已存在会嵌套，应逐文件复制
