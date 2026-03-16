# Feature Note - A5.1 Environment Lighting Foundation

Companion references:

- Chinese companion:
  - `docs/FeatureNotes/20260316_A5_EnvironmentLightingFoundation_ZH.md`

- Date: 2026-03-16
- Work Item ID: A5.1
- Title: Add environment-lighting foundation to the scene lighting pass
- Owner: AI coding session
- Companion Note:
  - `docs/FeatureNotes/20260316_A5_EnvironmentLightingFoundation_ZH.md`
- Related Plan:
  - `docs/FeatureNotes/20260315_IndirectLightingOptionsSurvey.md`
- Path Convention:
  - Repo-relative paths are rooted at the current directory (`C:\glTFRenderer`).
  - Code files should normally start with `glTFRenderer/`; root utility scripts use `scripts/`.

## 1. Scope

- In scope:
  - Replace the hard-coded ambient placeholder in `SceneLighting.hlsl` with structured environment-lighting terms.
  - Add a minimal environment-lighting parameter buffer to `RendererSystemLighting`.
  - Split indirect lighting into diffuse and specular environment contributions.
  - Apply SSAO only to diffuse indirect lighting.
  - Add debug UI controls for the environment-lighting foundation.
- Out of scope:
  - HDR sky or cubemap asset ingestion.
  - Irradiance convolution, prefiltered specular IBL, or BRDF LUT generation.
  - Reflection probes, probe volumes, or screen-space GI.
  - Runtime skybox rendering.

## 2. Functional Logic

- Behavior before:
  - `SceneLighting.hlsl` computed direct light, then added a small hard-coded ambient color multiplied by SSAO.
  - The ambient placeholder did not distinguish diffuse indirect from specular indirect.
- Behavior after:
  - `SceneLighting.hlsl` computes:
    - direct lighting,
    - diffuse environment lighting,
    - specular environment lighting.
  - SSAO modulates only the diffuse indirect term.
  - Environment lighting is controlled by runtime parameters owned by `RendererSystemLighting`.
- Key runtime flow:
  - `RendererSystemLighting` uploads environment parameters.
  - `SceneLighting.hlsl` reads those parameters through `LightingGlobalBuffer`.
  - The shader evaluates a hemispherical sky/ground environment approximation for both diffuse and specular indirect terms.

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
  - `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- Core algorithm changes:
  - Add a dedicated lighting constant buffer for environment-lighting controls.
  - Replace the placeholder ambient add with:
    - diffuse environment response from the surface normal,
    - specular environment response from the reflection direction.
  - Keep the environment model intentionally simple: hemispherical sky zenith / horizon / ground colors.
  - Use this step as an architectural bridge toward full IBL resources later.
- Parameter/model changes:
  - New lighting global params:
    - `sky_zenith_color`
    - `sky_horizon_color`
    - `ground_color`
    - `environment_control`
  - `environment_control` stores:
    - diffuse intensity,
    - specular intensity,
    - horizon falloff exponent,
    - enable flag.

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- `docs/FeatureNotes/20260316_A5_EnvironmentLightingFoundation.md`
- `docs/FeatureNotes/20260316_A5_EnvironmentLightingFoundation_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 5. Validation and Evidence

- Functional checks:
  - Build completed successfully for `RendererDemo`.
  - New `LightingGlobalBuffer` path compiled through `RendererSystemLighting` and `SceneLighting.hlsl`.
- Visual checks:
  - Not executed in this iteration.
- Performance checks:
  - Not executed in this iteration.
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/build_verify_20260316_160600.stdout.log`
    - `build_logs/build_verify_20260316_160600.stderr.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260316_160601.msbuild.log`
    - `build_logs/rendererdemo_20260316_160601.stdout.log`
    - `build_logs/rendererdemo_20260316_160601.stderr.log`
    - `build_logs/rendererdemo_20260316_160601.binlog`
  - Doc validation logs:
    - `build_logs/validate_docs_20260316_160844.stdout.log`
    - `build_logs/validate_docs_20260316_160844.stderr.log`

## 6. Acceptance Result

- Status: PASS (build validation scope)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 608
  - Error count: 0
  - `scripts/Validate-DocReferences.ps1` reported pre-existing missing archived log and environment-path references outside this feature scope.
  - This note records the foundation step, not the final full-IBL endpoint.

## 7. Risks and Follow-up

- Known risks:
  - The hemispherical environment model improves lighting semantics, but it is still not asset-driven IBL.
  - Specular indirect remains an approximation until prefiltered environment resources and BRDF LUT are available.
- Next tasks:
  - A5.2: add asset-driven environment source input.
  - A5.3: add irradiance diffuse and prefiltered specular IBL resources.
  - A5.4: decide whether SSGI or probe GI is the next dynamic-indirect stage after IBL resources land.
