# Feature Note - B7 Panel GBuffer 双层兼容计划

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260225_B7_PanelGBufferTwoLayerCompatibilityPlan.md`

- Date: 2026-02-25
- Work Item ID: B7
- Title: 通过 Panel GBuffer 和双层顺序 frosted 合成支持 3D 透视 panel
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
  - `docs/RendererDemo_FrostedGlass_FramePassReference.md`（帧级 pass / 依赖 / 资源合同）

## 1. 需求更新

- 新优先级要求：
  - 后续 frosted-glass 工作优先支持 Apple Vision Pro 风格的 3D 透视 panel。
  - 默认层叠目标保持双层（`front`、`back`）并按顺序合成。
  - 保持与现有 2D UI 渲染流水线兼容。

## 2. 基线风险

- 当前 frosted panel 逻辑主要由 screen-space SDF 与 layer-order 驱动。
- 主要风险：
  - 透视变换后的 panel 边缘与 optics 表现并不总能匹配真实 3D 几何
  - 重叠排序可能偏离真实 depth 顺序
  - 在激烈的 camera / panel 运动下，时间稳定性压力增大

## 3. 提议架构

1. Panel GBuffer prepass（raster）
- 把 frosted panel background 渲染到专用 panel payload target。
- 同时支持：
  - 3D panel geometry path
  - 2D frosted-background prepass path
- 每个像素保留 top-2 payload：front layer 和 back layer。

2. Blur pyramid path（compute，沿用现有）
- 复用当前 downsample / blur pyramid 流水线。
- 保留质量档位和运行时保护策略。

3. Sequential composite path（compute）
- Pass A：先把 back layer 合成到 scene 上。
- Pass B：再以前一个 pass 的结果为输入合成 front layer。
- 对于只有单层 panel 的像素，保留 single-layer 快速路径。

4. UI content pass（保持不变）
- 文本 / 图标 / 按钮以及非 frosted widget 继续走现有 UI 路径。
- 只有 frosted panel background 会写入 Panel GBuffer payload。

## 4. Panel GBuffer Payload 范围（最小集合）

- 对 front / back 每层都至少保留：
  - `coverage` / `mask`
  - `panel_id`（或紧凑索引）
  - `depth`（保证层级排序确定）
  - `edge/profile normal`（或等价的透视边缘方向 payload）
  - optics / material 控制：
    - blur sigma / blur strength
    - thickness / refraction strength
    - fresnel intensity / fresnel power
    - rim intensity / tint controls

说明：

- 具体 packing 可后续微调，这里定义的是 B7 验收所需的语义最小集。

## 4.1 Thickness 驱动边缘高光规则（必需）

- 目标：
  - 让边缘高光和厚度感知相关，而不是只靠 slider 驱动。
- 必需行为：
  - panel thickness 增大时，边缘高光可见性和边缘阴影趋势都必须增强
  - 中心区域要保持受控，避免整块面板过亮
  - 折射幅度需要随 thickness 有界地增长
- 实现级指导公式：
  - `edge_term = pow(saturate(1 - abs(dot(N, V))), edge_power)`
  - `thickness_factor = saturate((thickness - t_min) / max(t_max - t_min, eps))`
  - `edge_highlight = base_highlight * lerp(1.0, highlight_boost_max, thickness_factor) * edge_term`
  - `edge_shadow = base_shadow * thickness_factor * edge_term`
  - `refraction_scale = base_refraction * lerp(1.0, refraction_boost_max, thickness_factor)`
- 说明：
  - 常数值可以在 B7.5 中调优，但这种耦合行为是验收强制项。

## 4.2 主方向光高光调制规则（必需）

- 目标：
  - 在存在方向光时，让 panel 边缘高光对场景光照方向做出反应。
- 必需行为：
  - 每帧从 lighting system 中挑出最亮的 directional light 方向，写入 frosted global parameters
  - 在 composite 阶段用该方向调制边缘高光
  - 当场景中没有方向光时，保留现有基于视角的高光表现，不出现明显断裂
- 选择规则：
  - 方向光排序指标为光照强度亮度：`0.2126 * R + 0.7152 * G + 0.0722 * B`
  - 选择亮度最大的 directional light 作为高光方向源

## 4.3 AVP 风格边缘高光增强规则（必需）

- 目标：
  - 提高明亮边缘的可读性，使效果更接近 Apple Vision Pro 玻璃观感。
- 必需行为：
  - 保留现有 rim / fresnel 贡献
  - 增加独立的 edge-specular 层，而不是只做乘性调制
  - edge-specular 覆盖范围由 profile / rim / fresnel 边缘基底驱动，并支持可调宽度
  - 当方向光不可用时，回退行为仍然稳定
- Phase-1 全局高级控件：
  - `edge_spec_intensity`
  - `edge_spec_sharpness`
  - `edge_highlight_width`
  - `edge_highlight_white_mix`

## 5. 2D / 3D 兼容策略

- 3D frosted panel：
  - 以真实 world depth 写入 Panel GBuffer。
- 2D frosted panel background：
  - 通过专用 UI prepass 写入 Panel GBuffer。
  - 支持两种模式：
    - overlay mode：固定 composite order / depth policy
    - world-space mode：depth-aware policy
- 2D non-frosted UI content：
  - 不写 Panel GBuffer。
  - 继续走当前 frosted composite 之后的 UI draw path。

## 6. 执行计划（分阶段）

1. B7.1 Panel GBuffer 资源与 payload schema
- 增加 render target 与 binding layout。
- 定义 front / back payload packing 与 clear policy。

2. B7.2 3D panel raster prepass
- 将 frosted panel geometry 渲染到 Panel GBuffer。
- 验证每像素 top-2 层的写入策略。

3. B7.3 双层顺序 composite 集成
- 在 render graph 中接入 back-then-front composite 流程。
- 保留 single-layer 快速路径。

4. B7.4 2D frosted background 兼容 prepass
- 把 2D frosted background 送入 Panel GBuffer。
- 保持 non-frosted UI 不变。

5. B7.5 thickness-edge coupling 实现
- 在 composite 中实现并调优 thickness 驱动的 edge highlight / shadow。
- 增加 `edge_power`、`highlight_boost_max`、`refraction_boost_max` 等 debug 控件。

6. B7.6 稳定性与验收
- 验证视觉稳定性与 pass timing。
- 调优默认值，并在 feature note 中更新证据。
- 集成主方向光驱动的高光调制，并确保无光时能回退。

7. B7.7 AVP 风格边缘高光增强
- 在 composite 中增加 profile-aware 的独立 edge-specular 层。
- 暴露全局控件：spec intensity、sharpness、edge width、white tint mix。
- 在保持回退稳定的前提下，把默认值调向更强的边缘可读性。

## 7. 验收目标

- 功能：
  - Panel GBuffer 的 top-2 payload 生成和消费正确。
  - `Back->front` 顺序 composite 路径确定。
- 视觉：
  - 透视变换后的 panel 边缘与折射行为稳定。
  - 重叠区域呈现预期的双层叠放效果。
  - 2D non-frosted UI 内容不受影响。
  - 在固定 camera 和背景下，厚度增加会带来更强 / 更宽的边缘高光，以及略强的边缘变暗，但不会导致中心区域过曝。
- 性能：
  - 非重叠主导场景下，相比当前 B6 基线开销有界。
  - 重叠场景下仍在预算内，且 fallback 可用。

## 8. 初始风险与缓解

- 风险：额外 panel payload target 带来带宽压力。
  - 缓解：紧凑的 payload packing，以及在可接受时引入 half-resolution 路径。
- 风险：overlay 2D panel 与 world-space 3D panel 之间排序歧义。
  - 缓解：增加显式 per-panel mode 和确定性的 order policy。
- 风险：UI 可读性或 draw order 回归。
  - 缓解：保留现有 non-frosted UI pass，并按场景类型增加验证检查表。

## 9. 下一步动作

- 继续推进带共享 producer 的 B7：
  - 把外部 3D panel producer 接到 `world_space_mode` payload path
  - 为 2D frosted background 增加 prepass producer，共享同一 payload path
- 保持 B6 输出作为回归和性能对比的基线。

## 10. 进度快照（2026-02-25）

- B7.1 已完成（schema + wiring）：
  - 增加了 front / back panel profile payload render target（`PanelProfile`、`PanelProfileSecondary`）。
  - Mask / payload pass 现在能为 front / back 层输出 profile normal direction 和 optical thickness。
  - Composite pass 已消费 profile payload，为后续 thickness-edge coupling 打下接口基础。
- B7.2 runtime + raster payload path 已完成：
  - 增加了运行时 payload-path 选项（`Compute (SDF)` / `Raster (Panel GBuffer)`）。
  - 增加 instanced raster front / back pass：
    - front pass 写 front payload，并做基于深度的层解析
    - back pass 重新 raster，同时拒绝 front panel，并解析第二层
  - Raster 模式下的 payload 生成已转为 panel-geometry coverage 驱动，而不是全屏 panel-loop 遍历。
  - `Tick` 现在可按运行时选择注册 compute 或 raster payload path，并在 debug UI 中显示 compute fallback 状态。
- B7.3a world-space raster producer integration 已完成：
  - 在 panel descriptor / GPU payload 中增加了 world-space payload 字段（`world_center`、`world_axis_u`、`world_axis_v`、`world_space_mode`）。
  - Raster payload VS 已支持 world-space quad projection，实现真实几何驱动的 depth / coverage。
  - Raster payload PS 现在能以 panel-local UV profile 路径评估 world-space payload，同时保持现有 screen-space path 不变。
  - Debug UI 现在暴露 world-space panel toggle 和变换向量，用于在线验证。
- B7.3b 2D / 3D 兼容所需的 depth policy split 已完成：
  - 增加显式 per-panel depth policy（`Overlay` / `Scene Occlusion`），并打包进 panel layering payload。
  - World-space payload raster 仅在 policy 为 `Scene Occlusion` 时应用 scene-depth occlusion。
  - 这样既能支持 VisionOS 风格的 world-space panel 遮挡，也能保持 2D overlay background 的确定性 overlay 行为。
- B7.5 thickness-edge coupling implementation 已完成：
  - 增加了 thickness coupling 的全局调参控件（`edge power`、`highlight boost max`、`refraction boost max`、`edge shadow strength`、`thickness range min/max`）。
  - Compute 与 raster payload evaluator 中，折射幅度都已改为随归一化 thickness 缩放。
  - Composite 高光 / 阴影已加入 thickness 权重边缘响应，并对中心区域做有界抑制。
- B7.6 dominant directional-light highlight modulation 已完成：
  - 增加了从 lighting system 查询最亮方向光方向的逻辑。
  - Frosted global buffer 每帧更新高光方向光向量和有效性标志。
  - Composite 高光项现在支持方向光调制；当没有方向光时，会回退到之前的 view-driven 行为。
- B7.7 AVP 风格 edge highlight 增强 phase-1 已完成：
  - 在现有 rim / fresnel 之上增加了独立 edge-specular 项。
  - 边缘覆盖范围由 profile / rim / fresnel 基底和可调宽度共同控制。
  - 新增高级全局控件：`Edge Spec Intensity`、`Edge Spec Sharpness`、`Edge Highlight Width`、`Edge Highlight White Mix`。
- B7.4 shared producer payload merge foundation 已完成：
  - 增加了针对 world-space panel 和 2D overlay frosted background 的外部 producer 输入 API。
  - 上传路径现已把内部 debug panel、外部 world-space panel 和外部 overlay panel 合并为一个共享 payload 流。
  - 有效 panel count 在上传时被 `MAX_PANEL_COUNT` 截断，并在 debug UI 中显示 `uploaded/requested`，用于溢出诊断。
