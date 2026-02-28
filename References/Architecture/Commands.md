# MCP 命令全表（当前 117 条）

> 按需加载。最新命令数以 `get_capabilities` 返回为准。
> 内置命令：`ping` / `get_capabilities` / `batch`

---

## EditorCommands

**Actor**：`get_actors_in_level`、`find_actors_by_name`、`spawn_actor`、`delete_actor`、`set_actor_transform`、`get_actor_properties`、`set_actor_property`、`spawn_blueprint_actor`、`duplicate_actor`

**视口/选择**：`focus_viewport`、`take_screenshot`、`select_actor`、`deselect_all`、`get_selected_actors`

**标签/层级**：`set_actor_label`、`get_actor_label`、`add_actor_tag`、`remove_actor_tag`、`get_actor_tags`、`attach_actor_to_actor`、`detach_actor`

**世界**：`get_world_settings`、`set_world_settings`

### spawn_actor 支持的 actor_type

| actor_type | 备注 |
|------------|------|
| `StaticMeshActor` | 默认 |
| `PointLight` / `SpotLight` / `DirectionalLight` | 光源 |
| `CameraActor` | 摄像机 |
| `SkyLight` / `SkyAtmosphere` / `ExponentialHeightFog` | 环境（UE4.26+） |

⚠️ **DirectionalLight 陷阱**：创建后必须显式设 `bAtmosphereSunLight=true`，否则 SkyAtmosphere 不生效，场景渲染为黑色。

### set_actor_property 组件回落机制

先在 Actor 找属性；找不到时遍历所有 `UActorComponent`。响应含 `"component"` 字段说明找到的组件名。

---

## BlueprintCommands

**管理**：`create_blueprint`、`add_component_to_blueprint`、`set_static_mesh_properties`、`set_physics_properties`、`compile_blueprint`、`set_blueprint_property`、`set_pawn_properties`

**碰撞**：`set_component_collision_profile`、`set_component_collision_enabled`、`set_component_property`

**查询**：`get_blueprint_variables`、`get_blueprint_functions`、`get_blueprint_components`、`list_blueprints`、`get_blueprint_compile_errors`、`validate_blueprint`

---

## BlueprintNodeCommands

`add_blueprint_event_node`、`add_blueprint_input_action_node`、`add_blueprint_function_node`、`connect_blueprint_nodes`、`add_blueprint_variable`、`add_blueprint_get_self_component_reference`、`add_blueprint_self_reference`、`find_blueprint_nodes`、`add_blueprint_get_variable_node`、`add_blueprint_set_variable_node`、`add_blueprint_branch_node`、`add_blueprint_sequence_node`、`add_blueprint_cast_node`、`add_blueprint_math_node`、`add_blueprint_print_string_node`、`add_blueprint_custom_function`

---

## UMGCommands

`create_umg_widget_blueprint`、`add_text_block_to_widget`、`add_button_to_widget`、`bind_widget_event`、`add_widget_to_viewport`、`set_text_block_binding`、`add_image_to_widget`、`add_progress_bar_to_widget`、`add_horizontal_box_to_widget`、`add_vertical_box_to_widget`、`set_widget_visibility`、`set_widget_anchor`、`update_text_block_text`、`get_widget_tree`

**参数约定**：C++ 侧用 `blueprint_name` + `widget_name`；Python 侧第一参数 `widget_name` 映射为 `blueprint_name`。

---

## LevelCommands

`new_level`、`open_level`、`save_current_level`、`save_all_levels`、`get_current_level_name`、`get_level_dirty_state`、`run_level_validation`

**推荐**：始终用 `safe_switch_level` 代替直接调 `new_level`/`open_level`（防弹框）。

---

## AssetCommands

**浏览**：`list_assets`、`find_asset`、`does_asset_exist`、`get_asset_info`

**文件夹**：`create_folder`、`list_folders`、`delete_folder`

**操作**：`duplicate_asset`、`rename_asset`、`delete_asset`、`save_asset`、`save_all_assets`

**DataTable**：`create_data_table`、`add_data_table_row`、`get_data_table_rows`

**编辑器**：`open_asset_editor` — 在编辑器中打开任意资产（参数：`asset_path`）

---

## MaterialCommands

`create_material`、`set_material_property`（blend_mode/shading_model/two_sided）、`add_material_expression`（Constant/Constant3Vector/Constant4Vector/Fresnel/Multiply/Add/Lerp）、`connect_material_property`、`connect_material_expressions`、`compile_material`

---

## ProjectCommands

`create_input_mapping`、`run_console_command`、`get_project_settings`

UE5 专属（Enhanced Input）：`create_input_action`、`create_input_mapping_context`、`add_input_mapping`、`set_input_action_type`

---

## DiagnosticsCommands

**视觉感知**：`get_viewport_camera_info`、`get_actor_screen_position`、`highlight_actor`

**热重载**：`trigger_hot_reload`、`get_live_coding_status`

**源文件**：`get_source_file`、`modify_source_file`（自动 `.bak.时间戳` 备份）

**路径**：`get_engine_path`

---

## Python 工具模块

| 文件 | 主要工具 |
|------|---------|
| `base.py` | `send_unreal_command()`、`make_error()` |
| `editor_tools.py` | Actor/视口/选择/标签/附着/WorldSettings |
| `blueprint_tools.py` | BP 创建/组件/属性/编译/碰撞/查询 |
| `node_tools.py` | 节点图操作 |
| `umg_tools.py` | Widget Blueprint |
| `project_tools.py` | 输入/控制台/项目设置 |
| `level_tools.py` | 关卡管理（含 `safe_switch_level`） |
| `asset_tools.py` | 资产/DataTable |
| `log_tools.py` | UE 日志读取分析 |
| `diagnostics_tools.py` | 截图/相机/Actor屏幕坐标 |
| `compile_tools.py` | 源码读写/热重载（4层）/UBT/kill_editor/full_rebuild |
| `system_tools.py` | 编辑器进程管理 |
| `project_info_tools.py` | get_project_info / check_mcp_compatibility |
