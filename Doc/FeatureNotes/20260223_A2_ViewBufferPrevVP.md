# Feature Note - A2.1 ViewBuffer Previous View-Projection Matrix Plumbing

- Date: 2026-02-23
- Work Item ID: A2.1
- Title: Add `prev_view_projection_matrix` to view buffer and upload path
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Scope

- In scope:
  - Extend `ViewBuffer` with `prev_view_projection_matrix`.
  - Upload previous-frame matrix from camera module every frame.
  - Reset previous-frame state on viewport resize.
  - Keep shadow view buffer consistent with the new layout.
- Out of scope:
  - Velocity buffer generation.
  - Temporal reprojection logic and history blending.
  - Any visual algorithm that consumes previous-frame matrix.

## 2. Functional Logic

- Behavior before:
  - `ViewBuffer` provided only current `view_projection_matrix` and inverse matrix.
- Behavior after:
  - `ViewBuffer` carries both current and previous view-projection matrices.
  - First frame and resize boundary use current matrix as previous fallback to avoid invalid history.
- Key runtime flow:
  - Camera module computes current VP.
  - Upload uses cached previous VP (or current if uninitialized).
  - Cache is advanced after upload.

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `RendererModuleCamera` CPU-side upload path.
  - `SceneViewCommon.hlsl` constant buffer layout.
  - Directional shadow setup path writing `ViewBuffer`.
- Core algorithm changes:
  - No new rendering algorithm yet; this is data plumbing for later temporal steps.
- Parameter/model changes:
  - New field: `prev_view_projection_matrix` in both C++ and HLSL view buffer definitions.

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.h`
- `glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/SceneViewCommon.hlsl`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`

## 5. Validation and Evidence

- Functional checks:
  - `RendererDemo` verify build succeeded after ABI/layout change.
- Visual checks:
  - Not executed in this iteration (plumbing-only change).
- Performance checks:
  - Not executed in this iteration.
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/build_verify_20260223_184547.stdout.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260223_184548.msbuild.log`
    - `build_logs/rendererdemo_20260223_184548.stdout.log`
    - `build_logs/rendererdemo_20260223_184548.stderr.log`
    - `build_logs/rendererdemo_20260223_184548.binlog`

## 6. Acceptance Result

- Status: PASS (build validation scope)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 30 (existing warning baseline)
  - Error count: 0

## 7. Risks and Follow-up

- Known risks:
  - Matrix is uploaded but currently unused by shaders; no immediate visual benefit expected.
- Next tasks:
  - A2.2: introduce velocity or reprojection consumer path using previous VP.
  - A2.3: add debug visualization to validate previous/current matrix behavior in motion.
