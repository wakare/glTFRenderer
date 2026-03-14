# RendererDemo Frosted Glass 帧内 Pass 参考

配套文档：

- 共享关键类说明图：
  - `docs/RendererFramework_KeyClass_Diagram.md`
- 英文版本：
  - `docs/RendererDemo_FrostedGlass_FramePassReference.md`

- 版本：v1.2
- 日期：2026-02-26
- 范围：`RendererSystemFrostedGlass` 的单帧 frosted-glass 渲染路径
- 源文件：
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedPanelProducer.cpp`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedPanelProducer.h`
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlassPostfx.hlsl`

## 1. 维护合同（必需）

- 这份文档是 frosted-glass pass 的帧级合同。
- 任何修改 frosted-glass render pass 的 PR，都必须在同一个 PR 中更新本文档。
- 必须同步的情况：
  - pass 新增 / 删除 / 重命名（包括 `debug_name` 变化）
  - 依赖变化（`dependency_render_graph_nodes`）
  - 资源绑定变化（`sampled_render_targets`、`render_targets`、`buffer_resources`）
  - `Tick` 中的运行时路径切换逻辑变化
  - frosted 资源的 render target 创建 / format / resolution policy 变化
- 评审检查项：
  - 文档中的 pass 顺序与 `Tick` 中的运行时注册顺序一致
  - 文档中的资源 producer / consumer 映射与代码一致
  - strict multilayer 与 payload path switch 描述仍然准确

## 2. 运行时路径切换

### 2.1 Panel payload 生产路径切换

- 运行时标志：
  - 请求：`m_panel_payload_path == PanelPayloadPath::RasterPanelGBuffer`
  - 就绪：`m_panel_payload_raster_ready`
- 注册行为：
  - raster 就绪：注册 `Frosted Mask/Parameter Raster Front` + `Frosted Mask/Parameter Raster Back`
  - 否则：注册 compute `Frosted Mask/Parameter`
- 回退标志：
  - `m_panel_payload_compute_fallback_active = request_raster && !use_raster`

### 2.2 Composite 分支切换

- Blur source mode：
  - 由 `m_blur_source_mode`（UI / runtime）请求
  - 运行时生效模式写入 `m_global_params.blur_source_mode`
  - `SharedDual` 当前会回退到 `SharedMip`
- Legacy full-fog 模式：
  - 由 `m_global_params.full_fog_mode` 控制（frosted debug UI 中的 `Full Fog Mode`）
  - 启用后，legacy composite 路径会把 frosted sampling 偏向 screen-UV 的超低频 level，以减弱几何边缘清晰度
- Strict multilayer 分支条件：
  - `m_global_params.multilayer_mode == MULTILAYER_MODE_FORCE`
  - 且 `m_multilayer_runtime_enabled == true`
- 分支：
  - strict + legacy：`Back Composite -> Multilayer Blur Pyramid -> Front Composite`
  - strict + shared mip：`Back Composite SharedMip -> Multilayer SharedMip Downsample -> Front Composite SharedMip`
  - fast + legacy：legacy 单 composite pass
  - fast + shared mip：shared-mip 单 composite pass

### 2.3 Temporal history ping-pong

- Active node 选择：
  - 读 A / 写 B：`... History A->B`
  - 读 B / 写 A：`... History B->A`
- 帧末切换：
  - `m_temporal_history_read_is_a = !m_temporal_history_read_is_a`

### 2.4 Panel source 聚合（上传前）

- `g_frosted_panels` 上传顺序依次合并：
  - 内部可编辑 panel（`m_panel_descs`）
  - callback producer world-space panel（`m_producer_world_space_panel_descs`）
  - callback producer overlay panel（`m_producer_overlay_panel_descs`）
  - 手动外部 world-space panel（`m_external_world_space_panel_descs`）
  - 手动外部 overlay panel（`m_external_overlay_panel_descs`）
- 上传结果会裁剪到 `MAX_PANEL_COUNT`。
- temporal history validity 跟随实际上传后的 `panel_count`，而不只是内部 panel 数。

## 3. 每帧 Pass 图

### 3.1 基础 blur-source 链（取决于 mode）

| ID | Pass 调试名 | 类型 | 显式依赖 | 主要输入资源 | 主要输出资源 |
|---|---|---|---|---|---|
| A01 | Downsample Half | Compute | - | Lighting output | Half ping (shared postfx) |
| A02 | Blur Half Horizontal | Compute | A01 | Half ping | Half pong |
| A03 | Blur Half Vertical | Compute | A02 | Half pong | `m_half_blur_final_output` |
| A04 | Downsample Quarter | Compute | A03 | `m_half_blur_final_output` | Quarter ping (shared postfx) |
| A05 | Blur Quarter Horizontal | Compute | A04 | Quarter ping | Quarter pong |
| A06 | Blur Quarter Vertical | Compute | A05 | Quarter pong | `m_quarter_blur_final_output` |
| A07 | Downsample Eighth | Compute | A06 | `m_quarter_blur_final_output` | `m_eighth_blur_ping` |
| A08 | Blur Eighth Horizontal | Compute | A07 | `m_eighth_blur_ping` | `m_eighth_blur_pong` |
| A09 | Blur Eighth Vertical | Compute | A08 | `m_eighth_blur_pong` | `m_eighth_blur_final_output` |
| A10 | Downsample Sixteenth | Compute | A09 | `m_eighth_blur_final_output` | `m_sixteenth_blur_ping` |
| A11 | Blur Sixteenth Horizontal | Compute | A10 | `m_sixteenth_blur_ping` | `m_sixteenth_blur_pong` |
| A12 | Blur Sixteenth Vertical | Compute | A11 | `m_sixteenth_blur_pong` | `m_sixteenth_blur_final_output` |
| A13 | Downsample ThirtySecond | Compute | A12 | `m_sixteenth_blur_final_output` | `m_thirtysecond_blur_ping` |
| A14 | Blur ThirtySecond Horizontal | Compute | A13 | `m_thirtysecond_blur_ping` | `m_thirtysecond_blur_pong` |
| A15 | Blur ThirtySecond Vertical | Compute | A14 | `m_thirtysecond_blur_pong` | `m_thirtysecond_blur_final_output` |

SharedMip 链：

| ID | Pass 调试名 | 类型 | 显式依赖 | 主要输入资源 | 主要输出资源 |
|---|---|---|---|---|---|
| S01 | SharedMip Downsample Half | Compute | - | Lighting output | `m_half_blur_final_output` |
| S02 | SharedMip Downsample Quarter | Compute | S01 | `m_half_blur_final_output` | `m_quarter_blur_final_output` |

说明：

- 当前 SharedMip 运行时路径会注册完整的 `S01`~`S05` downsample 链。

### 3.2 Panel payload 生产器（每帧只激活一条路径）

| ID | Pass 调试名 | 类型 | 显式依赖 | 主要输入资源 | 主要输出资源 |
|---|---|---|---|---|---|
| B01 | Frosted Mask/Parameter | Compute | - | scene depth / normal、panel buffer、global buffer | `m_frosted_mask_parameter_output`、`m_frosted_mask_parameter_secondary_output`、`m_frosted_panel_optics_output`、`m_frosted_panel_optics_secondary_output`、`m_frosted_panel_profile_output`、`m_frosted_panel_profile_secondary_output` |
| B02 | Frosted Mask/Parameter Raster Front | Graphics | - | scene depth / normal、panel buffer、global buffer | `m_frosted_mask_parameter_output`、`m_frosted_panel_optics_output`、`m_frosted_panel_profile_output`、`m_frosted_panel_payload_depth` |
| B03 | Frosted Mask/Parameter Raster Back | Graphics | B02 | scene depth / normal、`FrontMaskParamTex`、panel buffer、global buffer | `m_frosted_mask_parameter_secondary_output`、`m_frosted_panel_optics_secondary_output`、`m_frosted_panel_profile_secondary_output`、`m_frosted_panel_payload_depth_secondary` |

说明：

- Raster 路径会绘制 instanced panel quad（`vertex_count = 6`、`instance_count = MAX_PANEL_COUNT`）。
- Back raster pass 依赖 front raster pass，用于避免 depth / payload 自环。

### 3.3 Composite 分支

### Strict multilayer 分支（`Back -> Front`）

Legacy pyramid strict 路径：

| ID | Pass 调试名 | 类型 | 显式依赖 | 主要输入资源 | 主要输出资源 |
|---|---|---|---|---|---|
| C01 | Frosted Composite Back | Compute | A15 | lighting output + base blur finals + front / back mask / optics / profile payloads | `m_frosted_back_composite_output` |
| C02 | Downsample Half Multilayer | Compute | C01 | `m_frosted_back_composite_output` | `m_half_multilayer_ping` |
| C03 | Blur Half Multilayer Horizontal | Compute | C02 | `m_half_multilayer_ping` | `m_half_multilayer_pong` |
| C04 | Blur Half Multilayer Vertical | Compute | C03 | `m_half_multilayer_pong` | `m_half_multilayer_blur_final_output` |
| C05 | Downsample Quarter Multilayer | Compute | C04 | `m_half_multilayer_blur_final_output` | `m_quarter_multilayer_ping` |
| C06 | Blur Quarter Multilayer Horizontal | Compute | C05 | `m_quarter_multilayer_ping` | `m_quarter_multilayer_pong` |
| C07 | Blur Quarter Multilayer Vertical | Compute | C06 | `m_quarter_multilayer_pong` | `m_quarter_multilayer_blur_final_output` |
| C08 | Downsample Eighth Multilayer | Compute | C07 | `m_quarter_multilayer_blur_final_output` | `m_eighth_multilayer_ping` |
| C09 | Blur Eighth Multilayer Horizontal | Compute | C08 | `m_eighth_multilayer_ping` | `m_eighth_multilayer_pong` |
| C10 | Blur Eighth Multilayer Vertical | Compute | C09 | `m_eighth_multilayer_pong` | `m_eighth_multilayer_blur_final_output` |
| C11 | Downsample Sixteenth Multilayer | Compute | C10 | `m_eighth_multilayer_blur_final_output` | `m_sixteenth_multilayer_ping` |
| C12 | Blur Sixteenth Multilayer Horizontal | Compute | C11 | `m_sixteenth_multilayer_ping` | `m_sixteenth_multilayer_pong` |
| C13 | Blur Sixteenth Multilayer Vertical | Compute | C12 | `m_sixteenth_multilayer_pong` | `m_sixteenth_multilayer_blur_final_output` |
| C14 | Downsample ThirtySecond Multilayer | Compute | C13 | `m_sixteenth_multilayer_blur_final_output` | `m_thirtysecond_multilayer_ping` |
| C15 | Blur ThirtySecond Multilayer Horizontal | Compute | C14 | `m_thirtysecond_multilayer_ping` | `m_thirtysecond_multilayer_pong` |
| C16 | Blur ThirtySecond Multilayer Vertical | Compute | C15 | `m_thirtysecond_multilayer_pong` | `m_thirtysecond_multilayer_blur_final_output` |
| C17 | Frosted Composite Front History A->B / B->A（每帧只激活一个） | Compute | C16 | `BackCompositeTex` + multilayer blur finals + front / back mask / optics / profile payloads + velocity + history read | `m_frosted_pass_output`、history write（`m_temporal_history_a` 或 `m_temporal_history_b`） |

SharedMip strict 路径：

| ID | Pass 调试名 | 类型 | 显式依赖 | 主要输入资源 | 主要输出资源 |
|---|---|---|---|---|---|
| CS01 | Frosted Composite Back SharedMip | Compute | S05 | lighting output + shared-mip blur finals + front / back mask / optics / profile payloads | `m_frosted_back_composite_output` |
| CS02 | SharedMip Downsample Half Multilayer | Compute | CS01 | `m_frosted_back_composite_output` | `m_half_multilayer_blur_final_output` |
| CS03 | SharedMip Downsample Quarter Multilayer | Compute | CS02 | `m_half_multilayer_blur_final_output` | `m_quarter_multilayer_blur_final_output` |
| CS04 | SharedMip Downsample Eighth Multilayer | Compute | CS03 | `m_quarter_multilayer_blur_final_output` | `m_eighth_multilayer_blur_final_output` |
| CS05 | SharedMip Downsample Sixteenth Multilayer | Compute | CS04 | `m_eighth_multilayer_blur_final_output` | `m_sixteenth_multilayer_blur_final_output` |
| CS06 | SharedMip Downsample ThirtySecond Multilayer | Compute | CS05 | `m_sixteenth_multilayer_blur_final_output` | `m_thirtysecond_multilayer_blur_final_output` |
| CS07 | Frosted Composite Front SharedMip History A->B / B->A（每帧只激活一个） | Compute | CS06 | `BackCompositeTex` + multilayer shared-mip finals + front / back mask / optics / profile payloads + velocity + history read | `m_frosted_pass_output`、history write（`m_temporal_history_a` 或 `m_temporal_history_b`） |

### Fast 分支

| ID | Pass 调试名 | 类型 | 显式依赖 | 主要输入资源 | 主要输出资源 |
|---|---|---|---|---|---|
| L01 | Frosted Composite History A->B / B->A（每帧只激活一个） | Compute | A15 | lighting output + base blur finals + front / back mask / optics / profile payloads + velocity + history read | `m_frosted_pass_output`、history write（`m_temporal_history_a` 或 `m_temporal_history_b`） |
| LS01 | Frosted Composite SharedMip History A->B / B->A（每帧只激活一个） | Compute | S05 | lighting output + shared-mip finals + front / back mask / optics / profile payloads + velocity + history read | `m_frosted_pass_output`、history write（`m_temporal_history_a` 或 `m_temporal_history_b`） |

### 3.4 Frosted 之后的外部消费者

- `RendererSystemToneMap` 的 `Tone Map Composite` pass 会读取 `m_frosted_pass_output`，并写入 `ToneMap_Output`。
- 因此 frosted 最终输出是 tone mapping 的直接上游依赖。

### 3.5 Pass 计算逻辑（输入 -> 输出）

本节聚焦单个 pass 的计算语义，而不是 graph 拓扑。

#### 3.5.1 Blur-source passes（`A01`~`A15`、`S01`~`S05`、`C02`~`C16`、`CS02`~`CS06`）

- `DownsampleMain`：
  - 输出像素映射到 `2x2` 源像素块（`base_pixel = output_xy * 2`）。
  - 输出是 4 个 clamp 后源 sample 的算术平均。
- `BlurHorizontalMain` / `BlurVerticalMain`：
  - 沿 X 或 Y 做 1D Gaussian convolution。
  - `radius = max(blur_radius, 1)`。
  - `sigma = max(radius * 0.5 * blur_kernel_sigma_scale, 0.8)`。
  - 在偏移 `[-radius, +radius]` 上做归一化加权求和。
- 运行时映射：
  - `LegacyPyramid`：使用 `A01`~`A15` 和 `C02`~`C16`。
  - `SharedMip`：使用 `S01`~`S05` 和 `CS02`~`CS06`（只做 downsample 的 source 链）。

#### 3.5.2 Payload 生成 - compute 路径（`B01`）

- 对每个像素：
  - 从 depth gradient 计算 scene edge factor：
    - `scene_edge = saturate((|dDepth/dx| + |dDepth/dy|) * scene_edge_scale)`。
  - 计算视角相关项：
    - 从 depth 重建 world position，构建 view dir，`n_dot_v = saturate(abs(dot(N, V)))`。
- 对每个 panel 候选：
  - 用 shape SDF（`RoundedRect` / `Circle` / `ShapeMask`）求 mask 和 rim。
  - 从 SDF normal + scene normal 构建折射方向。
  - 折射 UV：
    - `uv + dir * thickness * refraction_strength * (0.2 + mixed_fresnel)`。
  - 基于深度的 blur 衰减：
    - `depth_aware_weight = exp(-abs(refracted_depth - center_depth) * depth_weight_scale)`。
    - `effective_blur_strength = blur_strength * lerp(depth_aware_min_strength, 1, depth_aware_weight)`。
  - 候选排序依据：
    - `layer_order`（高者优先），然后 `mask`，再然后 `panel_index`。
- 打包 top-2 层：
  - `mask_param = (mask, rim, mixed_fresnel, panel_index)`
  - `panel_optics = (refraction_uv.x, refraction_uv.y, effective_blur_strength, scene_edge)`
  - `panel_profile = (profile_normal_uv.x, profile_normal_uv.y, rim, optical_thickness)`

#### 3.5.3 Payload 生成 - raster 路径（`B02`、`B03`）

- `PanelPayloadVS`：
  - 把每个 panel 展开为 instanced quad。
  - panel mode 切换：
    - screen-space panel：clip XY 来自 `center_uv/half_size_uv`，depth 来自 `EncodePanelLayerDepth`。
    - world-space panel：clip position 来自投影后的 world quad（`world_center + corner.x * world_axis_u + corner.y * world_axis_v`）。
  - 输出 `local_uv`，用于 raster payload 评估（shape / profile 在 panel local space 中计算）。
- `PanelPayloadFrontPS`：
  - 在光栅化像素上评估单个 panel payload。
  - evaluator 切换：
    - screen-space panel：复用现有屏幕空间 SDF payload evaluator。
    - world-space panel：使用 local-UV SDF / profile evaluator，并配合 screen-UV refraction / depth-aware optics。
  - depth policy：
    - `Overlay`：不做 scene-depth 遮挡剔除（用于 2D overlay background）。
    - `Scene Occlusion`：比较 panel depth 与 scene depth，并丢弃被遮挡像素。
  - 写入 front payload MRT；当 mask 无效时丢弃。
- `PanelPayloadBackPS`：
  - evaluator 相同，但会剔除当前像素上与 front panel 相同的 panel（`FrontMaskParamTex`），以生成第二层。
  - 采用与 front pass 相同的 depth-policy gate。
  - 写入 back payload MRT。

#### 3.5.4 Frosted 颜色评估器（用于 `C01`、`C17`、`L01`、`CS01`、`CS07`、`LS01`）

- 入口：`EvaluatePanelFrostedColor(scene_color, center_normal, view_dir, screen_uv, mask_param, panel_optics, panel_profile, ...)`。
- 步骤：
  - 校验 `mask` 与 `panel_index`。
  - 在折射 UV 处按线性（双线性）重建采样 blur-source level，避免 point-sampling shimmer / leak。
    - `LegacyPyramid`：采样 `1/2`、`1/4`、`1/8`、`1/16`、`1/32`。
    - `SharedMip`：从共享 downsample 链采样 `1/2`、`1/4`、`1/8`、`1/16`、`1/32`，并在 shader 中应用 shared-mode 低频补偿。
  - legacy full-fog 分支（`full_fog_mode != 0`）：
    - 通过 sigma 驱动的最小 blur-strength floor，绕过 depth-aware blur attenuation floor drop。
    - 用偏向 `screen_uv` 的 `1/16` + `1/32` 低频采样构建 fog color，弱化折射边缘结构。
    - 提高 fog blend floor 并提前返回，以获得全 panel 雾化的外观，跳过边缘高光 / specular 栈。
  - 进行 highlight 去噪抑制：
    - 检测高于 blur baseline 的 scene luminance 异常值（`scene_luminance - blur_luminance`）。
    - 在最终 blur blend 前，只削弱这部分高频亮残差。
  - 构建 sigma 驱动的混合因子（`quarter` / `eighth` / `sixteenth` / `thirtysecond`）。
  - 随 sigma 增大逐步混向更低频结果（coarse / veil 路径），然后施加对比度压缩和去色偏控制。
  - 按 `effective_blur_strength` 与原始 scene 合成。
  - thickness 耦合：
    - 用全局范围（`thickness_range_min/max`）归一化 thickness。
    - 用 `thickness_refraction_boost_max` 提升折射幅度。
    - 通过 thickness edge power 计算边缘响应（`edge_term`）。
    - 施加有界的边缘阴影（`thickness_edge_shadow_strength`）和高光增强（`thickness_highlight_boost_max`）。
  - directional-light 耦合：
    - 每个像素在 composite pass 中解析 `center_normal` 和 `view_dir`。
    - evaluator 会用 panel profile（world-space panel 还会用 panel world axes）构建 panel highlight normal。
    - profile-direction bending 只在 panel 边缘附近生效，以避免内部 medial-axis seam。
    - evaluator 从 `FrostedGlassGlobalBuffer` 读取 `highlight_light_dir_weight`。
    - 当其有效（`w > 0`）时，边缘高光由主方向光方向调制。
    - 当其无效（`w == 0`）时，调制退化为中性，回退到基于视角的基础行为。
    - 方向调制的范围 / 对比度由以下参数控制：
      - `directional_highlight_min`
      - `directional_highlight_max`
      - `directional_highlight_curve`
  - 独立 edge-specular 层：
    - 用 `max(rim, profile_edge)` 做严格边缘门控；非边缘像素这一项强制为 0。
    - `edge_highlight_width` 控制边缘覆盖范围。
    - `edge_spec_sharpness` 控制 spec lobe 锐度。
    - `edge_spec_intensity` 控制 spec 强度。
    - spec 由 core + halo（宽 lobe）构成，避免边缘高光过细。
    - `edge_highlight_white_mix` 控制偏白程度。
- 输出：
  - `layer_mask`、`out_frosted_color`、`out_blur_metric`（multilayer 逻辑使用的 sigma-strength metric）。

#### 3.5.5 Back composite（`C01`：`CompositeBackMain`）

- 输入：scene color + back payload（在 multilayer gating 下也会读取 front payload）。
- 调用 `EvaluatePanelFrostedColor` 计算 back frosted color。
- 若启用 multilayer：
  - `effective_back_mask = ComputeEffectiveBackMask(back_mask, front_mask)`。
  - 用 effective back mask 在 scene 与 back frosted 之间做混合。
- 输出：`BackCompositeOutput`。

#### 3.5.6 Front composite strict 路径（`C17`：`CompositeFrontMain`）

- 输入 scene 是 `BackCompositeTex`（当启用时已包含 back layer）。
- 应用 front layer 的 frosted 评估并完成混合。
- 时间稳定化：
  - 用 motion vector 做 reprojection：`prev_uv = current_uv - velocity`。
  - history weight：
    - `temporal_history_blend`
    - 受 velocity reject 与 edge reject 衰减
    - 再乘以当前 panel mask
- 输出：
  - `Output`（最终 frosted color）
  - `HistoryOutputTex`（下一帧 history）

#### 3.5.7 Legacy composite 路径（`L01`：`CompositeMain`）

- 从原始 `InputColorTex` 开始，评估 front 和 back 两层 frosted color。
- 若启用 multilayer：
  - 先合成 back，再在 back-composited color 之上重新评估 front。
  - 包含避免 front 比 back 更锐利的保护：
    - 用 front / back blur metric ratio 与 `multilayer_front_transmittance` 缩放 front mix。
- 使用与 strict 路径相同的 temporal stabilization 逻辑。

## 4. 资源合同

### 4.1 外部输入

- Lighting：`m_lighting->GetLightingOutput()`
- Scene GBuffer：
  - depth：`m_base_pass_depth`
  - normal：`m_base_pass_normal`
  - velocity：`m_base_pass_velocity`

### 4.2 Frosted 自有 render target（除注明外均为全分辨率）

- 最终输出与历史：
  - `m_frosted_pass_output`（`PostFX_Frosted_Output`）
  - `m_temporal_history_a`、`m_temporal_history_b`
- Layer payload：
  - `m_frosted_mask_parameter_output`、`m_frosted_mask_parameter_secondary_output`
  - `m_frosted_panel_optics_output`、`m_frosted_panel_optics_secondary_output`
  - `m_frosted_panel_profile_output`、`m_frosted_panel_profile_secondary_output`
  - `m_frosted_panel_payload_depth`、`m_frosted_panel_payload_depth_secondary`（raster payload 路径）
- Strict 分支中间目标：
  - `m_frosted_back_composite_output`
- 基础 blur-source 输出（由 Legacy / SharedMip composite 路径共享）：
  - `m_half_blur_final_output`、`m_quarter_blur_final_output`、`m_eighth_blur_final_output`、`m_sixteenth_blur_final_output`、`m_thirtysecond_blur_final_output`
  - 仅 Legacy 使用的 ping/pong 中间资源：`m_eighth_blur_ping/pong`、`m_sixteenth_blur_ping/pong`、`m_thirtysecond_blur_ping/pong`
  - Legacy 路径下来自 `PostFxSharedResources` 的 shared half / quarter ping/pong
- Strict multilayer blur-source 输出：
  - `m_half_multilayer_blur_final_output`、`m_quarter_multilayer_blur_final_output`、`m_eighth_multilayer_blur_final_output`、`m_sixteenth_multilayer_blur_final_output`、`m_thirtysecond_multilayer_blur_final_output`
  - 仅 Legacy 使用的 ping/pong 中间资源：`m_half_multilayer_ping/pong`、`m_quarter_multilayer_ping/pong`、`m_eighth_multilayer_ping/pong`、`m_sixteenth_multilayer_ping/pong`、`m_thirtysecond_multilayer_ping/pong`

### 4.3 Buffer 资源

- `g_frosted_panels`：
  - structured SRV
  - 容量：`MAX_PANEL_COUNT`
  - payload 类型：`FrostedGlassPanelGpuData`
  - 来源：内部 + 外部（手动 / callback producer）的聚合 panel descriptor
- `FrostedGlassGlobalBuffer`：
  - CBV
  - payload 类型：`FrostedGlassGlobalParams`
  - 包含用于高光调制的每帧主方向光 payload：
    - `highlight_light_dir_weight.xyz`：主方向光方向（world space）
    - `highlight_light_dir_weight.w`：有效性标志（`1` 有效，`0` 回退）
  - 包含 AVP 风格 edge-spec phase-1 控件：
    - `edge_spec_intensity`
    - `edge_spec_sharpness`
    - `edge_highlight_width`
    - `edge_highlight_white_mix`
  - 包含方向高光对比度控制：
    - `directional_highlight_min`
    - `directional_highlight_max`
    - `directional_highlight_curve`

## 5. 预期激活 Pass 数（用于快速 sanity check）

LegacyPyramid 模式：

- Fast 分支 + compute payload：每帧 17 个 frosted pass node（`15 base + 1 payload + 1 composite`）
- Fast 分支 + raster payload：每帧 18 个 frosted pass node（`15 base + 2 payload raster + 1 composite`）
- Strict 分支 + compute payload：每帧 33 个 frosted pass node（`15 base + 1 payload + 17 strict branch`）
- Strict 分支 + raster payload：每帧 34 个 frosted pass node（`15 base + 2 payload raster + 17 strict branch`）

SharedMip 模式（当前 SharedDual 的回退目标）：

- Fast 分支 + compute payload：每帧 7 个 frosted pass node（`5 shared base + 1 payload + 1 composite`）
- Fast 分支 + raster payload：每帧 8 个 frosted pass node（`5 shared base + 2 payload raster + 1 composite`）
- Strict 分支 + compute payload：每帧 13 个 frosted pass node（`5 shared base + 1 payload + 7 strict branch`）
- Strict 分支 + raster payload：每帧 14 个 frosted pass node（`5 shared base + 2 payload raster + 7 strict branch`）

如果运行时观察到的 pass 数与这些值不一致，优先检查 `Tick` 中的注册逻辑。

## 6. 后续渲染改动的更新检查表

- 运行时标志或分支条件变化时，更新第 2 节。
- pass 名称 / 顺序 / 依赖变化时，更新第 3 节。
- shader 数学、packing 或 blend 语义变化时，更新第 3.5 节。
- resource handle、format 或 ownership 变化时，更新第 4 节。
- node 注册数量变化时，更新第 5 节。
- 文档更新必须和代码变更放在同一个 PR 中，不要作为后续补丁延后。
