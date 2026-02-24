# Feature Note - B6.1 Top-2 Multilayer Composition Core

- Date: 2026-02-24
- Work Item ID: B6.1
- Title: Implement top-2 overlap payload and layered composite modes for frosted glass
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
  - `Doc/FeatureNotes/20260224_B6_MultilayerCompositionPlan.md`

## 1. Scope

- In scope:
  - Extend mask/parameter stage to output top-2 panel payloads per pixel (front/back).
  - Extend composite stage to support layered mixing with mode control:
    - `single`
    - `auto`
    - `multi_layer`
  - Keep single-layer fast path as baseline.
  - Add debug controls for multilayer mode and thresholds.
- Out of scope:
  - Dynamic budget-triggered runtime fallback.
  - Final visual acceptance capture for overlap stress scenarios.
  - Final per-scene performance acceptance sign-off.

## 2. Functional Logic

- Behavior before:
  - Mask pass emitted one selected panel payload per pixel.
  - Composite consumed one panel only, even in overlap regions.
- Behavior after:
  - Mask pass now emits:
    - primary payload (front panel)
    - secondary payload (back panel)
  - Selection policy remains deterministic and stable:
    - higher `layer_order`
    - same layer then higher mask
    - tie by panel index
  - Composite can now combine front/back layers depending on mode.
  - `auto` mode enables multilayer only when secondary overlap mask exceeds threshold.

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `MaskParameterMain`
  - `CompositeMain`
- Core algorithm changes:
  - Introduced reusable panel candidate evaluation helper for mask stage.
  - Top-2 insertion strategy inside one pass loop (no per-pixel sort buffer).
  - Layered blend in composite:
    - compute front frosted color
    - optionally compute/use back frosted color
    - blend with transmittance-like weighting (`1 - front_mask`) and configurable back weight
- Parameter/model changes:
  - New global parameters:
    - `multilayer_mode`
    - `multilayer_overlap_threshold`
    - `multilayer_back_layer_weight`
  - New intermediate RTs:
    - `PostFX_Frosted_MaskParameter_Secondary`
    - `PostFX_Frosted_PanelOptics_Secondary`

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 5. Validation and Evidence

- Functional checks:
  - Verify build succeeded after B6.1 integration.
- Visual checks:
  - Not executed in this iteration (interactive validation pending).
- Performance checks:
  - Not executed in this iteration (timing capture pending).
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/b6_verify_wrapper_20260224_152421.stdout.log`
    - `build_logs/b6_verify_wrapper_20260224_152421.stderr.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260224_152421.msbuild.log`
    - `build_logs/rendererdemo_20260224_152421.stdout.log`
    - `build_logs/rendererdemo_20260224_152421.stderr.log`
    - `build_logs/rendererdemo_20260224_152421.binlog`

## 6. Acceptance Result

- Status: PASS (B6.1 core implementation scope, build validation)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 76
  - Error count: 0
  - B6 remains in progress until visual/performance acceptance is complete.

## 7. Risks and Follow-up

- Known risks:
  - Back-layer weighting is heuristic and needs scene tuning to avoid over-soft overlap regions.
  - `auto` threshold may need per-content profile presets.
- Next tasks:
  - B6.2: add overlap ratio counters and dynamic budget-aware fallback.
  - B6 acceptance: capture overlap-heavy visual/perf evidence and finalize default mode settings.
