# Feature Note - B1.2 Quarter Blur Pyramid 集成

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260223_B1_QuarterBlurPyramid.md`

- Date: 2026-02-23
- Work Item ID: B1.2
- Title: 把 quarter-resolution blur 链接入 frosted multipass composite
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. 范围

- In scope:
  - 在现有 half 链基础上增加 quarter-resolution 的 downsample + separable blur pass。
  - 把 quarter blurred output 接到 frosted composite shader。
  - 按 panel blur sigma 在 half / quarter blurred signal 之间做混合。
- Out of scope:
  - Temporal history accumulation。
  - 专门的 panel mask / parameter 预处理 pass。
  - 可读性保护和 immersion 策略类工作项。

## 2. 功能逻辑

- Behavior before:
  - Frosted multipass 只使用 half-resolution blur 链。
  - Composite 只采样一张 blurred texture（`BlurredColorTex`）。
- Behavior after:
  - 流水线现在包含：
    1. `Downsample Half`
    2. `Blur Half Horizontal`
    3. `Blur Half Vertical`
    4. `Downsample Quarter`
    5. `Blur Quarter Horizontal`
    6. `Blur Quarter Vertical`
    7. `Frosted Composite`
  - Composite 会同时采样：
    - `BlurredColorTex`（half 结果）
    - `QuarterBlurredColorTex`（quarter 结果）
  - 当 blur sigma 更大时，quarter 分量会占更高权重。

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `RendererSystemFrostedGlass` 的 pass graph wiring。
  - `FrostedGlass.hlsl` 的 composite sampling 与 blend 逻辑。
- Core algorithm changes:
  - 在 half blur final output 的基础上增加 quarter pyramid 分支。
  - 新增基于 sigma 的混合：

```hlsl
quarter_blend = saturate((panel_blur_sigma - 4.0f) / 6.0f);
```

  - 最终的 blurred sample 为：`lerp(half_blur, quarter_blur, quarter_blend)`。
- Parameter / model changes:
  - CPU 侧 panel schema 无变化。
  - 现有 `panel_blur_sigma` 现在也会控制 half / quarter blur 的混合权重。

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`

## 5. 验证与证据

- Functional checks:
  - Quarter 链接入后，`RendererDemo` verify build 成功。
- Visual checks:
  - 本记录未包含（运行时视觉确认仍待互动验证）。
- Performance checks:
  - 本记录未包含。
- Evidence files / links:
  - MSBuild summary:
    - `build_logs/rendererdemo_20260223_214341.msbuild.log`
  - Standard outputs:
    - `build_logs/rendererdemo_20260223_214341.stdout.log`
    - `build_logs/rendererdemo_20260223_214341.stderr.log`
  - Binlog:
    - `build_logs/rendererdemo_20260223_214341.binlog`

## 6. 验收结果

- Status: PASS（B1.2 implementation scope，build validation）
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 65
  - Error count: 0
  - B1 仍在进行中，直到完成视觉 / 性能验收以及完整替换目标。

## 7. 风险与后续

- Known risks:
  - Sigma threshold 和 quarter mix ratio 的视觉调优可能仍需按场景细化。
  - 运动中的边缘质量和时间稳定性仍需运行时验证。
- Next tasks:
  - B1.3：拆出 mask / parameter stage，并继续打磨 compositing。
  - B1 视觉 / 性能验收：对照 v1 基线抓截图并补 timing 证据。
