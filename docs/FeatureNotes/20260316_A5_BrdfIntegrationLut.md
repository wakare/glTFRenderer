# Feature Note - A5.4 BRDF Integration LUT

Companion references:

- Chinese companion:
  - `docs/FeatureNotes/20260316_A5_BrdfIntegrationLut_ZH.md`

- Date: 2026-03-16
- Work Item ID: A5.4
- Title: Add CPU-generated BRDF integration LUT for split-sum specular IBL
- Owner: AI coding session
- Companion Note:
  - `docs/FeatureNotes/20260316_A5_BrdfIntegrationLut_ZH.md`
- Related Plan:
  - `docs/FeatureNotes/20260315_IndirectLightingOptionsSurvey.md`
  - `docs/FeatureNotes/20260316_A5_PreintegratedEnvironmentApprox.md`
- Path Convention:
  - Repo-relative paths are rooted at the current directory (`C:\glTFRenderer`).
  - Code files should normally start with `glTFRenderer/`; root utility scripts use `scripts/`.

## 1. Scope

- In scope:
  - Generate a 2D BRDF integration LUT on CPU during lighting-system initialization.
  - Bind the LUT into the lighting compute pass.
  - Replace the specular-indirect heuristic factors with a split-sum LUT term.
  - Expose the representative prefilter roughness to shader code through existing lighting globals.
- Out of scope:
  - Multi-mip or cubemap prefilter resource generation.
  - GPU-side BRDF LUT baking.
  - Runtime environment-source switching UI.

## 2. Functional Logic

- Behavior before:
  - Specular indirect blended sharp and blurred environment radiance.
  - Fresnel, grazing, and roughness response still used hand-tuned heuristic multipliers.
- Behavior after:
  - Lighting now creates an additional `environmentBrdfLutTex` resource.
  - Specular indirect samples the LUT by `(NdotV, roughness)`.
  - Shader output uses:
    - prefiltered environment radiance,
    - `F0 * A + B` from the LUT,
    - existing specular intensity control.
  - The current single-prefilter texture remains in place, but its representative roughness is now passed explicitly through `environment_texture_params.y`.
- Key runtime flow:
  - `RendererSystemLighting::EnsureEnvironmentBrdfLut(...)` creates the LUT texture once.
  - `BuildLightingPassSetupInfo(...)` binds `environmentBrdfLutTex`.
  - `GetEnvironmentSpecularLighting(...)` samples the LUT and applies the split-sum term.

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- Core algorithm changes:
  - BRDF LUT generation:
    - output: `128 x 128` 2D texture stored as `RGBA16_FLOAT`,
    - method: CPU importance-sampled GGX integration over `(NdotV, roughness)`,
    - stored channels:
      - `R`: scale term `A`,
      - `G`: bias term `B`.
  - Shader integration:
    - LUT sampling uses `float2(NdotV, roughness)`,
    - specular BRDF term becomes `F0 * A + B`,
    - environment radiance still blends sharp source and fixed-prefilter texture by the encoded representative roughness.
- Parameter/model changes:
  - No new debug sliders were added in this step.
  - `environment_texture_params.y` now carries the representative roughness of the prefiltered environment texture.
  - New internal texture handle:
    - BRDF LUT.

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- `docs/FeatureNotes/20260316_A5_BrdfIntegrationLut.md`
- `docs/FeatureNotes/20260316_A5_BrdfIntegrationLut_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 5. Validation and Evidence

- Functional checks:
  - Build completed successfully for `RendererDemo`.
  - New LUT creation, binding, and shader usage compiled through both C++ and HLSL.
- Visual checks:
  - Not executed in this iteration.
- Performance checks:
  - Not executed in this iteration.
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/build_verify_20260316_164423.stdout.log`
    - `build_logs/build_verify_20260316_164423.stderr.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260316_164425.msbuild.log`
    - `build_logs/rendererdemo_20260316_164425.stdout.log`
    - `build_logs/rendererdemo_20260316_164425.stderr.log`
    - `build_logs/rendererdemo_20260316_164425.binlog`

## 6. Acceptance Result

- Status: PASS (build validation scope)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 0
  - Error count: 0
  - Specular indirect now uses a BRDF LUT instead of the previous heuristic visibility and grazing factors.

## 7. Risks and Follow-up

- Known risks:
  - The specular prefilter texture is still a single representative blur, not a full roughness mip ladder.
  - BRDF LUT generation remains CPU-side and adds startup work.
  - Runtime visual validation has not yet been executed.
- Next tasks:
  - A5.5: replace the single prefilter approximation with a roughness-aware mip or multi-level prefilter path.
  - A5.6: decide whether to keep CPU environment preprocessing as bootstrap only or migrate LUT and prefilter generation to GPU.
  - A5.7: add runtime capture validation for metal and grazing-angle response.
