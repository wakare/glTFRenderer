# Feature Note - A2.3 Velocity Debug View Path

- Date: 2026-02-23
- Work Item ID: A2.3
- Title: Add velocity visualization mode for motion-vector validity checks
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Scope

- In scope:
  - Add a debug-view mode in final tone-map stage to visualize base-pass velocity.
  - Bind `BasePass_Velocity` into tone-map pass as optional debug input.
  - Add runtime UI controls for debug mode and velocity scale.
- Out of scope:
  - Temporal reprojection/history consumption.
  - Velocity post-processing (dilate/filter).
  - Automated screenshot baseline update.

## 2. Functional Logic

- Behavior before:
  - Tone-map pass only consumed frosted final color and output display color.
  - Velocity buffer existed (A2.2) but had no visualization path.
- Behavior after:
  - Tone-map pass supports `Debug View = Velocity`.
  - In velocity mode, output color encodes:
    - R/G: signed UV motion direction and magnitude around neutral gray.
    - B: motion speed intensity.
  - `Velocity Scale` controls visual amplification for motion readability.

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `RendererSystemToneMap` pass bindings and UI controls.
  - `ToneMap.hlsl` debug branch.
- Core algorithm changes:
  - Add a debug branch in tone-map compute shader:
    - sample `InputVelocityTex.xy`
    - scale by `debug_velocity_scale`
    - map to debug color and bypass tone-map operator branch.
- Parameter/model changes:
  - `ToneMapGlobalParams` adds:
    - `debug_view_mode` (`0=Final`, `1=Velocity`)
    - `debug_velocity_scale`

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemToneMap.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemToneMap.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/ToneMap.hlsl`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`

## 5. Validation and Evidence

- Functional checks:
  - Verify build succeeded with new tone-map bindings and constant-buffer layout changes.
- Visual checks:
  - Not executed in this iteration (build-only validation scope).
- Performance checks:
  - Not executed in this iteration.
- Evidence files/links:
  - MSBuild summary:
    - `build_logs/rendererdemo_20260223_194613.msbuild.log`
  - Standard outputs:
    - `build_logs/rendererdemo_20260223_194613.stdout.log`
    - `build_logs/rendererdemo_20260223_194613.stderr.log`
  - Binlog:
    - `build_logs/rendererdemo_20260223_194613.binlog`

## 6. Acceptance Result

- Status: PASS (build validation scope)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 30
  - Error count: 0

## 7. Risks and Follow-up

- Known risks:
  - Runtime visual correctness is not validated in this note.
  - Velocity visualization relies on UV-space convention; downstream temporal passes must use matching convention.
- Next tasks:
  - A2 close-out: runtime visual validation screenshots/checklist under camera movement.
  - A4.x: consume velocity with history path and rejection logic.
