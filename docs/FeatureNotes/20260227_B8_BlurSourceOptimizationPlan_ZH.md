# Feature Note - B8 Frosted Blur Source 优化计划

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260227_B8_BlurSourceOptimizationPlan.md`

- Date: 2026-02-27
- Work Item ID: B8
- Title: 用共享低成本 blur source 替换固定 frosted blur pyramid
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
  - `docs/RendererDemo_FrostedGlass_FramePassReference.md`
  - `docs/FeatureNotes/20260225_B7_PanelGBufferTwoLayerCompatibilityPlan.md`

## 1. 需求更新

- 新需求：
  - 降低 frosted 场景 blur 生成的运行时成本。
  - 仍然优先保持 AVP 风格的主观模糊观感。
  - 保留 B7 的双层顺序合成行为（`back -> front`）以及 2D / 3D panel 兼容性。
- 范围：
  - 仅调整 blur-source 生成架构。
  - 除非为接入所必需，否则保持当前 panel payload 和 composite 特性集合不变。

## 2. 当前基线成本快照

在当前 B7 基线实现下：

- 固定 blur 链级别：
  - `1/2`、`1/4`、`1/8`、`1/16`、`1/32`
- 单条 blur 链的 pass 数：
  - `5 x Downsample` + `5 x Horizontal Blur` + `5 x Vertical Blur` = `15 passes`
- Strict multilayer 路径：
  - 会为 front-over-back 路径额外再建一整条 blur 链
  - 额外 `15 passes`
- Blur 生成总量：
  - 单路径运行时：`15` 个 blur pass
  - Strict multilayer 运行时：`30` 个 blur pass

Blur 相关 render-target 占用（活跃 handle）：

- 基础链：
  - Frosted 自有专用 RT：`11`
  - 加上来自 `PostFxSharedResources` 的共享 half / quarter ping-pong：`4`
- Strict multilayer 链：
  - 额外的 frosted 自有 RT：`15`
- Frosted 路径最坏情况下会触碰的 blur-source RT handle：
  - 约 `30` 个（含共享 postfx 资源）

说明：

- 这里只统计 source-generation 阶段，不包含 mask / composite / history 输出。
- 现有 B7 验收快照已经在这套基线上验证过视觉质量。

## 3. 优化目标

在保持当前视觉方向的同时，用明显更低的 pass 与 resource 成本替换当前 blur-source 路径：

- 保持足够强的低频背景模糊，支撑 AVP 风格玻璃观感。
- 不改动 compositing、edge highlight、refraction 与 multilayer 策略。
- 支持质量档位，让低成本模式成为默认，高质量模式作为可选。

## 4. 提议的替换架构

## 4.1 路径 A - Shared Mip Source（主目标）

- 从 scene color 生成一个全局的低分辨率 frosted blur source。
- 在 frosted composite shader 中用 mip 选择（`SampleLevel`）作为 blur source。
- Strict multilayer 下可选的双 source 模式：
  - Source 0 来自 scene color。
  - Source 1 来自 back-composited color（仅在 strict 路径激活时使用）。

预期成本：

- 每个 source `1~2` 个 pass（`downsample` + 可选的显式 mip 生成 pass）。
- 每个 source `1` 个带 mip 的低分辨率 color RT。

## 4.2 路径 B - Shared Dual Filter Source（可选高质量档）

- 保留全局 source 思路，但增加低分辨率 dual-filter pass。
- 根据质量档位，使用 `2~4` 个 dual pass。

预期成本：

- 每个 source `3~5` 个 pass。
- 每个 source `2` 个低分辨率 ping-pong RT。

## 4.3 推荐 rollout

- 默认：路径 A（`SharedMip`），获得最佳 cost / perf 比。
- 可选高质量档：路径 B（`SharedDual`）。
- 迁移期间保留 legacy 固定 pyramid 作为回退路径。

## 5. 目标预算对比

| 模式 | Blur Source 策略 | Passes（单路径） | Passes（Strict） | Blur RTs（单路径） | Blur RTs（Strict） |
|---|---|---:|---:|---:|---:|
| Legacy | 固定 5 级 pyramid | 15 | 30 | ~15 touched | ~30 touched |
| B8 Path A | Shared mip source | 1~2 | 2~4 | 1 | 2 |
| B8 Path B | Shared dual source | 3~5 | 6~10 | 2 | 4 |

面向验收的目标：

- Strict multilayer 模式下，blur-source pass 数至少下降 `50%`。
- 保持重叠区域视觉行为不变（排序和混合策略不变）。

## 6. 迁移计划

1. B8.1 - 运行时路径切换与资源抽象
- 增加 blur source mode 枚举：
  - `LegacyPyramid`
  - `SharedMip`
  - `SharedDual`
- 增加 debug / runtime 开关和安全回退路径。

2. B8.2 - Shared mip source 实现
- 增加低分辨率 source RT 创建与更新 pass。
- 增加可选的显式 mip-chain 生成 pass（取决于 backend）。

3. B8.3 - Composite shader 接入
- 在新模式下，用 source+mip sampling 路径替换固定多纹理 blur sampling 依赖。
- 保留 legacy 代码路径用于 A/B 对比。

4. B8.4 - Strict multilayer 二级 source 集成
- 增加一个由 back-composited input 生成的可选第二 source。
- 保持当前 `back -> front` 顺序行为和 panel mask 逻辑。

5. B8.5 - 验证与默认模式决策
- 以 B7 基线场景和重叠场景对比质量 / 性能。
- 确认默认模式（预期为 `SharedMip`）。

## 7. 验收检查表

功能：

- Single 和 strict multilayer 两种模式都能在新 blur source 下正确渲染。
- Resize 和 temporal history 生命周期保持有效。

视觉：

- 在约定测试场景中，AVP 风格的主观模糊等级保持或提升。
- Front / back 重叠行为仍然确定且稳定。

性能：

- Pass 数和每帧 frosted timing 相比 legacy 有可测量的下降。
- 不引入新的 descriptor / resource 校验错误。

## 8. 进度跟踪（B8 的事实来源）

状态说明：`Planned`、`In Progress`、`Blocked`、`Accepted`

| Sub-item | 范围 | 状态 | 最近更新 | 证据 |
|---|---|---|---|---|
| B8.1 | Runtime switch + abstraction | In Progress | 2026-02-28 | `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`、`glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`（`BlurSourceMode`、runtime registration switch） |
| B8.2 | Shared mip source generation | In Progress | 2026-02-28 | `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`（SharedMip 运行时为 base + strict multilayer second source 注册完整 5 级 downsample 链） |
| B8.3 | Composite shader source migration | In Progress | 2026-02-28 | `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl` SharedMip 分支采样完整 shared level（`1/2`~`1/32`）并应用 shared-mode 低频补偿调优；legacy 路径新增 `Full Fog Mode` 分支（偏向 screen-UV 的超低频采样 + 更强的边界去结构化） |
| B8.4 | Strict multilayer second-source integration | In Progress | 2026-02-28 | SharedMip strict 路径会在 front composite 前，从 `m_frosted_back_composite_output` 生成第二 source |
| B8.5 | Perf / visual acceptance and default-mode decision | Planned | 2026-02-27 | 本文档 |

进度维护规则：

- 每次 B8 迭代都必须在同一个 PR 中更新这张表。
- 任何 pass / resource 合同变更也必须同步更新：
  - `docs/RendererDemo_FrostedGlass_FramePassReference.md`

## 9. 下一步动作

- 继续 B8.3 的 SharedMip 质量调优：
  - 对照 B7 基线场景校准 shared 低频补偿
  - 在高 sigma panel 下验证 strict multilayer 重叠质量
- 继续 B8.3 的 Legacy Full Fog 对齐调优：
  - 验证 legacy 模式下（`Full Fog Mode = On`）的几何边界去结构化质量
  - 在 full-fog 切换下保持 strict / fast 分支行为一致
- 执行 B8.5 验收：
  - 用 Frosted debug UI 中的 `Frosted Active Nodes (expected)` 和 runtime path label 快速核对 pass 数
  - 在 force multilayer 场景中采集 pass count / timing
  - 在重叠较重场景中对比 SharedMip 和 Legacy 的视觉行为
