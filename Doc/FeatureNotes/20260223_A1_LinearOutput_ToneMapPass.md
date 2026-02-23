# Feature Note - A1.1 Linear Lighting Output and Independent Tone Map Pass

- Date: 2026-02-23
- Work Item ID: A1.1
- Title: Linear lighting output path with dedicated tone mapping final pass
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Scope

- In scope:
  - Remove direct sRGB conversion from scene lighting pass output.
  - Add a dedicated tone map compute pass as final color output.
  - Upgrade intermediate lighting/frosted outputs to `RGBA16_FLOAT`.
  - Wire tone map system into `DemoAppModelViewer` runtime pipeline.
- Out of scope:
  - Full HDR exposure control strategy across all systems.
  - Temporal stabilization and history buffers.
  - Advanced tone-mapping operator tuning and color grading pipeline.

## 2. Functional Logic

- Behavior before:
  - `SceneLighting.hlsl` emitted `LinearToSrgb(final_lighting)` directly.
  - Frosted pass output was final frame output.
- Behavior after:
  - Lighting pass emits linear color.
  - Frosted pass processes linear input/output in float format.
  - New `RendererSystemToneMap` runs after frosted and becomes final output.
- Key runtime flow:
  - `Scene -> Lighting(linear) -> Frosted(linear) -> ToneMap(sRGB output) -> Swapchain`

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `SceneLighting.hlsl`: remove inline sRGB conversion.
  - `FrostedGlass` system output format set to `RGBA16_FLOAT`.
  - New `ToneMap.hlsl`: exposure + operator selection (`Reinhard` / `ACES`) + gamma output transform.
- Core algorithm changes:
  - Move transfer-function responsibility to a dedicated terminal pass.
- Parameter/model changes:
  - New tone map global params: `exposure`, `gamma`, `tone_map_mode`.

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemToneMap.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemToneMap.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/ToneMap.hlsl`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.h`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`
- `glTFRenderer/RendererDemo/RendererDemo.vcxproj`
- `glTFRenderer/RendererDemo/RendererDemo.vcxproj.filters`

## 5. Validation and Evidence

- Functional checks:
  - Build completed successfully for `RendererDemo` target.
  - New system compiles and is linked in runtime system list.
- Visual checks:
  - Not executed in this iteration (build-only validation).
- Performance checks:
  - Not executed in this iteration (build-only validation).
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/build_verify_20260223_183108.stdout.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260223_183109.msbuild.log`
    - `build_logs/rendererdemo_20260223_183109.stdout.log`
    - `build_logs/rendererdemo_20260223_183109.stderr.log`
    - `build_logs/rendererdemo_20260223_183109.binlog`

## 6. Acceptance Result

- Status: PASS (build validation scope)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 30 (pre-existing warning classes `C4099`, `C4267`, `C4244`)
  - Error count: 0

## 7. Risks and Follow-up

- Known risks:
  - Visual regression not yet validated at runtime (needs screenshot/perceptual check).
  - Tone map defaults may require scene-dependent tuning.
- Next tasks:
  - A1.2: runtime visual verification and baseline screenshot update.
  - A1.3: evaluate tone-map operator and exposure defaults against acceptance targets.
