# Feature Note - A5.2 Environment Texture Input

Companion references:

- Chinese companion:
  - `docs/FeatureNotes/20260316_A5_EnvironmentTextureInput_ZH.md`

- Date: 2026-03-16
- Work Item ID: A5.2
- Title: Add file-backed environment texture input to the lighting pass
- Owner: AI coding session
- Companion Note:
  - `docs/FeatureNotes/20260316_A5_EnvironmentTextureInput_ZH.md`
- Related Plan:
  - `docs/FeatureNotes/20260315_IndirectLightingOptionsSurvey.md`
  - `docs/FeatureNotes/20260316_A5_EnvironmentLightingFoundation.md`
- Path Convention:
  - Repo-relative paths are rooted at the current directory (`C:\glTFRenderer`).
  - Code files should normally start with `glTFRenderer/`; root utility scripts use `scripts/`.

## 1. Scope

- In scope:
  - Add a file-backed environment texture handle to `RendererSystemLighting`.
  - Bind a regular `TextureHandle` into the lighting compute pass through `AddTexture(...)`.
  - Sample an equirectangular environment texture in `RendererModuleLighting.hlsl`.
  - Keep the procedural sky/ground environment as a fallback.
  - Add debug controls for environment-texture intensity and texture enable state.
- Out of scope:
  - Cubemap resource type support.
  - Mip-chain prefilter generation.
  - BRDF LUT integration.
  - Runtime texture hot-reload or arbitrary path editing UI.

## 2. Functional Logic

- Behavior before:
  - Environment indirect light was driven only by procedural sky/ground colors and scalar controls.
  - No real texture resource participated in scene indirect lighting.
- Behavior after:
  - `RendererSystemLighting` loads a default environment texture from file:
    - `glTFRenderer/glTFResources/Models/Plane/dawn_4k.png`
  - If the file is unavailable, lighting falls back to a 1x1 texture handle and the shader keeps using the procedural gradient path.
  - The lighting shader can now switch between:
    - procedural gradient environment,
    - file-backed environment texture sampling.
- Key runtime flow:
  - Lighting init creates `LightingGlobalBuffer` and ensures an environment texture handle exists.
  - `BuildLightingPassSetupInfo(...)` binds `environmentTex` as a regular texture resource.
  - The shader converts world-space directions to lat-long UVs and samples the environment texture.

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- Core algorithm changes:
  - Add `EnsureEnvironmentTexture(...)` to create either:
    - a file-backed environment texture, or
    - a fallback 1x1 texture.
  - Extend `LightingGlobalParams` with `environment_texture_params`.
  - Sample environment radiance from an equirectangular texture using:
    - `u = atan2(z, x) / (2*pi) + 0.5`
    - `v = acos(y) / pi`
  - Preserve the procedural environment model as a stable fallback path.
- Parameter/model changes:
  - New texture parameter block:
    - `environment_texture_params.x`: environment texture intensity
    - `environment_texture_params.z`: use-texture flag

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- `docs/FeatureNotes/20260316_A5_EnvironmentTextureInput.md`
- `docs/FeatureNotes/20260316_A5_EnvironmentTextureInput_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 5. Validation and Evidence

- Functional checks:
  - Build completed successfully for `RendererDemo`.
  - New regular-texture binding path compiled through the lighting compute pass.
- Visual checks:
  - Not executed in this iteration.
- Performance checks:
  - Not executed in this iteration.
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/build_verify_20260316_162217.stdout.log`
    - `build_logs/build_verify_20260316_162217.stderr.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260316_162218.msbuild.log`
    - `build_logs/rendererdemo_20260316_162218.stdout.log`
    - `build_logs/rendererdemo_20260316_162218.stderr.log`
    - `build_logs/rendererdemo_20260316_162218.binlog`

## 6. Acceptance Result

- Status: PASS (build validation scope)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 0
  - Error count: 0
  - This stage upgrades environment lighting from parameter-only to resource-backed sampling, but it is still pre-IBL-final.

## 7. Risks and Follow-up

- Known risks:
  - The sampled environment texture is still used without irradiance convolution or specular prefiltering.
  - Roughness response remains approximate because this stage does not yet use a BRDF LUT or mip-filtered specular environment.
  - The default environment source is fixed in code for now.
- Next tasks:
  - A5.3: add irradiance diffuse and roughness-aware specular prefilter resources.
  - A5.4: add BRDF integration LUT support.
  - A5.5: add environment-source selection or runtime configuration once texture lifetime management is formalized.
