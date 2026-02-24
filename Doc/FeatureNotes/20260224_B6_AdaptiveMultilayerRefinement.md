# Feature Note - B6.2 Adaptive Multilayer Refinement

- Date: 2026-02-24
- Work Item ID: B6.2
- Title: Refine layered composite ordering and add auto-mode runtime budget fallback
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
  - `Doc/FeatureNotes/20260224_B6_Top2MultilayerCore.md`

## 1. Scope

- In scope:
  - Adjust multilayer composite behavior to reduce "front layer appears clearer than back layer" artifacts in overlap regions.
  - Add runtime budget-aware fallback in `auto` mode for multilayer path.
  - Add debug UI for budget and runtime state observation.
- Out of scope:
  - Final artifact tuning for all scene classes.
  - Full GPU-time-driven dynamic quality adaptation.

## 2. Functional Logic

- Behavior before:
  - Front and back frosted colors were both evaluated against original scene color, then blended.
  - In some overlaps, front-layer result could visually look sharper than expected.
  - `auto` mode had no runtime budget fallback.
- Behavior after:
  - Multilayer path now evaluates back layer first, then evaluates front layer over back-composited base.
  - This reduces overlap cases where top layer appears unexpectedly clearer than underlying layer.
  - `auto` mode now has budget fallback:
    - tracks over-budget streak
    - disables multilayer for cooldown frames when budget pressure persists
  - Runtime state is exposed in debug UI.

## 3. Algorithm and Runtime Changes

- Composite refinement:
  - `back_composited_color = blend(scene, back_layer)`
  - `front_layer` is then re-evaluated with `back_composited_color` as base input.
- Adaptive runtime guard:
  - New params/state:
    - `multilayer_runtime_enabled`
    - `multilayer_frame_budget_ms`
    - over-budget streak counter + cooldown frame counter
  - Mode behavior:
    - `single`: always off
    - `multi_layer`: always on
    - `auto`: overlap + runtime budget guarded

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 5. Validation and Evidence

- Functional checks:
  - Verify build succeeded after B6.2 integration.
- Visual checks:
  - Interactive visual confirmation pending (user-facing validation step).
- Performance checks:
  - Runtime fallback counters exposed; full timing acceptance pending.
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/b62_verify_wrapper_20260224_154326.stdout.log`
    - `build_logs/b62_verify_wrapper_20260224_154326.stderr.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260224_154327.msbuild.log`
    - `build_logs/rendererdemo_20260224_154327.stdout.log`
    - `build_logs/rendererdemo_20260224_154327.stderr.log`
    - `build_logs/rendererdemo_20260224_154327.binlog`

## 6. Acceptance Result

- Status: PASS (B6.2 core implementation scope, build validation)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 146
  - Error count: 0
  - B6 overall remains in progress until visual/perf acceptance sign-off.

## 7. Risks and Follow-up

- Known risks:
  - Frame-time proxy (`interval`) is CPU-side and may not perfectly represent GPU pressure.
  - Further artistic tuning may be required for overlap-heavy high-contrast scenes.
- Next tasks:
  - B6 acceptance pass: run overlap-heavy motion scenes and tune defaults (`back weight`, `overlap threshold`, `budget ms`).
  - Optional: add GPU timing-based quality switch for more accurate runtime fallback.
