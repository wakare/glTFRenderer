# Feature Note - B3 Shape Mask 与层叠升级

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260224_B3_ShapeMaskLayering.md`

- Date: 2026-02-24
- Work Item ID: B3
- Title: 实现真实的不规则 shape-mask 路径和确定性的 panel layering 策略
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. 范围

- In scope:
  - 把 `ShapeMask` 的 fallback 路径替换成真实的不规则 SDF 生成。
  - 增加显式的 per-panel layer order 和确定性的 overlap 选择策略。
  - 在 debug 控件中暴露 shape index 和 layer order。
  - 在 model-viewer demo scene 中增加默认重叠的 `ShapeMask` panel 用于验证。
- Out of scope:
  - 基于纹理的可创作 mask 资产。
  - 完整的多 panel 交互 / 选择 UX。
  - 最终视觉 / 性能验收抓图。

## 2. 功能逻辑

- Behavior before:
  - `ShapeMask` 只是保留 / fallback，实际渲染时仍回退成 rounded-rect。
  - Overlap 选择使用 strongest local mask，会在 mask 接近时导致不稳定。
- Behavior after:
  - `ShapeMask` 现在通过 `custom_shape_index` 路由到程序化 irregular SDF 变体（lobe / diamond / cross / hybrid）。
  - Panel overlap 现在按以下优先级确定性解析：
    - 更高的 `layer_order`
    - 同层时更高的 local mask
    - 再相同则用更高的 panel index 打破平手
  - Debug UI 新增：
    - `Custom Shape Index`
    - `Layer Order`
  - Demo 启动场景中会多出一块重叠的 `ShapeMask` panel，便于直接验证 B3。

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `FrostedGlass.hlsl` 的 mask stage 和 panel payload 布局。
- Core algorithm changes:
  - 新增 irregular mask SDF helper：
    - `LobeShapeSDF`
    - `DiamondSDF`
    - `CrossSDF`
    - `ShapeMaskSDF` dispatcher
  - 在 `MaskParameterMain` 中增加 panel layering payload（`layering_info.x`）和确定性选择逻辑。
- Parameter / model changes:
  - Panel descriptor 新增字段 `layer_order`。
  - GPU payload 新增字段 `layering_info`。

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`
- `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 5. 验证与证据

- Functional checks:
  - 接入 B3 后 verify build 成功。
- Visual checks:
  - 本次迭代未执行（互动验证待补）。
- Performance checks:
  - 本次迭代未执行（timing 采集待补）。
- Evidence files / links:
  - Build wrapper summary:
    - 仅记录本地工件名，这些文件不入库。
    - `b3_verify_wrapper_20260224_145648.stdout.log`
    - `b3_verify_wrapper_20260224_145648.stderr.log`
  - MSBuild logs:
    - 仅记录本地工件名，这些文件不入库。
    - `rendererdemo_20260224_145649.msbuild.log`
    - `rendererdemo_20260224_145649.stdout.log`
    - `rendererdemo_20260224_145649.stderr.log`
    - `rendererdemo_20260224_145649.binlog`

## 6. 验收结果

- Status: PASS（B3 core implementation scope，build validation）
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 76
  - Error count: 0
  - B3 仍在进行中，直到补齐视觉 / 性能验收证据。

## 7. 风险与后续

- Known risks:
  - 当前 shape-mask 路径仍是程序化生成，而不是 asset-authored，因此未来仍需要纹理 mask 支持来表达更强的设计意图。
  - 当 layer 值极其接近时，内容制作层面可能仍需要更严格的量化策略。
- Next tasks:
  - B3 验收：抓取 overlap predictability 和 irregular-shape visual checklist。
  - B4 跟进：在 layered mask 结果之上增加 readability-safe 行为。
