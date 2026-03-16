# Feature Note - A5.3 Preintegrated Environment Approximation

Companion references:

- Chinese companion:
  - `docs/FeatureNotes/20260316_A5_PreintegratedEnvironmentApprox_ZH.md`

- Date: 2026-03-16
- Work Item ID: A5.3
- Title: Add CPU-generated irradiance and specular-prefilter environment textures
- Owner: AI coding session
- Companion Note:
  - `docs/FeatureNotes/20260316_A5_PreintegratedEnvironmentApprox_ZH.md`
- Related Plan:
  - `docs/FeatureNotes/20260315_IndirectLightingOptionsSurvey.md`
  - `docs/FeatureNotes/20260316_A5_EnvironmentTextureInput.md`
- Path Convention:
  - Repo-relative paths are rooted at the current directory (`C:\glTFRenderer`).
  - Code files should normally start with `glTFRenderer/`; root utility scripts use `scripts/`.

## 1. Scope

- In scope:
  - Load the default environment image on CPU.
  - Generate a low-frequency irradiance lat-long texture.
  - Generate a blurred specular-prefilter lat-long texture.
  - Bind both derived textures into the lighting compute pass.
  - Use derived textures in shader for diffuse indirect and roughness-aware specular indirect.
- Out of scope:
  - GPU-side convolution or prefilter passes.
  - Cubemap representation.
  - Mipmap-based roughness ladder.
  - BRDF integration LUT.

## 2. Functional Logic

- Behavior before:
  - Lighting could sample a file-backed environment texture directly.
  - Diffuse and specular indirect still relied on direct environment sampling or procedural fallback only.
- Behavior after:
  - Lighting now creates three environment resources when the source image is available:
    - source environment texture,
    - irradiance texture,
    - specular-prefilter texture.
  - Diffuse indirect uses the irradiance texture.
  - Specular indirect blends between sharp source radiance and prefiltered radiance by roughness.
  - If the source image is unavailable, all three paths fall back to 1x1 textures and the procedural environment can still be used.
- Key runtime flow:
  - `RendererSystemLighting::EnsureEnvironmentTexture(...)` loads the source image and creates the source texture.
  - `EnsureDerivedEnvironmentTextures(...)` generates derived textures on CPU.
  - `BuildLightingPassSetupInfo(...)` binds:
    - `environmentTex`
    - `environmentIrradianceTex`
    - `environmentPrefilterTex`

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- Core algorithm changes:
  - Source environment image is decoded to linear RGB on CPU.
  - Irradiance texture generation:
    - output: low-resolution lat-long texture,
    - method: cosine-weighted hemisphere sampling around the target normal.
  - Specular-prefilter generation:
    - output: blurred lat-long texture,
    - method: cone sampling around the target reflection direction with a fixed roughness representative blur.
  - Shader integration:
    - diffuse indirect samples irradiance texture,
    - specular indirect blends source radiance and prefiltered radiance using `roughness^2`.
- Parameter/model changes:
  - No new UI-visible scalar parameters were required beyond existing environment-texture controls.
  - New internal texture handles:
    - source environment,
    - irradiance,
    - prefilter.

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- `docs/FeatureNotes/20260316_A5_PreintegratedEnvironmentApprox.md`
- `docs/FeatureNotes/20260316_A5_PreintegratedEnvironmentApprox_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 5. Validation and Evidence

- Functional checks:
  - Build completed successfully for `RendererDemo`.
  - New derived-environment texture path compiled through both C++ and HLSL.
- Visual checks:
  - Not executed in this iteration.
- Performance checks:
  - Not executed in this iteration.
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/build_verify_20260316_163336.stdout.log`
    - `build_logs/build_verify_20260316_163336.stderr.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260316_163336.msbuild.log`
    - `build_logs/rendererdemo_20260316_163336.stdout.log`
    - `build_logs/rendererdemo_20260316_163336.stderr.log`
    - `build_logs/rendererdemo_20260316_163336.binlog`

## 6. Acceptance Result

- Status: PASS (build validation scope)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 0
  - Error count: 0
  - This stage adds approximate preintegrated environment resources without yet introducing the final split-sum BRDF stack.

## 7. Risks and Follow-up

- Known risks:
  - CPU-generated derived textures are an approximation and may be slow if the source image or sample counts grow substantially.
  - Specular prefilter currently uses one representative blur texture rather than a mip/roughness pyramid.
  - Without a BRDF LUT, metal and grazing response remain simplified.
- Next tasks:
  - A5.4: add BRDF integration LUT support.
  - A5.5: decide whether to keep CPU preintegration as bootstrap only or replace it with GPU-side environment preprocessing.
  - A5.6: add runtime capture/screenshot validation to tune the derived-texture quality and cost.
