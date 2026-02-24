# Feature Note - B2 Panel State Machine and Runtime Control

- Date: 2026-02-24
- Work Item ID: B2
- Title: Add panel interaction state machine with runtime state curves and input-driven transitions
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Scope

- In scope:
  - Add panel interaction states: `Idle`, `Hover`, `Grab`, `Move`, `Scale`.
  - Add per-panel state-curve model to drive blur/rim/fresnel/alpha behavior.
  - Add runtime transition smoothing and per-frame state blending.
  - Add demo-side runtime input mapping for state changes on panel `0`.
  - Expose debug UI controls for state, transition speed, and per-state curve tuning.
- Out of scope:
  - Multi-panel hit-test and selection routing.
  - Full interaction semantics for panel transform manipulation.
  - Final visual acceptance capture (screenshots + motion checklist).

## 2. Functional Logic

- Behavior before:
  - Frosted panels had static parameters only.
  - No explicit interaction state model existed.
  - Runtime did not drive blur/rim/fresnel/alpha from interaction state transitions.
- Behavior after:
  - Panel descriptor now includes:
    - target interaction state
    - transition speed
    - panel alpha
    - per-state curve set
  - Runtime now maintains blended state weights and updates them each frame with smooth exponential transition.
  - GPU upload path now applies blended state curves to:
    - blur sigma
    - blur strength
    - rim intensity
    - fresnel intensity
    - panel alpha
  - Demo runtime input mapping (panel `0`):
    - mouse move -> `Hover`
    - `LMB` -> `Move`
    - `RMB` -> `Grab`
    - `Ctrl + LMB` -> `Scale`
    - otherwise -> `Idle`
- Key runtime flow:
  - `Input -> target state -> blended state weights -> panel GPU parameters -> mask/composite shading`

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `RendererSystemFrostedGlass` runtime state update + panel upload conversion.
  - `FrostedGlass.hlsl` uses `shape_info.w` as panel alpha in mask stage.
- Core algorithm changes:
  - Introduced state-weight blending:
    - `weight += (target - weight) * (1 - exp(-speed * dt))`
  - Blended per-state curves are normalized and applied during GPU parameter packing.
  - Mask stage now multiplies panel SDF mask by runtime panel alpha.
- Parameter/model changes:
  - New panel interaction model (`PanelInteractionState`, `PanelStateCurve`).
  - New debug controls for interaction state and curve tuning.
  - Demo toggle: enable/disable input-driven state machine.

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.h`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`
- `scripts/Build-RendererDemo-Verify.ps1`

## 5. Validation and Evidence

- Functional checks:
  - Verify build succeeded after B2 integration.
  - Build script now reports explicit PID/duration/status to avoid confusion with Rider background MSBuild.
- Visual checks:
  - Not executed in this iteration (interactive validation pending).
- Performance checks:
  - Not executed in this iteration (timing capture pending).
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/b2_verify_wrapper2_20260224_123446.stdout.log`
    - `build_logs/b2_verify_wrapper2_20260224_123446.stderr.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260224_123446.msbuild.log`
    - `build_logs/rendererdemo_20260224_123446.stdout.log`
    - `build_logs/rendererdemo_20260224_123446.stderr.log`
    - `build_logs/rendererdemo_20260224_123446.binlog`

## 6. Acceptance Result

- Status: PASS (B2 core implementation scope, build validation)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 0
  - Error count: 0
  - B2 remains in progress until runtime visual stability checklist is captured.

## 7. Risks and Follow-up

- Known risks:
  - Current demo input mapping drives panel `0` only and does not include full multi-panel hit testing.
  - Overlap/selection behavior is not finalized and may need alignment with `B3` layering rules.
- Next tasks:
  - B2 acceptance pass: capture smooth transition evidence under interaction/motion.
  - B2 extension: add multi-panel targeting strategy for runtime interactions.
  - M2/M3 follow-up: integrate with temporal stabilization (`A4`) and validate flicker behavior.
