# Feature Note - A5.5 Roughness Prefilter Ladder

Companion references:

- Chinese companion:
  - `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder_ZH.md`

- Date: 2026-03-16
- Work Item ID: A5.5
- Title: Replace single specular-prefilter approximation with a roughness-aware prefilter ladder
- Owner: AI coding session
- Companion Note:
  - `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder_ZH.md`
- Related Plan:
  - `docs/FeatureNotes/20260315_IndirectLightingOptionsSurvey.md`
  - `docs/FeatureNotes/20260316_A5_BrdfIntegrationLut.md`
- Path Convention:
  - Repo-relative paths are rooted at the current directory (`C:\glTFRenderer`).
  - Code files should normally start with `glTFRenderer/`; root utility scripts use `scripts/`.

## 1. Scope

- In scope:
  - Replace the previous single prefiltered-environment texture with a small roughness ladder.
  - Bind multiple prefilter textures into the lighting compute pass.
  - Add explicit roughness thresholds to the lighting global constant buffer.
  - Sample and interpolate prefilter levels in shader according to surface roughness.
- Out of scope:
  - True cubemap or mip-chain prefilter storage.
  - GPU-side prefilter generation.
  - Runtime editor controls for per-level resolution or roughness values.

## 2. Functional Logic

- Behavior before:
  - Specular indirect used one blurred prefilter texture plus a roughness-dependent blend from the sharp source environment.
  - Roughness response was improved by the BRDF LUT, but environment prefiltering still represented only one blur scale.
- Behavior after:
  - Lighting now generates four CPU-side prefilter textures at increasing roughness levels.
  - Shader chooses between:
    - the sharp source environment,
    - four prefiltered levels,
    - linear interpolation between adjacent levels.
  - Roughness thresholds are passed by `environment_prefilter_roughness`.
- Key runtime flow:
  - `EnsureDerivedEnvironmentTextures(...)` now creates:
    - one irradiance texture,
    - four specular prefilter textures.
  - `BuildLightingPassSetupInfo(...)` binds:
    - `environmentPrefilterTex0`
    - `environmentPrefilterTex1`
    - `environmentPrefilterTex2`
    - `environmentPrefilterTex3`
  - `SampleEnvironmentPrefilter(...)` selects and blends the correct level.

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- Core algorithm changes:
  - CPU prefilter ladder:
    - roughness levels: `0.25`, `0.50`, `0.75`, `1.00`,
    - resolutions: `128x64`, `64x32`, `32x16`, `16x8`,
    - generation method: same lat-long cone-sampling prefilter, repeated per level.
  - Constant-buffer update:
    - add `environment_prefilter_roughness` to `LightingGlobalBuffer`.
  - Shader integration:
    - if roughness is below the first level, blend from sharp environment to level 0,
    - otherwise blend between neighboring prefilter levels,
    - keep BRDF LUT sampling from A5.4 unchanged.
- Parameter/model changes:
  - `LightingGlobalParams` grows from `80` bytes to `96` bytes.
  - Internal prefilter storage changes from one texture handle to a small texture list.

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder.md`
- `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 5. Validation and Evidence

- Functional checks:
  - Build completed successfully for `RendererDemo`.
  - Multi-level prefilter resources compiled through both C++ and HLSL binding paths.
- Visual checks:
  - Not executed in this iteration.
- Performance checks:
  - Not executed in this iteration.
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/build_verify_20260316_165328.stdout.log`
    - `build_logs/build_verify_20260316_165328.stderr.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260316_165330.msbuild.log`
    - `build_logs/rendererdemo_20260316_165330.stdout.log`
    - `build_logs/rendererdemo_20260316_165330.stderr.log`
    - `build_logs/rendererdemo_20260316_165330.binlog`

## 6. Acceptance Result

- Status: PASS (build validation scope)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 0
  - Error count: 0
  - Specular indirect is no longer limited to a single representative prefilter blur level.

## 7. Risks and Follow-up

- Known risks:
  - Prefilter levels are still separate lat-long textures, not a true cubemap mip hierarchy.
  - CPU startup cost increases because each level is generated offline at init time.
  - Runtime visual validation is still pending.
- Next tasks:
  - A5.6: decide whether the current CPU-generated ladder is good enough as a bootstrap path or should be replaced by GPU preprocessing.
  - A5.7: add runtime capture validation for rough metals and grazing-angle highlights.
  - A5.8: evaluate whether environment source should move from lat-long textures to cubemap-compatible storage.
