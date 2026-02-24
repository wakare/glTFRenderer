# Feature Note - B1.3 Frosted Mask/Parameter Stage Split

- Date: 2026-02-24
- Work Item ID: B1.3
- Title: Add explicit mask/parameter stage and route composite through intermediate panel data
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Scope

- In scope:
  - Add full-resolution mask/parameter compute pass before frosted composite.
  - Add dedicated full-resolution intermediate render targets for panel mask/optics payload.
  - Update frosted composite shader to consume precomputed mask/optics data instead of re-evaluating panel shape logic in composite.
  - Keep half/quarter blur pyramid path intact and reuse existing panel buffer schema.
- Out of scope:
  - Temporal accumulation/stabilization.
  - Irregular shape-mask implementation (`ShapeMask` real path).
  - Final visual/performance acceptance against frozen baseline.

## 2. Functional Logic

- Behavior before:
  - Frosted path had downsample/blur stages, then one composite pass that also performed panel SDF/mask/optics evaluation per pixel.
  - Composite pass sampled scene depth/normal and evaluated panel geometry directly.
- Behavior after:
  - Frosted path now has explicit stages:
    1. `Downsample Half`
    2. `Blur Half Horizontal`
    3. `Blur Half Vertical`
    4. `Downsample Quarter`
    5. `Blur Quarter Horizontal`
    6. `Blur Quarter Vertical`
    7. `Frosted Mask/Parameter`
    8. `Frosted Composite`
  - `Frosted Mask/Parameter` writes:
    - `MaskParamOutput` (mask/rim/fresnel/panel index)
    - `PanelOpticsOutput` (refraction uv/effective blur/scene edge)
  - `Frosted Composite` now consumes:
    - `MaskParamTex`
    - `PanelOpticsTex`
    - existing half/quarter blurred textures
- Key runtime flow:
  - `Lighting -> Blur Pyramid -> Mask/Parameter -> Composite -> Frosted Output`

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `RendererSystemFrostedGlass` pass graph wiring and resource creation.
  - `FrostedGlass.hlsl` split into `MaskParameterMain` and `CompositeMain`.
- Core algorithm changes:
  - Extracted per-pixel panel mask and optics derivation from composite into a dedicated compute pass.
  - Composite now uses prepacked intermediate data and focuses on blur reconstruction + highlight composition.
  - For overlapping panels in this iteration, mask stage selects the strongest local panel mask payload for composite input.
- Parameter/model changes:
  - No CPU-side panel schema change.
  - Added two full-resolution postfx intermediates:
    - `PostFX_Frosted_MaskParameter`
    - `PostFX_Frosted_PanelOptics`

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`

## 5. Validation and Evidence

- Functional checks:
  - `RendererDemo` verify build succeeded after mask/parameter stage split integration.
- Visual checks:
  - Not executed in this iteration (interactive validation pending).
- Performance checks:
  - Not executed in this iteration (timing capture pending).
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/b13_verify_20260224_115727.stdout.log`
    - `build_logs/b13_verify_20260224_115727.stderr.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260224_115728.msbuild.log`
    - `build_logs/rendererdemo_20260224_115728.stdout.log`
    - `build_logs/rendererdemo_20260224_115728.stderr.log`
    - `build_logs/rendererdemo_20260224_115728.binlog`

## 6. Acceptance Result

- Status: PASS (B1.3 implementation scope, build validation)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 0
  - Error count: 0
  - B1 remains in progress until visual/perf acceptance evidence is completed.

## 7. Risks and Follow-up

- Known risks:
  - Overlap behavior currently uses strongest local mask payload, which may differ from future robust layering policy planned in `B3`.
  - Runtime quality under motion (edge stability/halo behavior) still requires visual validation.
- Next tasks:
  - B1 acceptance pass: capture v1 vs v2 screenshots and per-pass timing evidence.
  - B2 implementation: panel state machine integration for runtime parameter transitions.
  - B3 follow-up: replace fallback overlap behavior with explicit layering policy for irregular masks.
