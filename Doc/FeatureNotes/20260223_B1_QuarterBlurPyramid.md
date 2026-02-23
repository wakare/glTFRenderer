# Feature Note - B1.2 Quarter Blur Pyramid Integration

- Date: 2026-02-23
- Work Item ID: B1.2
- Title: Integrate quarter-resolution blur chain into frosted multipass composite
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Scope

- In scope:
  - Add quarter-resolution downsample + separable blur passes on top of existing half chain.
  - Wire quarter blurred output into frosted composite shader.
  - Blend half/quarter blurred signals based on panel blur sigma.
- Out of scope:
  - Temporal history accumulation.
  - Dedicated panel mask/parameter pre-pass.
  - Readability and immersion policy work items.

## 2. Functional Logic

- Behavior before:
  - Frosted multipass used half-resolution blur chain only.
  - Composite sampled one blurred texture (`BlurredColorTex`).
- Behavior after:
  - Pipeline now includes:
    1. `Downsample Half`
    2. `Blur Half Horizontal`
    3. `Blur Half Vertical`
    4. `Downsample Quarter`
    5. `Blur Quarter Horizontal`
    6. `Blur Quarter Vertical`
    7. `Frosted Composite`
  - Composite samples both:
    - `BlurredColorTex` (half result)
    - `QuarterBlurredColorTex` (quarter result)
  - Quarter contribution increases for larger blur sigma values.

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `RendererSystemFrostedGlass` pass graph wiring.
  - `FrostedGlass.hlsl` composite sampling and blend logic.
- Core algorithm changes:
  - Added quarter pyramid branch sourced from half blur final output.
  - Added sigma-driven blend:
    - `quarter_blend = saturate((panel_blur_sigma - 4.0f) / 6.0f)`
    - final blurred sample = `lerp(half_blur, quarter_blur, quarter_blend)`
- Parameter/model changes:
  - No CPU panel schema change.
  - Existing `panel_blur_sigma` now also controls half/quarter blur mix.

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`

## 5. Validation and Evidence

- Functional checks:
  - `RendererDemo` verify build succeeded after quarter chain integration.
- Visual checks:
  - Not recorded in this note (runtime visual confirmation pending interactive validation).
- Performance checks:
  - Not recorded in this note.
- Evidence files/links:
  - MSBuild summary:
    - `build_logs/rendererdemo_20260223_214341.msbuild.log`
  - Standard outputs:
    - `build_logs/rendererdemo_20260223_214341.stdout.log`
    - `build_logs/rendererdemo_20260223_214341.stderr.log`
  - Binlog:
    - `build_logs/rendererdemo_20260223_214341.binlog`

## 6. Acceptance Result

- Status: PASS (B1.2 implementation scope, build validation)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 65
  - Error count: 0
  - B1 remains in progress until visual/perf acceptance and full replacement criteria are met.

## 7. Risks and Follow-up

- Known risks:
  - Visual tuning for sigma thresholds and quarter mix ratio may need scene-specific adjustment.
  - Runtime validation still required for edge quality and temporal stability under motion.
- Next tasks:
  - B1.3: add mask/parameter stage split and compositing polish.
  - B1 visual/perf acceptance pass: compare against v1 baseline and capture timing evidence.
