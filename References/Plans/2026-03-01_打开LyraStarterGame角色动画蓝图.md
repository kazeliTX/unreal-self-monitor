# 计划：打开LyraStarterGame角色动画蓝图

**日期**：2026-03-01
**目标**：启动虚幻编辑器并打开 LyraStarterGame 项目中角色的主要动画蓝图资产
**Git 分支**：ue/2026-03-01-open-lyra-anim-bp

## 涉及模块
- C++ 命令：`ping`、`get_capabilities`、`list_assets`、`find_asset`、`get_asset_info`
- Python 工具：`check_editor_status`、`launch_editor`、`wait_for_editor_ready`、`get_project_info`（system_tools、project_info_tools、asset_tools）

## 执行步骤
- [x] Step 1：检查编辑器状态 → 编辑器未运行，端口关闭
- [x] Step 2：启动 LyraStarterGame 编辑器（UE_5.6 Launcher）→ 10 秒内 MCP 端口开放
- [x] Step 3：等待编辑器就绪（ping pong 验证）
- [x] Step 4：验证 MCP 连接 → 110 命令可用，`"type"` 字段协议
- [x] Step 5：确认项目为 LyraStarterGame（EngineAssociation: "5.6"）
- [x] Step 6：`find_asset name=ABP_ asset_type=AnimBlueprint` → 19 个动画蓝图
- [x] Step 7：识别主角色蓝图 → `ABP_Mannequin_Base`（/Game/Characters/Heroes/Mannequin/Animations/）
- [x] Step 8：`get_asset_info` → class=AnimBlueprint，确认资产存在
- [x] Step 9：新增 `open_asset_editor` C++ 命令 → full_rebuild → 成功打开动画蓝图编辑器

## 验收标准
- [x] 编辑器成功启动并运行 LyraStarterGame 项目
- [x] MCP 连接正常，可执行命令（111 个命令）
- [x] 找到 19 个角色/武器动画蓝图资产
- [x] 获取到动画蓝图的完整路径和详细信息
- [x] 在编辑器中成功打开 ABP_Mannequin_Base

## 风险点
1. **未知资产名称**：不清楚 LyraStarterGame 中角色动画蓝图的具体名称和路径
2. **无打开命令**：当前 MCP 可能没有直接打开资产编辑器的命令
3. **编辑器启动时间**：启动大型项目可能需要较长时间
4. **MCP 兼容性**：LyraStarterGame 项目可能未安装或配置 MCP 插件
5. **临时关卡弹框**：如果启动时编辑器打开的是临时关卡，可能触发保存弹框

## 备用方案
- 如果无法直接打开动画蓝图，提供完整的资产路径供用户在编辑器中手动打开
- 使用 `filter_class: "AnimBlueprint"` 或 `filter_class: "Blueprint"` 缩小搜索范围
- 考虑搜索 "Hero"、"Character"、"Player" 等关键词查找角色相关蓝图