# Feature Note - B1.1 Frosted Glass Multipass Scaffold (Downsample + Blur + Composite)

- Date: 2026-02-23
- Work Item ID: B1.1
- Title: Replace single-pass frosted processing with multipass postfx scaffold
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Scope

- In scope:
  - Introduce multipass frosted graph wiring:
    - half-resolution downsample pass
    - half-resolution separable blur horizontal/vertical passes
    - full-resolution composite pass
  - Consume `PostFxSharedResources` half ping-pong resources in runtime passes.
  - Keep existing panel parameter model and debug UI controls available.
- Out of scope:
  - Dedicated mask/parameter pre-pass extraction.
  - Quarter-resolution blur pyramid consumption.
  - Temporal history accumulation and reprojection.

## 2. Functional Logic

- Behavior before:
  - Frosted effect executed as one full-screen compute pass with per-pixel neighborhood blur loops.
- Behavior after:
  - Frosted effect executes a staged postfx path:
    1. `Downsample Half`: full-res lighting color -> half-res ping
    2. `Blur Half Horizontal`: half ping -> half pong
    3. `Blur Half Vertical`: half pong -> half ping
    4. `Frosted Composite`: full-res panel logic + half blurred color -> final frosted output
  - Composite keeps panel edge/rim/refraction/fresnel logic while reading preblurred texture.
  - `Blur Sigma` remains effective through sigma-based modulation of blurred contribution.
- Key runtime flow:
  - `RendererSystemFrostedGlass::Tick` updates dispatch sizes for all passes and registers all nodes every frame.
  - Composite pass is explicitly dependent on blur vertical pass, ensuring stable pass ordering.

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `Resources/Shaders/FrostedGlassPostfx.hlsl` (new):
    - `DownsampleMain`
    - `BlurHorizontalMain`
    - `BlurVerticalMain`
  - `Resources/Shaders/FrostedGlass.hlsl`:
    - now samples `BlurredColorTex` instead of inline full-resolution spatial blur loops.
- Core algorithm changes:
  - Converted expensive per-pixel 2D blur in composite to separable preblur in dedicated passes.
  - Composite now performs shape/depth/refraction/fresnel weighting over prefiltered signal.
- Parameter/model changes:
  - No CPU-side panel schema change.
  - `FrostedGlassGlobalBuffer.blur_radius` now directly controls blur pass kernel radius.

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlassPostfx.hlsl`

## 5. Validation and Evidence

- Functional checks:
  - `RendererDemo` verify build succeeded after multipass node and shader integration.
- Visual checks:
  - Not executed in this iteration log (build-only validation scope).
- Performance checks:
  - Not executed in this iteration.
- Evidence files/links:
  - MSBuild summary:
    - `build_logs/rendererdemo_20260223_210529.msbuild.log`
  - Standard outputs:
    - `build_logs/rendererdemo_20260223_210529.stdout.log`
    - `build_logs/rendererdemo_20260223_210529.stderr.log`
  - Binlog:
    - `build_logs/rendererdemo_20260223_210529.binlog`

## 6. Acceptance Result

- Status: PASS (B1.1 scaffold scope)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 0
  - Error count: 0
  - B1 overall remains in progress; this iteration establishes multipass runtime foundation.

## 7. Risks and Follow-up

- Known risks:
  - Quarter-resolution resources are prepared but not yet consumed in frosted blur pyramid path.
  - Runtime visual/perf behavior still needs scene-based verification and timing capture.
- Next tasks:
  - B1.2: add quarter-level pyramid usage and tuned reconstruction/composite strategy.
  - B1.3: split explicit mask/parameter stage and validate quality/perf against v1 baseline.
