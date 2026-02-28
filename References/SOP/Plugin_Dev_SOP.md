# 插件开发 SOP（新增命令 / 版本兼容）

---

## 一、新增 C++ 命令

### 步骤

1. **在对应模块头文件**（`Public/Commands/UnrealMCPXxxCommands.h`）声明 handler：
   ```cpp
   TSharedPtr<FJsonObject> HandleXxx(const TSharedPtr<FJsonObject>& Params);
   ```

2. **在 `RegisterCommands()` 中注册**（`Private/Commands/UnrealMCPXxxCommands.cpp`）：
   ```cpp
   Registry.RegisterCommand(TEXT("xxx_command"),
       [this](const TSharedPtr<FJsonObject>& P) { return HandleXxx(P); });
   ```

3. **实现 handler**，错误返回：
   ```cpp
   return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("错误信息"));
   ```

4. **若新增整个模块**，还需在 `UnrealMCPBridge.h/.cpp` 中：
   - 声明 `TSharedPtr<FUnrealMCPXxxCommands> XxxCommands`
   - 构造函数：`XxxCommands = MakeShared<FUnrealMCPXxxCommands>();`
   - `RegisterCommands(*CommandRegistry)` 调用
   - 析构函数：`XxxCommands.Reset()`

5. **执行 full_rebuild**（见下方）

### ⚠️ 编译层级铁律

新增 MCP 命令 ≈ 必然涉及 `RegisterCommands()` 注册，**直接走 Tier 4，不需要评估 LiveCoding**。

---

## 二、新增 Python 工具

1. 创建 `Python/tools/xxx_tools.py`，实现 `register_xxx_tools(mcp: FastMCP)`
2. 工具函数遵循规范（见 CLAUDE.md `Python MCP 工具编写规范`）
3. 无需修改 `unreal_mcp_server.py`（自动发现）
4. 热重载（无需重启 MCP 服务器）：`reload_python_tool("xxx")`

---

## 三、full_rebuild 标准流程

```bash
# 1. 关闭编辑器
powershell -Command "Stop-Process -Name UnrealEditor -Force -ErrorAction SilentlyContinue; Stop-Process -Name LiveCodingConsole -Force -ErrorAction SilentlyContinue"

# 2. 清理恢复数据（防 "Recover Packages?" 弹框）
rm -f "<project_root>/Saved/Autosaves/PackageRestoreData.json"

# 3. UBT 编译（target 名来自 Source/*.Editor.Target.cs）
#    示例（Launcher 引擎）：
"<engine_root>/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
    <TargetName>Editor Win64 Development \
    -Project="<project_root>/<project>.uproject" -WaitMutex

# 4. 启动编辑器
start "" "<engine_root>/Engine/Binaries/Win64/UnrealEditor.exe" "<project_root>/<project>.uproject"

# 5. 等待 MCP 端口 55557 开放（ping 轮询）
```

**Lyra 项目**：target 名 = `LyraEditor`（Source/LyraEditor.Target.cs）

---

## 四、版本兼容性适配清单

> 详细 API 差异见 `References/Notes/UE_Compat.md`

安装到新项目/新引擎版本前必查：

- [ ] 确认 UBT target 名：`ls <project>/Source/*.Editor.Target.cs`
- [ ] 确认引擎版本（`ENGINE_MAJOR_VERSION` / `ENGINE_MINOR_VERSION`）
- [ ] 检查 `ASkyAtmosphere` 头文件 → `Components/SkyAtmosphereComponent.h`
- [ ] 检查 `WorldSettings.h` → `GameFramework/WorldSettings.h`（无需版本条件）
- [ ] UE4 目标：Enhanced Input 已用 `#if MCP_ENHANCED_INPUT_SUPPORTED` 守卫
- [ ] VS2022 14.44+：`Build.cs` 中 `ShadowVariableWarningLevel = WarningLevel.Off`
- [ ] 新 `.cpp` 文件使用 `TActorIterator` 时必须 `#include "EngineUtils.h"`

**编译成功标志**：`Result: Succeeded`，编辑器启动后 `ping` 返回 `pong`。
