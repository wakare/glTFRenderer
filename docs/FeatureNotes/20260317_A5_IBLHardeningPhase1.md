# Feature Note - A5-H1 IBL Hardening Phase 1

Companion references:

- Chinese companion:
  - `docs/FeatureNotes/20260317_A5_IBLHardeningPhase1_ZH.md`

- Date: 2026-03-17
- Work Item ID: A5-H1
- Title: Record the IBL hardening review and extract environment-lighting resources from `RendererSystemLighting`
- Owner: AI coding session
- Companion Note:
  - `docs/FeatureNotes/20260317_A5_IBLHardeningPhase1_ZH.md`
- Related Plan:
  - `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder.md`
  - `docs/FeatureNotes/20260316_A5_ModelViewerRegressionValidation.md`
- Path Convention:
  - Repo-relative paths are rooted at the current directory (`C:\glTFRenderer`).
  - Code files should normally start with `glTFRenderer/`; root utility scripts use `scripts/`.

## 1. Scope

- In scope:
  - Record the current IBL review findings and convert them into an implementation order.
  - Complete the first hardening task: extract environment-lighting resource lifetime and binding logic from `RendererSystemLighting`.
  - Keep the existing A5 runtime behavior and validation entrypoints intact.
- Out of scope:
  - Replace the roughness ladder with a true mip chain or texture array in this iteration.
  - Remove the remaining temporary debug controls in this iteration.
  - Change environment source format from LDR lat-long to HDR or cubemap storage in this iteration.

## 2. Review Summary

- Main findings from the review:
  - `RendererSystemLighting` owned source loading, irradiance generation, prefilter generation, BRDF LUT generation, and pass binding in one class.
  - Source image decoding happened twice during initialization.
  - Specular IBL still uses separate prefilter textures and a shader path that samples all levels.
  - The runtime still keeps some bring-up behavior, including the hard-coded default source path and the procedural-versus-texture debug switch.
  - The current environment path is still LDR-only because it only accepts 32-bit WIC formats and uploads derived resources as `RGBA8_UNORM`.

## 3. Prioritized Follow-up Order

- A5-H1:
  - extract environment-lighting resources into a reusable resource owner.
- A5-H2:
  - replace the multi-SRV prefilter ladder path with a mip-backed resource shape that supports cheaper roughness lookup.
  - if the current `RendererInterface` or lower renderer/RHI path cannot express that cleanly, widen the framework surface instead of preserving the workaround.
- A5-H3:
  - remove or isolate temporary bring-up controls and make environment-source configuration explicit.
- A5-H4:
  - widen the source/derived-resource path toward HDR-capable input and decide whether preprocessing stays on CPU, moves offline, or moves to GPU.

## 4. Functional Logic

- Behavior before:
  - `RendererSystemLighting` directly owned:
    - environment source loading,
    - irradiance generation,
    - prefilter generation,
    - BRDF LUT generation,
    - environment texture bindings for the lighting pass.
  - The environment source image was decoded once for the source texture and again for the derived textures.
- Behavior after:
  - A dedicated `EnvironmentLightingResources` now owns:
    - source texture lifetime,
    - irradiance and prefilter texture creation,
    - BRDF LUT creation,
    - environment texture binding setup for the lighting pass.
  - `RendererSystemLighting` now keeps:
    - lighting global parameters,
    - direct-light and shadow orchestration,
    - lighting pass creation,
    - lighting debug UI.
  - Environment source decoding now happens once per initialization path.

## 5. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/EnvironmentLightingResources.h`
- `glTFRenderer/RendererDemo/RendererSystem/EnvironmentLightingResources.cpp`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/RendererDemo.vcxproj`
- `glTFRenderer/RendererDemo/RendererDemo.vcxproj.filters`
- `docs/FeatureNotes/20260317_A5_IBLHardeningPhase1.md`
- `docs/FeatureNotes/20260317_A5_IBLHardeningPhase1_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 6. Validation and Evidence

- Functional checks:
  - `RendererDemo` build completed successfully after the resource extraction landed.
  - A5 approved-baseline compare passed on both DX and Vulkan after the refactor fix-up.
- Visual checks:
  - Approved-baseline compare covered:
    - `env_texture_ibl_reference`
    - `ibl_off_reference`
    - `procedural_ibl_reference`
  - A transient regression was caught during the first compare run:
    - `procedural_ibl_reference` failed because the extracted BRDF-LUT path used a different `GeometrySchlickGGXIBL` approximation than the existing baseline.
    - The implementation was corrected to match the original baseline before final acceptance.
- Performance checks:
  - No new performance gate was added in this iteration.
  - The compare path remained visual-first, consistent with the current A5.6 gate policy.
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/build_a5h1_20260317_r3.stdout.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260317_154249.msbuild.log`
    - `build_logs/rendererdemo_20260317_154249.stdout.log`
    - `build_logs/rendererdemo_20260317_154249.stderr.log`
    - `build_logs/rendererdemo_20260317_154249.binlog`
  - Compare wrapper summary:
    - `build_logs/compare_a5h1_baseline_20260317_r2.stdout.log`
  - Compare outputs:
    - `.tmp/regression_compare_a5_model_viewer_lighting/a5cb_20260317_154555/summary.json`
    - `.tmp/regression_compare_a5_model_viewer_lighting/a5cb_20260317_154555/summary.md`

## 7. Acceptance Result

- Status: PASS
- Acceptance notes:
  - The review-derived hardening order is now recorded in-project.
  - The first hardening task, `EnvironmentLightingResources`, is integrated and build-clean.
  - The final approved-baseline compare passed on both DX and Vulkan.

## 8. Remaining Risks and Next Tasks

- Known risks after A5-H1:
  - Specular prefilter lookup still binds and samples multiple independent textures.
  - Temporary debug controls and anonymous environment parameter packing still remain.
  - The environment source and derived textures are still limited to the current LDR path.
- Next tasks:
  - A5-H2: collapse specular prefilter storage toward a mip-backed single-resource path and reduce per-pixel cost; extend renderer/RHI-facing interfaces if that is what the maintained path needs.
  - A5-H3: isolate or remove temporary environment bring-up controls from runtime-facing logic.
  - A5-H4: evaluate HDR-capable source input and a more scalable preprocessing path.
  - Principle update: future refactors should let concrete rendering needs drive `RendererCore` / `RHICore` interface growth when required, instead of treating framework avoidance as a success condition.
