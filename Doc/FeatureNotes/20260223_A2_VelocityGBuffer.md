# Feature Note - A2.2 Velocity GBuffer Output Path (Producer Only)

- Date: 2026-02-23
- Work Item ID: A2.2
- Title: Add base-pass velocity render target and shader motion-vector output
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Scope

- In scope:
  - Add a new base-pass render target for velocity output.
  - Extend model rendering shader to write per-pixel motion vector.
  - Expose velocity RT handle through `RendererSystemOutput<RendererSystemSceneRenderer>`.
- Out of scope:
  - Temporal history buffer consumption.
  - Motion-vector debug visualization UI.
  - Temporal reprojection or denoise logic.

## 2. Functional Logic

- Behavior before:
  - Base pass wrote color/normal/depth only.
  - No dedicated velocity channel was generated.
- Behavior after:
  - Base pass writes `color + normal + velocity + depth`.
  - Velocity is encoded as UV-space delta:
    - `velocity_uv = current_uv - previous_uv`
  - Previous position uses `prev_view_projection_matrix` from `ViewBuffer` (added in A2.1).
- Key runtime flow:
  - VS computes previous-frame clip position from current world position.
  - FS reconstructs `previous_uv` and writes velocity to `SV_TARGET2`.

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `RendererSystemSceneRenderer` base pass MRT layout.
  - `ModelRenderingShader.hlsl` VS/FS outputs.
- Core algorithm changes:
  - Added producer-side motion vector output in base pass.
  - VS exports both current and previous clip-space positions.
  - FS computes velocity from NDC delta and converts to UV-space delta (without reading `ViewBuffer`).
- Parameter/model changes:
  - New RT: `BasePass_Velocity` (`RGBA16_FLOAT`).
  - New FS output: `velocity : SV_TARGET2`.

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSceneRenderer.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSceneRenderer.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/ModelRenderingShader.hlsl`

## 5. Validation and Evidence

- Functional checks:
  - `RendererDemo` verify build succeeded with new MRT layout and shader outputs.
- Visual checks:
  - Not executed in this iteration (producer-only path).
- Performance checks:
  - Not executed in this iteration.
- Evidence files/links:
  - MSBuild summary:
    - `build_logs/rendererdemo_20260223_185103.msbuild.log`
  - Standard outputs:
    - `build_logs/rendererdemo_20260223_185103.stdout.log`
    - `build_logs/rendererdemo_20260223_185103.stderr.log`
  - Binlog:
    - `build_logs/rendererdemo_20260223_185103.binlog`

## 6. Acceptance Result

- Status: PASS (build validation scope)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 7
  - Error count: 0

## 7. Risks and Follow-up

- Known risks:
  - Velocity currently uses current-frame instance transform for both current/previous positions; animated transform history is not yet modeled.
  - No runtime debug view yet to verify vector direction/magnitude visually.
- Next tasks:
  - A2.3: add debug visualization for velocity and validity checks.
  - A4.x: consume velocity/history in temporal stabilization pipeline.
