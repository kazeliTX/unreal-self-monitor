# UnrealMCP 自动化安装 SOP

> 当用户要求「将 UnrealMCP 安装到某个项目」时，Claude 必须先执行本 SOP 的环境读取流程，再产出针对性方案。

---

## 流程总览

```
用户提供目标项目路径
        │
        ▼
① 读取工程信息
        │
        ▼
② 读取引擎信息
        │
        ▼
③ 生成兼容性报告
        │
        ▼
④ 产出定制化安装方案（含警告）
        │
        ▼
⑤ 用户确认后执行安装
        │
        ▼
⑥ 验证安装结果
```

---

## Step 1：读取工程信息

给定路径后，**必须先读取**以下内容，再做任何决策：

```bash
# 1. 找到 .uproject 文件
find <project_path> -maxdepth 2 -name "*.uproject"

# 2. 读取 .uproject
cat <project>.uproject
```

**从 .uproject 提取**：

| 字段 | 含义 | 决策影响 |
|------|------|---------|
| `EngineAssociation` | `""` = 源码编译；`"5.5"` = Launcher | 引擎路径查找策略 |
| `Plugins[].Name` | 已有插件名 | 检查 `UnrealMCP` 是否已安装 |
| `Modules[].Type` | 模块类型分布 | 了解项目规模 |

---

## Step 2：读取引擎信息

### 源码编译（EngineAssociation = ""）
```bash
# 引擎在相邻目录，读取版本文件
cat <project_root_parent>/Engine/Build/Build.version
```

### Launcher 安装（EngineAssociation = "5.5" 等）

**Windows 注册表路径**（按优先级）：
```
HKLM\SOFTWARE\EpicGames\Unreal Engine\<version>\InstalledDirectory
HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds  （自定义安装映射）
```

**Python 读取**：
```python
import winreg, json
key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
    r"SOFTWARE\EpicGames\Unreal Engine\5.5")
engine_path, _ = winreg.QueryValueEx(key, "InstalledDirectory")
```

**从 Build.version 提取**：
```json
{
  "MajorVersion": 4,   // 或 5
  "MinorVersion": 24,
  "PatchVersion": 2,
  "BranchName": "++UE4+Release-4.24"
}
```

---

## Step 3：生成兼容性报告

读取完工程和引擎信息后，**必须输出**以下报告再继续：

```
═══════════════════════════════════════
UnrealMCP 安装兼容性报告
═══════════════════════════════════════
工程路径    : <project_root>/<ue4_project>
.uproject   : <project>.uproject
引擎类型    : 源码编译（EngineAssociation=""）
引擎版本    : UE 4.24.2
引擎路径    : <project_root>/Engine

──────────────────────────────────────
兼容性检查
──────────────────────────────────────
✅ UEditorSubsystem     支持（UE4.22+）
✅ UToolMenus           支持（UE4.22+）
✅ UDeveloperSettings   支持
⚠️ BlueprintEditorLibrary  UE4 不可用，Build.cs 将条件排除
⚠️ Enhanced Input          UE4 不可用，ProjectCommands 相关命令禁用
⚠️ FAppStyle               UE4 需替换为 FEditorStyle（已由兼容宏处理）
❌ LiveCoding（<4.22）      不适用（4.24 可用）

──────────────────────────────────────
安装动作预览
──────────────────────────────────────
① 复制插件  → <project_root>/<ue4_project>/Plugins/UnrealMCP/
② 修补      → <project>.uproject（注入 UnrealMCP 插件引用）
③ 生成 mcp.json → [由用户指定 MCP client]
④ 重新生成 VS 项目文件
   → <project_root>/GenerateProjectFiles.bat <project>.uproject

⚠️ 注意：安装后需要在 Visual Studio 中重新编译插件
═══════════════════════════════════════
```

---

## Step 4：产出定制化安装方案

根据兼容性报告，定制 install.py 调用参数或手动步骤清单：

### UE4 源码编译工程（如 UE4 源码编译工程）

```bash
python Python/scripts/install.py \
  "<project_root>/<ue4_project>/<project>.uproject" \
  --mcp-client claude \
  --engine-root "<project_root>"
```

安装脚本将自动：
- 选择 UE4 兼容的 Build.cs 分支
- 在 .uplugin 中使用 `WhitelistPlatforms`（UE4 兼容字段）
- 排除 `BlueprintEditorLibrary` 和 Enhanced Input 依赖

### UE5 Launcher 工程

```bash
python Python/scripts/install.py \
  "C:/MyProject/MyProject.uproject" \
  --mcp-client claude
```

---

## Step 5：执行安装

**在 install.py 可用之前（Phase A 未完成时）**，手动执行等效步骤：

```bash
# 1. 复制插件
cp -r MCPGameProject/Plugins/UnrealMCP/ <target>/Plugins/UnrealMCP/

# 2. 修补 .uproject（Python 单行）
python -c "
import json, pathlib
p = pathlib.Path('<target>.uproject')
d = json.loads(p.read_text())
plugins = d.setdefault('Plugins', [])
if not any(pl['Name'] == 'UnrealMCP' for pl in plugins):
    plugins.append({'Name': 'UnrealMCP', 'Enabled': True})
p.write_text(json.dumps(d, indent='\t'))
print('Patched.')
"

# 3. 生成 VS 项目文件（源码编译工程）
"<project_root>/GenerateProjectFiles.bat" \
  "<project_root>/<ue4_project>/<project>.uproject"

# 4. 生成 mcp.json
python -c "
import json, pathlib
cfg = {'mcpServers': {'unrealMCP': {'command': 'uv', 'args': [
    '--directory', str(pathlib.Path('Python').resolve()),
    'run', 'unreal_mcp_server.py']}}}
pathlib.Path('mcp.json').write_text(json.dumps(cfg, indent=2))
print('mcp.json written.')
"
```

---

## Step 6：验证安装

```bash
# 编译验证（UE4 源码编译工程）
"<project_root>/Engine/Build/BatchFiles/Build.bat" \
  <ProjectName>Editor Win64 Development \
  "<project_root>/<ue4_project>/<project>.uproject"

# 运行时验证（编辑器打开后）
python Python/scripts/debug_runner.py --smoke-test
```

---

## 兼容性速查表

| UE 版本 | 构建类型 | 引擎路径来源 | 已知限制 |
|---------|---------|------------|---------|
| 4.22–4.27 | 源码 | 相邻 `Engine/` 目录 | 无 BlueprintEditorLibrary、Enhanced Input |
| 4.22–4.27 | Launcher | 注册表 | 同上 |
| 5.0–5.1 | Launcher | 注册表 | Enhanced Input 早期 API 差异 |
| 5.2–5.4 | Launcher | 注册表 | 基本完整支持 |
| 5.5+ | Launcher/源码 | 注册表 | 完整支持（当前基准） |

---

## 注意事项

1. **安装前先备份 .uproject** — 修补操作会改写原文件
2. **源码编译工程必须重新生成 VS 工程文件** — 否则新插件不会被 UBT 识别
3. **UE4 下 Enhanced Input 相关 MCP 命令不可用** — 使用 `get_capabilities` 查看实际可用命令
4. **首次编译时间较长** — UE4 大型项目（如 UE4 源码编译工程）可能需要 10–30 分钟
