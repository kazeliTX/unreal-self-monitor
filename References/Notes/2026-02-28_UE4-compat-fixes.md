# UE4.24 兼容性修复记录

**日期**: 2026-02-28
**目标引擎**: 自编译 UE4.24.2（D:\kazeli_KAZELI-PC0_S1_1）
**安装目标**: D:\kazeli_KAZELI-PC0_S1_1\NZMobile\Plugins\UnrealMCP\

---

## 修复汇总

### Build.cs 依赖

| 问题 | 原因 | 修复 |
|------|------|------|
| `DeveloperSettings` 模块找不到 | UE4 中该功能在 `Engine` 模块内，UE5 才独立 | `if (Major >= 5)` 条件添加 |
| `UGeneralProjectSettings` LNK2019 | 该类在 `EngineSettings` 模块中，不在 `Engine` | 添加 `"EngineSettings"` 依赖 |
| `FEditorStyle::GetStyleSetName` LNK2019 | UE4 需要 `EditorStyle` 模块；UE5 用 `FAppStyle` | `if (Major < 5)` 条件添加 `"EditorStyle"` |

### Include 路径差异

| 头文件 | UE5 路径 | UE4 路径 |
|--------|----------|----------|
| AssetRegistry | `AssetRegistry/AssetRegistryModule.h` | `AssetRegistryModule.h` |
| AssetRegistry IFace | `AssetRegistry/IAssetRegistry.h` | `IAssetRegistry.h` |
| WorldSettings | `Engine/WorldSettings.h` | `GameFramework/WorldSettings.h` |
| EditorActorSubsystem | `Subsystems/EditorActorSubsystem.h` | **不存在**（UE4 无此类） |

所有路径差异均用 `#if ENGINE_MAJOR_VERSION >= 5` 守卫。

### API 差异

| API | UE5 | UE4 |
|-----|-----|-----|
| `FAssetData` 资产类型 | `AssetClassPath.GetAssetName().ToString()` | `AssetClass.ToString()` |
| 资产标签迭代 | `TPair<FName, FAssetTagValueRef>` | `TPair<FName, FString>` |
| `CreatePackage` | `CreatePackage(*path)` (1参数) | `CreatePackage(nullptr, *path)` (2参数) |
| `UEditorActorSubsystem` | 存在，`GetEditorSubsystem<>()` | **完全不存在** |
| Actor 复制 | `EdActorSub->DuplicateActors(...)` | `GEditor->edactDuplicateSelected(...)` |
| `TryGetNumberField` | 接受 `float&` | 只接受 `double&` |
| `FMath::Max` | 可混合 int/float | 要求类型完全一致 |

### 其他问题

- **`TActorIterator<AActor>` 未定义**: 需要 `#include "EngineUtils.h"`（DiagnosticsCommands.cpp 漏了）
- **`cp -r src/ existing_dst/` 陷阱**: 若 `dst` 已存在则创建嵌套子目录 `dst/src/`，导致 UBT 编译两份文件产生重复符号错误。应逐文件复制。

---

## 涉及文件

- `UnrealMCP.Build.cs`
- `Private/Commands/UnrealMCPAssetCommands.cpp`
- `Private/Commands/UnrealMCPBlueprintCommands.cpp`
- `Private/Commands/UnrealMCPCommonUtils.cpp`
- `Private/Commands/UnrealMCPDiagnosticsCommands.cpp`
- `Private/Commands/UnrealMCPEditorCommands.cpp`
- `Private/Commands/UnrealMCPProjectCommands.cpp`
- `Private/Commands/UnrealMCPTestCommands.cpp`
- `Private/Commands/UnrealMCPUMGCommands.cpp`
- `Private/UnrealMCPBridge.cpp`

全部修改已在 commit `2881a92` 中提交。
