# RendererDemo Frosted Glass 开发与验收计划

配套文档：

- 共享关键类说明图：
  - `docs/RendererFramework_KeyClass_Diagram.md`
- 英文版本：
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

- 版本：v1.4
- 日期：2026-02-27
- 范围：`RendererCore` + `RendererDemo`
- 状态：已批准，作为实现与验收基线

## 配套引用（必须同步维护）

- Frosted 帧级 pass 参考：
  - `docs/RendererDemo_FrostedGlass_FramePassReference.md`
- Blur source 优化计划与进度跟踪：
  - `docs/FeatureNotes/20260227_B8_BlurSourceOptimizationPlan.md`
- 截图回归计划与进度跟踪：
  - `docs/FeatureNotes/20260228_B9_FixedViewpointScreenshotRegressionPlan.md`
- 截图回归运行手册与排障：
  - `docs/FeatureNotes/20260228_B9_RegressionWorkflow_Runbook.md`
- 规则：
  - 任何 frosted render-graph / pass / resource 合同变更，都必须在同一个 PR 中同步更新帧参考文档。

## 1. 背景与当前基线

本计划基于当前 `glTFRenderer` 仓库中的实现。

### 现有流水线

- `RendererSystemSceneRenderer` 的 scene base pass 会写 color / normal / depth render target。
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSceneRenderer.cpp`
- lighting pass 以 compute 方式运行，并写出单个 lighting output render target。
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
- frosted glass pass 当前是全屏 compute，并直接充当最终输出。
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`

### 现有框架能力

- RenderGraph 已支持 graphics / compute / ray-tracing pass node 及推断依赖。
  - `glTFRenderer/RendererCore/Public/RendererInterface.h`
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- Debug UI 已经暴露 pass timing 和 dependency diagnostics。
  - `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`
- window-relative render target 与 resize 生命周期已经实现。
  - `glTFRenderer/RendererCore/Public/RendererInterface.h`
  - `glTFRenderer/RendererCore/Private/ResourceManagerSurfaceSync.cpp`

### 需要解决的主要限制

- Lighting shader 在 post effect 之前就写入 `LinearToSrgb(...)`，限制了物理一致的后处理空间。
  - `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
- 当前 GBuffer 过于精简，缺少 velocity / history 友好的数据。
- Frosted glass 仍是单个重 pass，且缺少时间稳定化。
- 面板交互状态流（hover / grab / move / scale）尚未接入 panel 行为。
- 当前重叠处理仍是单像素 single-winner，无法较好复现多层玻璃叠放表现。
- 当前 frosted panel mask 与 optics 推导主要依赖屏幕空间逻辑，对真正 3D 透视 UI panel 有兼容风险。
- 当前固定 blur pyramid 路径有较高 pass / resource 成本，strict multilayer 还会重复整条 blur 链。

## 2. 目标与非目标

## 范围内

- 扩展框架和渲染流水线，支持高质量 frosted glass 继续演进。
- 给分阶段交付提供明确的实现路径和验收标准。
- 在新增 pass 的同时保持现有 demo 稳定性和 debug 可见性。
- 尽可能向 Apple Vision Pro 风格的玻璃叠层行为对齐，同时把运行时成本约束在可控预算内。

## 首次交付范围外

- 原生平台 passthrough 设备集成。
- 基于眼动的 foveated rendering 集成。
- XR runtime 与 space-anchoring 平台 API。

## 3. 工作流 A：框架与流水线扩展

## A1. 升级到线性 HDR 色彩流水线（P0）

- 说明：
  - scene 和 lighting 保持在线性 HDR buffer 中。
  - 在 swapchain blit 前新增独立的 tone mapping pass。
- 交付物：
  - HDR 中间目标、tone map shader / pass、配置开关。
- 主要文件：
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- 验收：
  - lighting pass 输出路径中不再直接做最终 sRGB 转换。
  - tone mapping 被隔离为独立步骤，并可用于调试开关。

## A2. GBuffer 与 view data 扩展（P0）

- 说明：
  - 补充高级玻璃合成所需的材质 / 辅助通道。
  - 为 view buffer 增加前一帧矩阵数据。
- 交付物：
  - 更新后的 `ViewBuffer`、base pass outputs 与消费者路径。
- 主要文件：
  - `glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.h`
  - `glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/SceneViewCommon.hlsl`
  - `glTFRenderer/RendererDemo/Resources/Shaders/ModelRenderingShader.hlsl`
- 验收：
  - previous-frame transform 按帧上传。
  - 新通道有效并可通过 debug 检查可视化。

## A3. PostFX 共享资源基础设施（P0）

- 说明：
  - 引入可复用的 half / quarter 分辨率 render target 与 ping-pong 资源。
  - 统一 postfx pass 的资源命名和 resize 行为。
- 交付物：
  - 可复用的资源创建工具 / 模式，以及 pass wiring。
- 主要文件：
  - `glTFRenderer/RendererDemo/RendererSystem/*`
  - `glTFRenderer/RendererCore/Public/RendererInterface.h`
- 验收：
  - 所有 postfx buffer 都是 window-relative 且 resize 正确。
  - resize 后没有陈旧 descriptor / resource binding。

## A4. 时间历史基础设施（P1）

- 说明：
  - 为时间稳定化增加 history buffer 生命周期。
  - 在 camera jump 或 resize 时增加 reset / invalidation 路径。
- 交付物：
  - History RT handle、逐帧 swap 逻辑、invalidatation policy。
- 验收：
  - temporal buffer 能跨帧正确使用，并在 resize 时正确 reset。

## A5. 可选的 rendergraph 易用性改进（P1）

- 说明：
  - 为 pass setup 约定和依赖声明增加便捷 helper。
- 交付物：
  - helper wrapper 或 utility builder，减少重复 setup 代码。
- 验收：
  - 新 pass 的接入样板更少，binding 错误更少。

## 4. 工作流 B：应用层与效果实现

## B1. Frosted glass V2 多 pass 架构（P0）

- 说明：
  - 用结构化阶段替换单个重 pass：
    - mask / parameter 阶段
    - blur pyramid 阶段
    - final composite 阶段
- 交付物：
  - 更新后的 `RendererSystemFrostedGlass` pass graph 与 shader。
- 主要文件：
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- 验收：
  - 在降低逐帧成本增长的同时，视觉上达到 v1 持平或更好。

## B2. Panel 状态机与运行时控制（P0）

- 说明：
  - 增加 panel 交互状态：idle、hover、grab、move、scale。
  - 用状态曲线驱动 blur / rim / fresnel / alpha。
- 交付物：
  - panel descriptor 中的状态参数以及运行时更新路径。
- 验收：
  - 状态切换平滑、确定，不出现突兀闪烁。

## B3. 形状与层叠升级（P1）

- 说明：
  - 为不规则形状实现真实 mask 路径。
  - 增强 panel layering 与 overlap 处理。
- 交付物：
  - `ShapeMask` 路径不再只是 fallback。
- 主要文件：
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- 验收：
  - 不规则 panel mask 能正确渲染，且堆叠结果可预测。

## B4. 可读性保护与 UI-safe 行为（P1）

- 说明：
  - 增加可选的 text-safe region 策略，避免关键 UI 标签后方 blur 过强。
- 交付物：
  - 可读性策略参数与调试开关。
- 验收：
  - 在高频背景下 UI 可读性保持稳定。

## B5. 沉浸度混合基线（P1）

- 说明：
  - 在虚拟背景层和玻璃合成之间增加可控 blend。
- 交付物：
  - `immersion_strength` 风格控制与过渡处理。
- 验收：
  - 过渡平滑，不出现突兀的曝光或对比度跳变。

## B6. 多层玻璃合成对齐（P0）

- 说明：
  - 把 overlap 处理从 single-winner 升级为多层合成（top-N，默认 top-2）。
  - 改善重叠区域的视觉行为，更贴近 Vision Pro 风格的叠层玻璃感知。
  - 增加质量档位与保护策略，把性能开销控制在边界内。
- 交付物：
  - 多层 panel 选择 payload（默认 top-2）与 layered composite 路径。
  - 运行时质量切换（`single`、`auto`、`multi-layer`）与 overlap-aware fallback。
  - 重叠场景的性能埋点与验收检查表。
- 主要文件：
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- 验收：
  - 重叠区域表现出稳定、确定的多层混合行为。
  - 非重叠区域仍走单层快速路径。
  - 默认 `auto` 模式在目标测试机与场景上满足约定性能预算。

## B7. 通过 Panel GBuffer 支持 3D 透视面板兼容（P0）

- 说明：
  - 引入专用 Panel GBuffer 路径，使 frosted panel 合成由 panel 几何覆盖 / 深度数据驱动，而不是只依赖屏幕空间 SDF。
  - 以双层合成为默认目标（`back -> front` 顺序合成），向 Vision Pro 风格叠层 panel 对齐，同时控制成本。
  - 增加显式的 thickness 驱动边缘高光 / 折射耦合，以匹配 3D 玻璃厚度感。
  - 增加由主方向光驱动的边缘高光调制；当没有方向光时回退到基于视角的高光。
  - 增加 AVP 风格的独立 edge-specular 层，并支持基于 profile 的边缘宽度控制。
  - 保持与现有 2D UI pipeline 兼容，仅把 frosted-background panel 送入 Panel GBuffer；文本 / 图标 / widget 仍保留在常规 UI pass。
- 交付物：
  - 针对 3D / 2D frosted panel background 的 raster prepass，写 front / back panel payload。
  - 消费 Panel GBuffer 与现有 blur pyramid 的 compute composite 路径。
  - 用于透视边缘光照行为的 edge / profile payload 支持（normal 或等效数据）。
  - 每帧从 lighting system 更新 frosted global parameter 中的主方向光参数。
  - 高级全局 UI 中的独立 edge-specular 控件（`intensity`、`sharpness`、`width`、`white mix`）。
  - 明确 2D overlay 模式与 world-space 模式的 depth / sorting 行为策略。
- 主要文件：
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
  - `glTFRenderer/RendererCore/Public/RendererInterface.h`
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- 验收：
  - 带透视变换的 3D panel 在相机运动下具有稳定的形状 / 边缘 / 折射表现。
  - 双层重叠按顺序合成（front 基于 back 结果），排序结果确定。
  - 2D 非 frosted UI 渲染路径保持不变，没有视觉回归。
  - thickness 增加能带来更强 / 更宽的边缘高光响应，且效果有界稳定，不会冲刷中心区域。
  - 有方向光时，边缘高光方向性跟随最强方向光；无方向光时，回退到现有基于视角的表现。
  - 在相同 panel 配置下，edge-specular 控件能带来更明显的白色边缘高光，同时不引入中心区域 washout。

## B8. Blur source 优化与成本降低（P0）

- 说明：
  - 用共享的低成本 blur source 策略替换固定多级 blur pyramid 依赖。
  - 在更低的运行时 pass / resource 成本下，优先保持 AVP 风格的主观模糊质量。
  - 保留 B7 的双层顺序合成行为和兼容保证。
- 交付物：
  - 支持运行时切换的 blur-source 模式：
    - `LegacyPyramid`（回退）
    - `SharedMip`（默认目标）
    - `SharedDual`（可选高质量模式）
  - 面向共享 source 输入的 composite sampling 路径。
  - 严格 multilayer 与来自 back-composited 输入的可选第二 source 集成。
  - 性能 / 视觉验收证据和默认模式决策。
- 主要文件：
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
  - `docs/RendererDemo_FrostedGlass_FramePassReference.md`
  - `docs/FeatureNotes/20260227_B8_BlurSourceOptimizationPlan.md`
- 验收：
  - 与 legacy 路径相比，blur-source pass 数明显下降。
  - 在约定的 AVP 对齐场景中，视觉质量保持或提升。
  - strict multilayer 重叠行为仍保持确定且稳定。
  - 不引入 resize、时间稳定性或 descriptor / resource 校验回归。

## 5. 里程碑计划

## M0：冻结基线与埋点

- 冻结参考和验收检查点。
- 记录当前截图和 pass timing。

## M1：流水线正确性

- 完成 A1 和 A2。
- 确认线性 HDR 路径和更新后的 GBuffer / view data。

## M2：Frosted V2 核心

- 完成 A3 和 B1。
- 达到稳定的多 pass frosted 输出。

## M3：交互与时间稳定性

- 完成 A4 和 B2。
- 增加稳定的状态切换和 history 行为。

## M4：质量补全

- 根据发布范围完成 B3 / B4 / B5 / B6。
- 完成最终验收并刷新基线。

## M5：透视兼容性集成

- 把 B7 作为下一个实现重点。
- 交付由 Panel GBuffer 驱动、具备 2D / 3D UI 兼容验收的双层 frosted 合成。

## M6：Blur source 优化落地

- 在 B7 验收后优先推进 B8。
- 交付共享低成本 blur source 路径，并验证 pass / resource 缩减和视觉保持。

## M7：固定视点截图回归系统

- 在 B8 达到稳定目标视觉方向后优先推进 B9。
- 交付命令行固定点截图和 baseline compare 工作流，并产出报告工件。
- 在同一工作流中接入 frame / per-pass 性能遥测采集和阈值门控。

## 6. 验收标准

## 功能验收

- 所有规划 pass 按正确顺序执行，依赖有效。
- resize / minimize / recover 流程下效果输出保持有效。
- Debug UI 能检查并调节关键 frosted 参数。

## 视觉验收

- 在相机和 panel 运动过程中，panel 边缘、rim highlight 和折射保持稳定。
- panel 边界处不出现明显 halo 跳变或 mask 不稳定。
- 在明亮 / 黑暗及高频背景下保持可接受的可读性。

## 性能验收

- 在相同场景 / 相机路径下与冻结基线对比。
- 用现有 render graph timing UI 报告每个 pass 的 CPU / GPU timing。
- 在目标测试机上把 frosted 升级控制在约定预算内。
- 对 multi-layer glass 模式，单独定义并跟踪 overlap 场景预算与 auto-fallback 行为。

## 稳定性验收

- 正常运行过程中不出现 descriptor / resource binding 错误。
- resize 生命周期行为不回归。
- 不因新 pass 校验问题导致 frame execution skip。

## 7. 任务积压摘要

| ID | 流 | 优先级 | 标题 | 退出条件 | 状态 | 证据 |
|---|---|---|---|---|---|---|
| A1 | Framework | P0 | 线性 HDR + tone map | Lighting 不再直接输出最终 sRGB | 进行中（A1.1 已完成） | `docs/FeatureNotes/20260223_A1_LinearOutput_ToneMapPass.md` |
| A2 | Framework | P0 | GBuffer / View 扩展 | Prev-frame 与额外通道可用 | 进行中（A2.1 / A2.2 / A2.3 已完成） | `docs/FeatureNotes/20260223_A2_ViewBufferPrevVP.md`, `docs/FeatureNotes/20260223_A2_VelocityGBuffer.md`, `docs/FeatureNotes/20260223_A2_VelocityDebugView.md` |
| A3 | Framework | P0 | PostFX 共享资源 | Half / quarter 与 ping-pong 资源就绪 | 进行中（A3.1 基础已完成） | `docs/FeatureNotes/20260223_A3_PostFXSharedResources.md` |
| B1 | App | P0 | Frosted V2 多 pass | 多 pass 输出替换 v1 单 pass | 进行中（B1.1 / B1.2 / B1.3 已完成；视觉 / 性能验收待完成） | `docs/FeatureNotes/20260223_B1_MultipassScaffold.md`, `docs/FeatureNotes/20260223_B1_QuarterBlurPyramid.md`, `docs/FeatureNotes/20260224_B1_FrostedMaskPass.md` |
| A4 | Framework | P1 | Temporal history 基础设施 | 稳定的 history 生命周期和 reset 路径 | 进行中（A4 核心生命周期 + invalidation + reprojection 已完成；视觉 / 性能验收待完成） | `docs/FeatureNotes/20260224_A4_TemporalHistoryInfrastructure.md` |
| B2 | App | P0 | Panel 状态机 | Idle / hover / grab / move / scale 完成 | 进行中（B2 核心状态机 + 运行时控制已完成；视觉验收待完成） | `docs/FeatureNotes/20260224_B2_PanelStateMachineRuntimeControl.md` |
| B3 | App | P1 | Shape mask 路径 | 完成不规则 mask 路径 | 进行中（B3 核心 shape-mask + layering policy 已完成；视觉 / 性能验收待完成） | `docs/FeatureNotes/20260224_B3_ShapeMaskLayering.md` |
| B4 | App | P1 | 可读性保护 | text-safe 行为已验证 | 计划中 | - |
| B5 | App | P1 | 沉浸度混合 | 平滑 blend 控制已验证 | 计划中 | - |
| B6 | App | P0 | 多层合成对齐 | 在成本有界前提下完成 Top-N 重叠混合 | 进行中（B6.1 top-2 payload + B6.2 adaptive / runtime refinement 已完成；视觉 / 性能验收待完成） | `docs/FeatureNotes/20260224_B6_MultilayerCompositionPlan.md`, `docs/FeatureNotes/20260224_B6_Top2MultilayerCore.md`, `docs/FeatureNotes/20260224_B6_AdaptiveMultilayerRefinement.md` |
| B7 | App | P0 | Panel GBuffer 3D 兼容 | 几何感知 panel payload 驱动的双层 frosted 合成 | 已验收（B7.1~B7.7 已完成，包括 prepass-feed source path 与 phase-2 highlight tuning；视觉 / 性能证据已于 2026-02-27 记录） | `docs/FeatureNotes/20260225_B7_PanelGBufferTwoLayerCompatibilityPlan.md` |
| B8 | App | P0 | Blur source 优化与降本 | 以共享低成本 blur source 替换固定 blur pyramid，并保持 B7 视觉行为 | 进行中（B8.1 / B8.2 运行时 pass 缩减已接通；B8.3 shader shared sampling branch 已集成；legacy `Full Fog Mode` 边界去结构化分支已落地；B8.5 验收待完成） | `docs/FeatureNotes/20260227_B8_BlurSourceOptimizationPlan.md` |
| B9 | Framework + App | P0 | 固定视点视觉 / 性能回归 | 一键 capture + 一键 baseline compare，用于确定性的视觉与帧性能回归检查 | 进行中（capture flow + script compare flow 已可用；CI gate 与 framework readback 待完成） | `docs/FeatureNotes/20260228_B9_FixedViewpointScreenshotRegressionPlan.md` |
| A5 | Framework | P1 | RenderGraph 易用性 helper | setup 样板减少 | 计划中 | - |

## 8. 本计划的开发与评审规则

- 每个已完成任务都必须更新：
  - 变更文件列表
  - 行为前后对比摘要
  - 验收证据（timing / screenshot / checklist）
- 没有至少一项可度量的验收证据，就不能算任务完成。
- 这份文件是范围和验收的事实来源。

## 9. 文档位置与用途

- 归档路径：`docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
- 这份文档是后续工作的实现与验收合同。

## 10. 开发节奏约定

本节定义后续围绕本计划推进开发时的默认执行节奏。

### 10.1 先做最小工作项

- 开发应对齐到最小可交付粒度（单个 backlog item 或一个明确子项）。
- 一次迭代只针对一个主要目标：
  - 一个行为变化
  - 一个验证范围
  - 一个验收结论
- 避免在同一实现周期里打包无关改进。

### 10.2 迭代流程（必需）

1. 从第 7 节选择一个 backlog item（或一个明确子项）。
2. 只实现这一范围内的变更。
3. 针对该变更执行验证（功能 + 视觉 / 性能，按需）。
4. 为本次迭代写一份 feature note 文档（见第 10.3 节）。
5. 更新本主计划文档中的状态字段（见第 10.4 节）。
6. 只有挂上证据后，才能把该项标记为已验收。

### 10.3 Feature Note 归档（每次通过迭代后必需）

- 每次完成并验证的迭代，都必须在以下目录新增独立 note：
  - `docs/FeatureNotes/`
- 使用模板：
  - `docs/FeatureNotes/Feature_Note_Template.md`
- 推荐命名：
  - `YYYYMMDD_WORKITEMID_ShortName.md`
  - 例如：`20260224_B1_FrostedMaskPass.md`

每份 feature note 都必须包含：

- 工作项 ID 和范围边界
- 功能逻辑摘要
- 算法或 shader 逻辑摘要
- 变更文件列表
- 验证证据（timing / screenshot / checklist）
- 风险与后续任务

### 10.4 主计划更新规则（必需）

每次迭代通过验证后，都要更新本文件中的：

- 任务状态变化（planned / in-progress / done）
- 指向 feature note 文件的验收证据链接
- 任何范围 / 进度 / 风险调整

如果验收失败，则保持任务打开，并记录失败原因和下一步动作。

### 10.5 完成定义（单次迭代级别）

只有满足以下全部条件，一次迭代才算完成：

- 范围内代码变更已完成
- 验证已执行且已记录
- feature note 已归档到 `docs/FeatureNotes/`
- 本主计划已按要求更新
