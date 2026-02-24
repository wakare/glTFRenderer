# Feature Note - A4 Temporal History Infrastructure

- Date: 2026-02-24
- Work Item ID: A4
- Title: Add frosted-glass temporal history lifecycle, ping-pong routing, and invalidation path
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Scope

- In scope:
  - Add full-resolution temporal history render targets for frosted composite.
  - Add frame-to-frame history ping-pong (`A->B`, `B->A`) and per-frame active-node selection.
  - Add temporal invalidation/reset paths on resize and camera jump.
  - Add velocity-based reprojection and rejection controls in frosted composite shader.
  - Add temporal debug controls in frosted debug UI.
- Out of scope:
  - Neighborhood clamping / advanced TAA-style history clamp.
  - Final visual checklist capture under camera/panel stress paths.
  - Final performance acceptance capture.

## 2. Functional Logic

- Behavior before:
  - Frosted composite had no temporal history input or lifecycle.
  - No dedicated history reset policy existed for resize/camera jump.
- Behavior after:
  - Frosted now owns two history RTs:
    - `PostFX_Frosted_History_A`
    - `PostFX_Frosted_History_B`
  - Two composite nodes are created and alternated per frame:
    - read `A` write `B`
    - read `B` write `A`
  - Camera module now exposes a consumable invalidation flag for temporal consumers.
  - History validity is reset on:
    - resize
    - detected camera jump (large view-projection delta)
    - explicit debug reset button
  - Composite shader now writes both:
    - final frosted output
    - next-frame history output

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `RendererSystemFrostedGlass` pass graph wiring and runtime state management.
  - `RendererModuleCamera` temporal invalidation signaling.
  - `FrostedGlass.hlsl` temporal reprojection blend in `CompositeMain`.
- Core algorithm changes:
  - Reproject history by velocity:
    - `prev_uv = current_uv - velocity`
  - Reject history with velocity and edge terms.
  - Blend history only within effective panel mask coverage.
  - Keep per-frame history-valid flag in global params and upload path.
- Parameter/model changes:
  - Added global temporal controls:
    - `temporal_history_blend`
    - `temporal_reject_velocity`
    - `temporal_edge_reject`
    - `temporal_history_valid`

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.h`
- `glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.cpp`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 5. Validation and Evidence

- Functional checks:
  - Verify build succeeded after A4 integration.
- Visual checks:
  - Not executed in this iteration (interactive validation pending).
- Performance checks:
  - Not executed in this iteration (timing capture pending).
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/a4_verify_wrapper_20260224_125940.stdout.log`
    - `build_logs/a4_verify_wrapper_20260224_125940.stderr.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260224_125940.msbuild.log`
    - `build_logs/rendererdemo_20260224_125940.stdout.log`
    - `build_logs/rendererdemo_20260224_125940.stderr.log`
    - `build_logs/rendererdemo_20260224_125940.binlog`

## 6. Acceptance Result

- Status: PASS (A4 core implementation scope, build validation)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 76
  - Error count: 0
  - A4 remains in progress until visual and performance acceptance evidence is captured.

## 7. Risks and Follow-up

- Known risks:
  - Current camera-jump detection uses matrix-delta threshold and may require threshold tuning for different camera motion styles.
  - No neighborhood color clamp is applied, so fast high-contrast motion may still show temporal artifacts.
- Next tasks:
  - A4 acceptance pass: validate motion stability and resize-reset behavior via interactive checklist.
  - M3 follow-up: run B2 state transition scenarios with temporal enabled and capture artifact review.
