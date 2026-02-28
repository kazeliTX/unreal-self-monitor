# 虚幻引擎 MCP 全自动化扩展计划

> 基于 `unreal-mcp` 现有工程，面向 AI 驱动的 Unreal Editor 全自动化操作目标的能力扩展与架构优化方案。
>
> 文档日期：2026-02-27（v2.0 - 基于实际代码全面修订）

---

## 一、现状评估（实际代码分析）

### 1.1 已实现命令清单（基线 v1）

#### C++ 已注册命令（35 个）

| 模块文件 | 已注册命令 |
|----------|-----------|
| **EditorCommands** | `get_actors_in_level`, `find_actors_by_name`, `spawn_actor`, `create_actor`（别名）, `delete_actor`, `set_actor_transform`, `get_actor_properties`, `set_actor_property`, `spawn_blueprint_actor`, `focus_viewport`, `take_screenshot` |
| **BlueprintCommands** | `create_blueprint`, `add_component_to_blueprint`, `set_component_property`, `set_physics_properties`, `compile_blueprint`, `set_static_mesh_properties`, `set_blueprint_property` |
| **BlueprintNodeCommands** | `connect_blueprint_nodes`, `add_blueprint_get_self_component_reference`, `add_blueprint_event_node`, `add_blueprint_function_node`, `add_blueprint_variable`, `add_blueprint_input_action_node`, `add_blueprint_self_reference`, `find_blueprint_nodes` |
| **UMGCommands** | `create_umg_widget_blueprint`, `add_text_block_to_widget`, `add_widget_to_viewport`, `add_button_to_widget`, `bind_widget_event`, `set_text_block_binding` |
| **ProjectCommands** | `create_input_mapping`（旧 InputSettings API） |
| **内置（Bridge）** | `ping`, `get_capabilities`, `batch` |

#### Python MCP 工具（与 C++ 对应）
- `editor_tools.py`：11 个工具（与 C++ EditorCommands 对应）
- `blueprint_tools.py`：7 个工具
- `node_tools.py`：8 个工具
- `umg_tools.py`：6 个工具
- `project_tools.py`：1 个工具

### 1.2 已完成架构优化
- ✅ 命令注册表模式（FMCPCommandRegistry，替换 if-else 链）
- ✅ 批处理协议（`batch` 内置命令）
- ✅ 资产路径参数化（`path`/`asset_path` 可选参数）
- ✅ Python 工具自动发现注册（auto_register_tools + pkgutil）
- ✅ 公共 helper（`send_unreal_command`, `make_error`）

### 1.3 已知缺陷与 Gap

| 类别 | 缺口描述 |
|------|---------|
| **Actor 操作** | 无选中/取消选中，无复制Actor，无父子附加关系，无Label设置，无Tag操作 |
| **蓝图节点** | 无 Branch/Cast/ForEach 节点，无 Get/Set 变量节点，无数学节点，无自定义函数创建 |
| **蓝图查询** | 无法列出蓝图变量、函数、组件树 |
| **关卡管理** | 无新建/打开/保存关卡命令 |
| **资产管理** | 完全缺失（无 list/find/import/save/rename/delete asset） |
| **UMG 控件** | 仅 TextBlock + Button，缺 Image/ProgressBar/ScrollBox/CheckBox/Slider 等 |
| **材质系统** | 完全缺失 |
| **动画系统** | 完全缺失 |
| **Niagara** | 完全缺失 |
| **输入系统** | 仅旧 InputSettings API，缺 Enhanced Input 资产创建 |
| **PIE 控制** | 无法启动/停止/暂停 Play In Editor |
| **Data Table** | 完全缺失（游戏数据工作流核心） |
| **World Settings** | 无法读写 WorldSettings（GameMode、重力等） |
| **Collision** | 无法设置碰撞配置 |
| **通用逃生口** | 无 `run_console_command` 万能命令 |

---

## 二、待扩展能力详细规划

### 模块 A：资产管理系统（Asset Management）

#### A-1 资产注册表查询

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `list_assets` | 列出指定路径下资产，支持 Class 过滤、递归 | P0 |
| `find_asset` | 按名称/路径精确或模糊查找资产，返回元信息 | P0 |
| `get_asset_info` | 获取资产详细信息（类型、路径、依赖引用数）| P0 |
| `does_asset_exist` | 检查资产是否存在于指定路径 | P0 |
| `get_asset_references` | 查找所有引用了某资产的其他资产 | P1 |
| `get_asset_dependencies` | 获取某资产依赖的所有资产列表 | P1 |

#### A-2 资产文件夹管理

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `create_folder` | 在 Content Browser 中创建目录 | P0 |
| `list_folders` | 列出指定路径的子目录 | P0 |
| `delete_folder` | 删除目录（可选递归）| P1 |
| `rename_folder` | 重命名目录 | P1 |

#### A-3 资产生命周期操作

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `duplicate_asset` | 复制资产到指定路径 | P0 |
| `rename_asset` | 重命名资产（自动修复引用）| P0 |
| `move_asset` | 移动资产到新路径（自动修复引用）| P0 |
| `delete_asset` | 删除资产（可选检查引用安全性）| P0 |
| `save_asset` | 保存指定资产 | P0 |
| `save_all_assets` | 保存所有未保存资产 | P0 |

#### A-4 资产导入/导出

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `import_asset` | 导入外部文件（FBX/OBJ/PNG/WAV/CSV 等）| P0 |
| `reimport_asset` | 对已有资产重新执行导入 | P1 |
| `export_asset` | 将资产导出为外部格式 | P2 |
| `batch_import` | 批量导入目录中的资产 | P1 |

#### A-5 资产元数据

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `get_asset_tags` | 读取资产标签 | P2 |
| `set_asset_tags` | 写入/修改资产标签 | P2 |
| `get_asset_metadata` | 读取自定义元数据 Key-Value | P2 |
| `set_asset_metadata` | 写入自定义元数据 | P2 |

---

### 模块 B：蓝图自动化扩展（Blueprint Automation）

#### B-1 蓝图信息查询（高优先级，AI 规划必需）

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `get_blueprint_variables` | 获取蓝图所有变量列表及类型 | **P0** |
| `get_blueprint_functions` | 获取蓝图所有函数/事件列表 | **P0** |
| `get_blueprint_components` | 获取蓝图组件树（名称、类型、父子关系）| **P0** |
| `list_blueprints` | 列出指定路径下的所有蓝图资产 | **P0** |
| `get_blueprint_parent_class` | 获取蓝图父类信息 | P1 |
| `get_blueprint_compile_errors` | 获取蓝图编译错误列表 | **P0** |
| `get_blueprint_interfaces` | 列出蓝图实现的所有接口 | P1 |

#### B-2 蓝图节点图增强

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `add_blueprint_get_variable_node` | 添加 Get Variable 节点（读取变量值）| **P0** |
| `add_blueprint_set_variable_node` | 添加 Set Variable 节点（设置变量值）| **P0** |
| `add_blueprint_branch_node` | 添加 Branch（If/Else）节点 | **P0** |
| `add_blueprint_sequence_node` | 添加 Sequence 节点（顺序执行）| **P0** |
| `add_blueprint_cast_node` | 添加 Cast To 节点 | **P0** |
| `add_blueprint_math_node` | 添加数学运算节点（Add/Sub/Mul/Div/Clamp 等）| **P0** |
| `add_blueprint_print_string_node` | 添加 Print String 调试节点 | **P0** |
| `add_blueprint_custom_function` | 在蓝图中创建新的自定义函数图 | **P0** |
| `add_blueprint_for_each_node` | 添加 ForEachLoop 节点 | P1 |
| `add_blueprint_delay_node` | 添加 Delay 节点（延迟执行）| P1 |
| `add_blueprint_interface_message_node` | 调用接口函数节点 | P1 |
| `add_blueprint_timeline_node` | 添加 Timeline 节点 | P1 |
| `add_blueprint_local_variable` | 在函数图中添加局部变量 | P1 |
| `add_blueprint_macro_node` | 引用内置或自定义宏 | P1 |
| `add_blueprint_comment` | 在节点图中添加注释框（组织布局）| P2 |
| `set_node_position` | 设置节点在图中的坐标（布局整理）| P1 |
| `auto_layout_nodes` | 自动整理节点图布局 | P2 |

#### B-3 蓝图库与接口资产

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `create_blueprint_function_library` | 创建蓝图函数库 | P1 |
| `create_blueprint_interface` | 创建蓝图接口资产 | P1 |
| `add_blueprint_interface_impl` | 为蓝图添加 Interface 实现 | P1 |
| `create_blueprint_macro_library` | 创建宏库 | P2 |

#### B-4 数据资产

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `create_struct_asset` | 创建 Struct 资产并添加字段 | P1 |
| `create_enum_asset` | 创建 Enum 资产并添加枚举值 | P1 |
| `create_data_asset` | 创建 DataAsset（基于 UPrimaryDataAsset 子类）| P1 |

#### B-5 动画蓝图（Animation Blueprint）

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `create_animation_blueprint` | 创建动画蓝图（指定骨骼、父类）| P0 |
| `add_anim_state_machine` | 在 AnimGraph 中添加状态机节点 | P0 |
| `add_anim_state` | 在状态机中添加状态并关联动画 | P0 |
| `add_anim_transition` | 添加状态间转换并设置条件 | P0 |
| `add_anim_sequence_node` | 添加播放单个动画序列的节点 | P0 |
| `add_anim_blend_space_node` | 在 AnimGraph 中引用 BlendSpace 资产 | P1 |
| `set_anim_blueprint_variable` | 设置动画蓝图变量（用于状态机条件）| P0 |
| `set_default_slot_node` | 添加 DefaultSlot 节点（用于 Montage 播放）| P1 |
| `add_layered_blend_per_bone` | 添加按骨骼分层混合节点 | P2 |
| `compile_animation_blueprint` | 编译动画蓝图 | P0 |

---

### 模块 C：资产处理系统（Asset Processing）

#### C-1 材质系统（Material）

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `create_material` | 创建材质资产，设置混合模式、着色模型 | P0 |
| `add_material_node` | 添加材质节点（Texture Sample/Parameter/Math 等）| P0 |
| `connect_material_nodes` | 连接材质节点引脚 | P0 |
| `set_material_node_value` | 设置材质节点常量值 | P0 |
| `create_material_instance` | 基于 Master 材质创建材质实例 | P0 |
| `set_material_instance_parameter` | 设置材质实例的标量/向量/纹理参数值 | P0 |
| `get_material_parameters` | 获取材质/实例的所有可暴露参数 | P1 |
| `create_material_parameter_collection` | 创建全局材质参数集合 | P1 |
| `compile_material` | 触发材质编译 | P1 |
| `assign_material_to_mesh` | 将材质分配给静态/骨骼网格的指定槽位 | P0 |

#### C-2 Niagara 粒子系统

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `create_niagara_system` | 创建 Niagara 粒子系统资产 | P1 |
| `add_niagara_emitter` | 向系统添加 Emitter | P1 |
| `set_niagara_emitter_property` | 设置 Emitter 属性（spawn rate、lifetime 等）| P1 |
| `set_niagara_system_variable` | 设置系统级暴露变量的默认值 | P1 |
| `compile_niagara_system` | 编译 Niagara 系统 | P1 |
| `create_niagara_parameter_collection` | 创建 Niagara 参数集合 | P2 |

#### C-3 动画资产处理

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `create_blend_space_1d` | 创建一维混合空间 | P0 |
| `create_blend_space_2d` | 创建二维混合空间 | P0 |
| `add_blend_space_sample` | 向混合空间添加动画样本（轴值 + 动画序列）| P0 |
| `set_blend_space_axis` | 设置混合空间轴参数（名称/范围/插值方式）| P0 |
| `create_animation_montage` | 基于动画序列创建 Montage | P0 |
| `add_montage_section` | 添加 Montage Section 并设置起始时间 | P0 |
| `add_anim_notify` | 在动画资产时间轴添加 AnimNotify | P1 |
| `add_anim_notify_state` | 添加有时间范围的 AnimNotifyState | P1 |
| `get_skeleton_bones` | 获取骨骼资产的骨骼层级结构 | P0 |
| `add_skeleton_socket` | 在骨骼上添加 Socket（位置/旋转偏移）| P1 |
| `set_animation_compression` | 设置动画压缩算法和精度 | P2 |
| `create_aim_offset` | 创建 Aim Offset 资产 | P2 |
| `retarget_animation` | 执行动画重定向（源骨骼 → 目标骨骼）| P2 |

#### C-4 纹理与图像处理

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `set_texture_settings` | 修改纹理导入设置（压缩/Mip/sRGB 等）| P1 |
| `create_render_target` | 创建 Render Target 资产 | P1 |
| `create_texture_from_color` | 程序化创建纯色/渐变纹理 | P2 |

#### C-5 声音资产

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `create_sound_cue` | 创建 Sound Cue 资产 | P2 |
| `set_sound_attenuation` | 设置声音衰减属性 | P2 |
| `create_sound_class` | 创建 Sound Class 资产 | P2 |

#### C-6 Data Table 系统（游戏数据工作流核心）

> Data Table 是 AI 自动生成游戏内容的关键路径：配置表、掉落表、对话表等均依赖它。

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `create_data_table` | 创建 DataTable 资产（指定 Struct 类型）| **P0** |
| `add_data_table_row` | 向 DataTable 添加一行数据（JSON 键值）| **P0** |
| `get_data_table_rows` | 读取 DataTable 所有行，返回 JSON 数组 | **P0** |
| `update_data_table_row` | 更新 DataTable 指定行的字段值 | P1 |
| `delete_data_table_row` | 删除 DataTable 指定行 | P1 |
| `import_csv_to_data_table` | 将 CSV 文件批量导入 DataTable | P1 |
| `export_data_table_to_csv` | 导出 DataTable 为 CSV | P2 |

#### C-7 Sequencer / 过场动画

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `create_level_sequence` | 创建 Level Sequence 资产 | P2 |
| `add_actor_to_sequence` | 向 Sequence 添加 Actor 轨道 | P2 |
| `add_transform_keyframe` | 为 Actor 轨道添加位移关键帧 | P2 |
| `add_event_track` | 添加事件轨道并关联蓝图函数 | P2 |
| `set_sequence_length` | 设置 Sequence 总时长 | P2 |
| `play_level_sequence` | 在编辑器中预览播放 Sequence | P2 |

---

### 模块 D：常见编辑器操作（Editor Operations）

#### D-1 关卡管理

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `new_level` | 新建关卡（空白 / 默认模板）| **P0** |
| `open_level` | 打开指定路径的关卡 | **P0** |
| `save_current_level` | 保存当前关卡 | **P0** |
| `save_all_levels` | 保存所有已修改关卡 | **P0** |
| `get_current_level_name` | 获取当前打开的关卡名称及路径 | **P0** |
| `add_sub_level` | 添加 Sub-Level（关卡流送）| P1 |
| `set_sub_level_visible` | 切换 Sub-Level 可见性 | P1 |

#### D-2 Actor 选中与编辑

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `select_actor` | 选中指定 Actor（单选/多选）| **P0** |
| `deselect_all` | 取消所有选中 | **P0** |
| `get_selected_actors` | 获取当前选中的 Actor 列表 | **P0** |
| `duplicate_actor` | 复制 Actor 到指定位置 | **P0** |
| `set_actor_label` | 设置 Actor 在编辑器中显示的 Label（不同于内部名）| **P0** |
| `get_actor_label` | 获取 Actor 的显示 Label | **P0** |
| `attach_actor_to_actor` | 将 Actor 附加到另一个 Actor（父子关系）| **P0** |
| `detach_actor` | 断开 Actor 的父子附加关系 | P1 |
| `group_actors` | 将多个 Actor 编组 | P1 |
| `add_actor_tag` | 给 Actor 添加 Tag（用于 ActorHasTag 逻辑）| **P0** |
| `remove_actor_tag` | 移除 Actor 的指定 Tag | P1 |
| `get_actor_tags` | 获取 Actor 所有 Tags | P1 |
| `align_actors_to_surface` | 将 Actor 对齐到表面（放置辅助）| P2 |
| `snap_actors_to_grid` | 吸附 Actor 到网格 | P2 |

#### D-3 组件 Collision 设置

> 碰撞配置是游戏逻辑的基础，AI 创建 Actor 后必须能设置碰撞。

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `set_component_collision_profile` | 设置组件的碰撞预设（NoCollision/BlockAll/OverlapAll 等）| **P0** |
| `set_component_collision_response` | 设置组件对特定 Channel 的碰撞响应 | P1 |
| `set_component_collision_enabled` | 启用/禁用组件碰撞 | **P0** |
| `get_component_collision_info` | 获取组件当前碰撞配置 | P1 |

#### D-4 World Settings

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `get_world_settings` | 获取当前关卡的 WorldSettings（GameMode、重力等）| **P0** |
| `set_world_settings` | 设置 WorldSettings 属性 | **P0** |
| `set_default_game_mode` | 设置关卡的默认 GameMode | **P0** |
| `set_gravity` | 设置世界重力（Z 轴加速度）| P1 |

#### D-5 PIE（Play In Editor）控制

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `start_pie` | 启动 Play In Editor | P1 |
| `stop_pie` | 停止 Play In Editor | P1 |
| `pause_pie` | 暂停/恢复 PIE | P1 |
| `enter_simulate_mode` | 进入 Simulate 模式 | P1 |

#### D-6 构建与烘焙

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `build_lighting` | 触发光照构建（质量可选）| P1 |
| `get_build_status` | 查询光照/导航构建进度 | P1 |
| `build_navigation` | 构建导航网格 | P1 |
| `invalidate_lighting` | 使光照缓存失效（重置）| P2 |

#### D-7 项目设置与配置

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `get_project_settings` | 读取指定模块的项目设置 | P1 |
| `set_project_settings` | 写入项目设置 | P1 |
| `get_config_value` | 读取 .ini 配置文件的指定键值 | P1 |
| `set_config_value` | 写入 .ini 配置文件 | P1 |
| `get_plugin_list` | 列出所有插件及其启用状态 | P2 |
| `enable_plugin` | 启用指定插件 | P2 |
| `run_console_command` | **执行任意编辑器控制台命令**（万能逃生口）| **P0** |

#### D-8 Enhanced Input 系统

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `create_input_action` | 创建 Enhanced Input Action 资产 | **P0** |
| `create_input_mapping_context` | 创建 Input Mapping Context 资产 | **P0** |
| `add_input_mapping` | 向 IMC 中添加按键绑定 | **P0** |
| `set_input_action_type` | 设置 Input Action 的值类型（bool/axis1d/axis2d 等）| **P0** |

#### D-9 World Partition / HLOD

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `enable_world_partition` | 启用/禁用 World Partition | P2 |
| `set_hlod_settings` | 配置 HLOD 层级参数 | P2 |

---

### 模块 E：UMG 控件扩展

> 当前只有 TextBlock + Button，AI 构建完整 UI 需要更多控件类型。

| 命令名 | 说明 | 优先级 |
|--------|------|--------|
| `add_image_to_widget` | 添加 Image 控件（可绑定 Texture2D/材质）| **P0** |
| `add_progress_bar_to_widget` | 添加 Progress Bar 控件（HP条/加载条等）| **P0** |
| `add_horizontal_box_to_widget` | 添加水平布局容器 | **P0** |
| `add_vertical_box_to_widget` | 添加垂直布局容器 | **P0** |
| `add_scroll_box_to_widget` | 添加可滚动容器 | P1 |
| `add_check_box_to_widget` | 添加 CheckBox 控件 | P1 |
| `add_slider_to_widget` | 添加 Slider 滑动条控件 | P1 |
| `add_editable_text_to_widget` | 添加可编辑文本框（输入框）| P1 |
| `add_border_to_widget` | 添加 Border 容器（带背景色/图片）| P1 |
| `add_overlay_to_widget` | 添加 Overlay 叠加容器 | P1 |
| `add_size_box_to_widget` | 添加 SizeBox（固定尺寸容器）| P2 |
| `set_widget_visibility` | 设置控件可见性（Visible/Hidden/Collapsed）| **P0** |
| `set_widget_anchor` | 设置控件的锚点和对齐方式 | **P0** |
| `update_text_block_text` | 动态更新 TextBlock 的文字内容 | **P0** |
| `set_image_texture` | 设置 Image 控件的纹理资产 | P1 |
| `set_progress_bar_percent` | 设置 ProgressBar 初始进度值 | P1 |
| `get_widget_tree` | 获取 Widget Blueprint 的控件层级结构 | **P0** |

---

## 三、架构优化设计（续）

### 3.1 尚未实现的架构优化（优先）

#### 3.1.1 异步任务支持（Async Commands）

对耗时操作（光照构建、资产导入、着色器编译等）引入异步执行机制：

```
AI Client                Python Server          C++ Plugin
    │                         │                      │
    │──── build_lighting ─────▶                      │
    │                         │──── async_exec ──────▶
    │◄─── {task_id: "abc123"} │                      │（后台执行）
    │                         │                      │
    │──── get_task_status ────▶                      │
    │     {task_id: "abc123"} │──── query ───────────▶
    │◄─── {progress: 45%} ────│◄─── {progress: 45%}  │
```

#### 3.1.2 Actor 查找缓存（FMCPActorCache）

当前所有 Actor 查找均为 O(n) 遍历，大场景性能差。

```cpp
class FMCPActorCache {
    TMap<FString, TWeakObjectPtr<AActor>> NameCache;
    // OnActorSpawned / OnActorDestroyed 委托自动维护缓存
};
```

#### 3.1.3 结构化错误码体系

统一 Python 返回格式，增加可机读的错误码：

```python
{
    "success": bool,
    "data": {...},
    "error": {
        "code": "ASSET_NOT_FOUND",
        "message": "Asset '/Game/...' not found",
        "details": {...}
    }
}
```

#### 3.1.4 Python 端高层复合工具

为 AI 提供高层工具，在 Python 端组合多个底层命令，减少 AI 规划负担：

```python
@mcp.tool()
def create_full_character_blueprint(name, mesh_path, anim_blueprint):
    """一步创建完整角色蓝图（BP + 胶囊 + Mesh + Camera + 动画蓝图绑定）"""
    ...

@mcp.tool()
def setup_game_mode(game_mode_class, pawn_class, hud_class):
    """设置关卡的 GameMode 及默认 Pawn/HUD"""
    ...
```

#### 3.1.5 工具分层（Low-Level / High-Level）

```
tools/
├── low_level/               # 与 C++ 一一对应的原子命令
│   ├── editor_tools.py      (已有，需扩展)
│   ├── blueprint_tools.py   (已有，需扩展)
│   ├── node_tools.py        (已有，需扩展)
│   ├── umg_tools.py         (已有，需扩展)
│   ├── project_tools.py     (已有，需扩展)
│   ├── asset_tools.py       ← 新增
│   ├── level_tools.py       ← 新增
│   ├── animation_tools.py   ← 新增
│   ├── material_tools.py    ← 新增
│   ├── niagara_tools.py     ← 新增
│   └── datatable_tools.py   ← 新增
│
└── high_level/              # 复合工具（AI 友好，减少多步调用）
    ├── character_tools.py   ← 新增（完整角色蓝图）
    ├── scene_tools.py       ← 新增（批量场景操作）
    ├── ui_tools.py          ← 新增（完整 UI 工作流）
    └── fx_tools.py          ← 新增（粒子特效工作流）
```

### 3.2 新增 C++ 命令模块规划

```
Source/UnrealMCP/Private/Commands/
├── UnrealMCPEditorCommands.cpp        (已有 → 扩展 Actor选中/Label/Tag/附加/关卡/WorldSettings/碰撞)
├── UnrealMCPBlueprintCommands.cpp     (已有 → 扩展查询接口/Data Table)
├── UnrealMCPBlueprintNodeCommands.cpp (已有 → 扩展 Branch/Cast/Get/Set变量/数学节点/自定义函数)
├── UnrealMCPUMGCommands.cpp           (已有 → 扩展更多控件类型/可见性/锚点)
├── UnrealMCPProjectCommands.cpp       (已有 → 扩展 Enhanced Input/配置读写/run_console_command)
│
├── UnrealMCPAssetCommands.cpp         ← 新增：资产增删改查/导入导出
├── UnrealMCPAnimationCommands.cpp     ← 新增：动画蓝图/BS/Montage/Notify/骨骼
├── UnrealMCPMaterialCommands.cpp      ← 新增：材质/材质实例/节点图
├── UnrealMCPNiagaraCommands.cpp       ← 新增：Niagara 粒子系统
├── UnrealMCPLevelCommands.cpp         ← 新增：关卡新建/打开/保存/Sub-Level
└── UnrealMCPSequencerCommands.cpp     ← 新增（P2）：Level Sequence/关键帧
```

---

## 四、优先级实施路线图

### Phase 1 — 核心缺口补全（P0，自动化工作流最小可用集）

**目标**：让 AI 能独立完成完整的游戏系统搭建工作流（无需手动操作 Editor）。

| 任务 | 涉及模块 | 新增文件 |
|------|---------|---------|
| **关卡管理**（new/open/save/get_current）| Editor | `UnrealMCPLevelCommands.cpp` + `level_tools.py` |
| **资产管理**（list/find/does_exist/save/rename/move/delete/import）| 资产 | `UnrealMCPAssetCommands.cpp` + `asset_tools.py` |
| **Data Table**（create/add_row/get_rows）| 数据 | 扩展 `UnrealMCPAssetCommands.cpp` + `datatable_tools.py` |
| **Actor 扩展**（select/deselect/duplicate/label/tag/attach）| Editor | 扩展 `UnrealMCPEditorCommands.cpp` |
| **World Settings**（get/set/set_game_mode）| Editor | 扩展 `UnrealMCPEditorCommands.cpp` |
| **Collision 设置**（set_collision_profile/enabled）| Editor | 扩展 `UnrealMCPBlueprintCommands.cpp` |
| **蓝图查询**（get_variables/functions/components/compile_errors）| Blueprint | 扩展 `UnrealMCPBlueprintCommands.cpp` |
| **蓝图节点扩展**（Get/Set变量、Branch、Cast、数学、PrintString、自定义函数）| 节点 | 扩展 `UnrealMCPBlueprintNodeCommands.cpp` |
| **UMG 扩展**（Image/ProgressBar/HBox/VBox/Visibility/Anchor/get_widget_tree）| UMG | 扩展 `UnrealMCPUMGCommands.cpp` |
| **run_console_command**（万能逃生口）| Project | 扩展 `UnrealMCPProjectCommands.cpp` |
| **Enhanced Input**（create_input_action/create_imc/add_mapping）| Project | 扩展 `UnrealMCPProjectCommands.cpp` |

### Phase 2 — 动画与材质（P0/P1，核心资产类型）

| 任务 | 涉及模块 |
|------|---------|
| 动画蓝图（create/state_machine/state/transition）| `UnrealMCPAnimationCommands.cpp` |
| BlendSpace（create_1d/2d + add_sample + set_axis）| `UnrealMCPAnimationCommands.cpp` |
| Montage（create + add_section + add_notify）| `UnrealMCPAnimationCommands.cpp` |
| 骨骼（get_bones + add_socket）| `UnrealMCPAnimationCommands.cpp` |
| 材质（create/add_node/connect/set_value）| `UnrealMCPMaterialCommands.cpp` |
| 材质实例（create_instance/set_parameter）| `UnrealMCPMaterialCommands.cpp` |
| assign_material_to_mesh | `UnrealMCPMaterialCommands.cpp` |
| 结构化错误码体系 | Python 架构 |
| High-Level 复合工具（character_tools）| Python 高层 |

### Phase 3 — 粒子与高级功能（P1/P2）

| 任务 | 涉及模块 |
|------|---------|
| Niagara 系统（create/emitter/property/compile）| `UnrealMCPNiagaraCommands.cpp` |
| PIE 控制（start/stop/pause）| 扩展 EditorCommands |
| 构建系统（build_lighting/navigation + 异步状态查询）| 扩展 EditorCommands |
| 项目设置读写（get/set_project_settings, get/set_config_value）| 扩展 ProjectCommands |
| Struct/Enum/DataAsset 资产创建 | 扩展 BlueprintCommands |
| 蓝图接口（create_interface + add_impl）| 扩展 BlueprintCommands |
| High-Level 复合工具（scene_tools, ui_tools, fx_tools）| Python 高层 |

### Phase 4 — 架构完善与高级特性（P2）

| 任务 |
|------|
| Actor 查找缓存（FMCPActorCache，O(n) → O(1)）|
| 异步任务系统（task_id + get_task_status）|
| 连接池与自动重连 |
| Sequencer（level_sequence/keyframe）|
| UMG 高级控件（CheckBox/Slider/Input/Border 等）|
| World Partition / HLOD 支持 |
| 纹理处理（set_texture_settings/create_render_target）|
| 声音资产（Sound Cue/Class/Attenuation）|
| 资产元数据（tags/metadata 读写）|

---

## 五、关键实现注意事项

### 5.1 UE API 模块依赖

新增命令模块需在 `UnrealMCP.Build.cs` 中添加对应模块：

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    // 已有
    "Core", "CoreUObject", "Engine", "UnrealEd", "Kismet",
    "BlueprintGraph", "UMGEditor", "UMG",
    // Phase 1 新增
    "AssetTools",           // 资产操作
    "AssetRegistry",        // 资产注册表查询
    "EditorAssetLibrary",   // 高级资产操作（已在部分文件中使用）
    "ContentBrowser",       // Content Browser 操作
    "LevelEditor",          // 关卡编辑器
    "EditorFramework",      // PIE 控制
    "EnhancedInput",        // Enhanced Input 系统
    // Phase 2 新增
    "AnimGraph",            // 动画蓝图 AnimGraph
    "AnimGraphRuntime",     // 动画蓝图运行时
    "MaterialEditor",       // 材质编辑器
    "SkeletalMeshEditor",   // 骨骼网格相关
    // Phase 3 新增
    "NiagaraEditor",        // Niagara 编辑器
    "MovieScene",           // Sequencer 基础
    "LevelSequence",        // Level Sequence
});
```

### 5.2 线程安全

所有涉及 UE 对象的操作必须在游戏线程（Game Thread）执行。当前 `MCPServerRunnable` 已通过 `AsyncTask(ENamedThreads::GameThread, ...)` 分发，新增命令遵守同样模式。

### 5.3 动画蓝图特殊性

动画蓝图的 `AnimGraph` 与普通蓝图的 `EventGraph` API 不同，需使用 `UAnimBlueprint`、`UAnimationGraph`、`UAnimGraphNode_*` 系列，不能复用现有 `K2Node_*` 逻辑。

### 5.4 材质节点图

材质图使用 `UMaterial`、`UMaterialExpression*` 系列（非 K2Node），需独立的节点创建和连接逻辑。

### 5.5 Actor Label vs Name

- `AActor::GetName()` → 内部唯一名（不可直接修改）
- `AActor::GetActorLabel()` / `FActorLabelUtilities::SetActorLabel()` → 编辑器显示名
- 新增命令应同时返回两者，AI 操作时使用 Label 进行人类友好的引用

### 5.6 run_console_command 安全边界

`run_console_command` 是极强的万能命令，但需要注意：
- 仅允许在编辑器模式下（非 PIE/Shipping）执行
- 记录所有执行日志便于审计
- 不允许执行 `quit` / `exit` 等破坏性命令（黑名单过滤）

### 5.7 Data Table Struct 限制

创建 Data Table 时必须提供 Struct 类型：
- 可使用 UE 内置 Struct（如 `FTableRowBase` 的子类）
- 或先通过 `create_struct_asset` 创建自定义 Struct，再创建 DataTable

---

## 六、能力扩展总览（命令数量统计）

| 模块 | 现有命令数 | Phase1 新增 | Phase2 新增 | Phase3+ 新增 | 最终合计 |
|------|-----------|------------|------------|-------------|---------|
| Actor / Editor 基础 | 11 | +12 | 0 | +4 | **27** |
| Blueprint | 7 | +7 | 0 | +6 | **20** |
| Blueprint 节点图 | 8 | +8 | 0 | +4 | **20** |
| UMG | 6 | +9 | 0 | +6 | **21** |
| **关卡管理** | 0 | **+6** | 0 | +2 | **8** |
| **资产管理** | 0 | **+15** | 0 | +5 | **20** |
| **Data Table** | 0 | **+5** | 0 | +2 | **7** |
| **动画系统** | 0 | 0 | **+16** | 0 | **16** |
| **材质系统** | 0 | 0 | **+10** | 0 | **10** |
| **Niagara 粒子** | 0 | 0 | 0 | **+6** | **6** |
| Project/Input/Config | 1 | +6 | 0 | +4 | **11** |
| Sequencer | 0 | 0 | 0 | **+6** | **6** |
| 架构内置命令 | 3 | 0 | +2 | +2 | **7** |
| **合计** | **~36** | **+68** | **+28** | **+47** | **~179** |

---

## 七、AI 自动化工作流场景验证

### 场景 A：从零搭建第三人称游戏原型
1. 创建关卡 → 设置 WorldSettings（GameMode）
2. 创建角色蓝图（Character 父类 + 胶囊 + Mesh + Camera）
3. 导入或引用骨骼 + 动画序列
4. 创建动画蓝图（状态机：Idle/Walk/Jump）
5. 创建 BlendSpace + Montage
6. 创建 Enhanced Input Action + IMC
7. 在角色蓝图中连接输入节点到移动/跳跃逻辑
8. 创建 HUD Widget（HP条 + 技能图标）
9. 设置 GameMode 使用该角色 + HUD

### 场景 B：AI 批量生成关卡内容
1. 读取 DataTable（关卡配置表）
2. 批量 spawn_actor（按坐标、类型）
3. 批量 set_actor_property（材质、缩放）
4. 批量 attach_actor_to_actor（场景层级）
5. 构建导航网格 → 保存关卡

### 场景 C：材质系统自动化
1. 创建材质（PBR 金属/粗糙度工作流）
2. 添加纹理采样节点 + 参数节点
3. 创建材质实例 + 设置参数
4. assign_material_to_mesh → 保存资产

---

*本文档将随实现进度持续更新。v2.0 基于完整代码分析，命令总数从 ~137 扩展为 ~179，新增 Data Table、Sequencer、Collision、WorldSettings、Actor附加/Label/Tag 等关键自动化能力。*
