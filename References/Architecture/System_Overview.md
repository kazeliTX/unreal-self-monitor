# UnrealMCP 系统架构概览

> 本文件为架构快照，详细内容见 CLAUDE.md

## 整体数据流

```
AI Client (Claude / Cursor / Windsurf)
    │  MCP Protocol (stdio)
    ▼
unreal_mcp_server.py  [FastMCP]
    │  TCP JSON (port 55557)
    ▼
UnrealMCPBridge  [UEditorSubsystem]
    │
    ├─ ping / get_capabilities / batch  [内置]
    └─ FMCPCommandRegistry
         ├─ EditorCommands
         ├─ BlueprintCommands
         ├─ BlueprintNodeCommands
         ├─ UMGCommands
         ├─ ProjectCommands
         ├─ LevelCommands
         ├─ AssetCommands
         └─ DiagnosticsCommands
```

## 关键设计原则

1. **命令注册表模式**：新增命令无需修改路由逻辑，只需注册
2. **Python 工具自动发现**：`xxx_tools.py` + `register_xxx_tools(mcp)` 即可自动挂载
3. **每命令独立 TCP 连接**：无状态，简单可靠
4. **错误格式统一**：`{"success": false, "message": "..."}` 或 `{"status": "error", "error": "..."}`

## 实现进度

| 阶段 | 状态 |
|------|------|
| Phase 1 基础扩展 | ✅ 完成 |
| Phase 2A 诊断系统 | ✅ 完成 |
| Phase 2B TestCommands | 待实现 |
| Phase 2C/D 动画/材质 | 待实现 |

## 相关文档

- [Phase 1 扩展计划](../Plans/Phase1_扩展计划.md)
- [Phase 2 自愈诊断计划](../Plans/Phase2_自愈诊断计划.md)
- [SOP 标准工作流](../SOP/UE_Automation_SOP.md)
