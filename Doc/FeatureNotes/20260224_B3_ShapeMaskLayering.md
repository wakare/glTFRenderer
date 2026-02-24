# Feature Note - B3 Shape Mask and Layering Upgrades

- Date: 2026-02-24
- Work Item ID: B3
- Title: Implement real irregular shape-mask path and deterministic panel layering policy
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Scope

- In scope:
  - Replace `ShapeMask` fallback path with real irregular SDF generation.
  - Add explicit per-panel layer order and deterministic overlap selection policy.
  - Expose shape index and layer order in debug controls.
  - Add overlapping irregular panel default setup in model-viewer demo scene.
- Out of scope:
  - Texture-driven authorable mask assets.
  - Full multi-panel interaction/selection UX.
  - Final visual/performance acceptance capture.

## 2. Functional Logic

- Behavior before:
  - `ShapeMask` was reserved/fallback and rendered as rounded-rect fallback.
  - Overlap selection used strongest local mask only, which could cause unstable selection with near-equal masks.
- Behavior after:
  - `ShapeMask` now routes to procedural irregular SDF variants (lobe/diamond/cross/hybrid) via `custom_shape_index`.
  - Panel overlap is now resolved deterministically with priority:
    - higher `layer_order` first
    - same layer -> higher local mask
    - tie -> higher panel index
  - Debug UI now exposes:
    - `Custom Shape Index`
    - `Layer Order`
  - Demo now starts with an additional overlapping `ShapeMask` panel for direct B3 validation.

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `FrostedGlass.hlsl` mask stage and panel payload layout.
- Core algorithm changes:
  - Added irregular mask SDF helpers:
    - `LobeShapeSDF`
    - `DiamondSDF`
    - `CrossSDF`
    - `ShapeMaskSDF` dispatcher
  - Added panel layering payload (`layering_info.x`) and deterministic selection logic in `MaskParameterMain`.
- Parameter/model changes:
  - Added panel descriptor field `layer_order`.
  - Added GPU payload field `layering_info`.

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`
- `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 5. Validation and Evidence

- Functional checks:
  - Verify build succeeded after B3 integration.
- Visual checks:
  - Not executed in this iteration (interactive validation pending).
- Performance checks:
  - Not executed in this iteration (timing capture pending).
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/b3_verify_wrapper_20260224_145648.stdout.log`
    - `build_logs/b3_verify_wrapper_20260224_145648.stderr.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260224_145649.msbuild.log`
    - `build_logs/rendererdemo_20260224_145649.stdout.log`
    - `build_logs/rendererdemo_20260224_145649.stderr.log`
    - `build_logs/rendererdemo_20260224_145649.binlog`

## 6. Acceptance Result

- Status: PASS (B3 core implementation scope, build validation)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 76
  - Error count: 0
  - B3 remains in progress until visual/perf acceptance evidence is captured.

## 7. Risks and Follow-up

- Known risks:
  - Current shape-mask path is procedural and not asset-authored, so exact designer intent still needs future mask-texture support.
  - Extremely close layer values can still require stricter quantization policy in content authoring.
- Next tasks:
  - B3 acceptance pass: capture overlap predictability and irregular-shape visual checklist.
  - B4 follow-up: add readability-safe behavior on top of layered mask result.
