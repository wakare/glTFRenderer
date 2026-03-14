# Feature Note - B6 多层合成计划

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260224_B6_MultilayerCompositionPlan.md`

- Date: 2026-02-24
- Work Item ID: B6
- Title: 面向 Vision Pro 风格 multilayer frosted composition 的需求更新与实现计划
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. 需求更新

- 新需求：
  - Frosted-glass 的 overlap 行为应尽量向 Apple Vision Pro 风格的多层感知对齐。
  - 重叠区域应支持 multi-layer mixing，而不是只做 single-winner。
  - 性能开销必须保持在可控范围内，并带有明确的质量档位和 fallback 策略。

## 2. 当前基线差距

- 当前 overlap 流水线每像素只选择一个 panel（single winner），然后做一次 composite。
- 这种方式确定、快速，但重叠区域无法表达分层透射行为。
- 与目标相比的差距：
  - 层级深度响应不足
  - overlap 区域层次感有限
  - 难以逼近期望的 multilayer glass 感受

## 3. 候选重构方案

## Option A（推荐）：Top-2 Layered Composition + Fast Path

- Summary:
  - 在 mask pass 中保留每像素 top-2 panel payload（front + back），按 layer priority + mask score 选择。
  - 在 composite 中分别计算 front 和 back 的 frosted contribution，再按 transmittance 风格权重做合成。
  - 对于只有单层的像素，继续走 single-layer path。
- Pros:
  - Overlap 区域能获得明显的画质提升。
  - 成本增加可预测且有上界。
  - 相比现有架构，结构改动较小。
- Cons:
  - 需要额外的 payload 带宽和重叠像素的额外着色。

## Option B：Weighted Blended Multi-layer（近似 OIT 风格）

- Summary:
  - 用加权公式在一个 pass 中累计多个 panel contribution。
- Pros:
  - 对大量 overlap 的情况有更平滑的聚合行为。
- Cons:
  - 更难做艺术调优和调试。
  - 排序语义不够显式。
  - 若归一化处理不慎，可能引入颜色能量不稳定。

## Option C：完整 N-layer 有序合成

- Summary:
  - 每像素排序并显式合成 N 层。
- Pros:
  - 理论上控制力和保真度最高。
- Cons:
  - 成本和复杂度最高。
  - 不适合作为当前性能目标下的默认策略。

## 4. 推荐执行计划

1. Phase B6.1 - 数据模型与 payload 扩展
- 把 mask payload 从单 panel index 扩展为 top-2 panel payload。
- 保持确定性选择策略：
  - 更高 `layer_order`
  - 再更高 mask
  - 最后用稳定 index 打破平手

2. Phase B6.2 - Layered composite 实现
- 在 `CompositeMain` 中增加 front / back 双层合成模型。
- 复用现有 blur pyramid 和 optics 推导。
- 对非 overlap 像素保留 single-layer 分支。

3. Phase B6.3 - 性能 guardrail
- 增加质量模式：
  - `single`（强制当前行为）
  - `auto`（默认，按 overlap 感知）
  - `multi_layer`（强制 top-2）
- 增加 overlap-aware fallback：
  - 如果 overlap score 很低，则执行 single-layer 路径。
- 增加 overlap pixel ratio 和 path usage 的 debug counter / timing label。

4. Phase B6.4 - 稳定性与验收
- 验证 layered 结果的 temporal stability。
- 调节权重，避免快速运动下 halo pumping。
- 记录视觉和 timing 验收证据。

## 5. 建议验收与预算基线

- Functional:
  - 重叠区域表现出确定性的双层行为，并在运动中保持稳定。
- Visual:
  - 与 single-winner 基线相比，layered overlap 能明显提升深度 / 叠层感。
- Performance（`auto` 模式的初始目标）：
  - 非 overlap 主导场景：frosted pass 成本较 B3 基线上升不超过 `20%`
  - overlap 压力场景：frosted pass 成本较 B3 基线上升不超过 `35%`
  - 在检测到预算压力时，系统可回退到 single-layer

## 6. B6 主要文件

- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`

## 7. 下一步动作

- 以 Option A（top-2 layered composition）作为默认策略，先开始 B6.1 实现。
