# Feature Note - B6.1 Top-2 Multilayer Composition 核心

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260224_B6_Top2MultilayerCore.md`

- Date: 2026-02-24
- Work Item ID: B6.1
- Title: 为 frosted glass 实现 top-2 overlap payload 和 layered composite 模式
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
  - `docs/FeatureNotes/20260224_B6_MultilayerCompositionPlan.md`

## 1. 范围

- In scope:
  - 扩展 mask / parameter 阶段，使其每像素输出 top-2 panel payload（front / back）。
  - 扩展 composite 阶段，支持以下 layered mixing 模式：
    - `single`
    - `auto`
    - `multi_layer`
  - 保留 single-layer 快速路径作为基线。
  - 增加 multilayer mode 和 threshold 的 debug 控件。
- Out of scope:
  - 动态预算触发的 runtime fallback。
  - overlap 压力场景下的最终视觉验收抓图。
  - 按场景分类的最终性能验收签字。

## 2. 功能逻辑

- Behavior before:
  - Mask pass 每像素只输出一个被选中的 panel payload。
  - Composite 即使在 overlap 区域也只消费一层 panel。
- Behavior after:
  - Mask pass 现在会输出：
    - primary payload（front panel）
    - secondary payload（back panel）
  - 选择策略仍然确定且稳定：
    - 更高 `layer_order`
    - 同层时更高 mask
    - 再用 panel index 打破平手
  - Composite 现在会按模式组合 front / back 两层。
  - 在 `auto` 模式下，只有当 secondary overlap mask 超过阈值时才启用 multilayer。

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `MaskParameterMain`
  - `CompositeMain`
- Core algorithm changes:
  - 在 mask stage 中引入可复用的 panel candidate evaluation helper。
  - 在单 pass 循环里做 top-2 插入策略，不增加单独的 per-pixel sort buffer。
  - Composite 中的 layered blend：
    - 先计算 front frosted color
    - 再按需计算 / 使用 back frosted color
    - 最终以 transmittance 风格权重（`1 - front_mask`）和可调 back weight 做混合
- Parameter / model changes:
  - 新增全局参数：
    - `multilayer_mode`
    - `multilayer_overlap_threshold`
    - `multilayer_back_layer_weight`
  - 新增中间 RT：
    - `PostFX_Frosted_MaskParameter_Secondary`
    - `PostFX_Frosted_PanelOptics_Secondary`

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 5. 验证与证据

- Functional checks:
  - B6.1 接入后 verify build 成功。
- Visual checks:
  - 本次迭代未执行（互动验证待补）。
- Performance checks:
  - 本次迭代未执行（timing 采集待补）。
- Evidence files / links:
  - Build wrapper summary:
    - 仅记录本地工件名，这些文件不入库。
    - `b6_verify_wrapper_20260224_152421.stdout.log`
    - `b6_verify_wrapper_20260224_152421.stderr.log`
  - MSBuild logs:
    - 仅记录本地工件名，这些文件不入库。
    - `rendererdemo_20260224_152421.msbuild.log`
    - `rendererdemo_20260224_152421.stdout.log`
    - `rendererdemo_20260224_152421.stderr.log`
    - `rendererdemo_20260224_152421.binlog`

## 6. 验收结果

- Status: PASS（B6.1 core implementation scope，build validation）
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 76
  - Error count: 0
  - B6 仍在进行中，直到完成视觉 / 性能验收。

## 7. 风险与后续

- Known risks:
  - Back-layer weighting 仍是启发式，需要按场景调优，避免 overlap 区域过软。
  - `auto` threshold 可能需要按内容类型提供不同 profile。
- Next tasks:
  - B6.2：增加 overlap ratio counter 和动态预算感知的 fallback。
  - B6 验收：记录 overlap-heavy 场景下的视觉 / 性能证据，并最终确定默认模式。
