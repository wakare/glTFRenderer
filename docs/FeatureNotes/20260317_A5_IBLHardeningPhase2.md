# Feature Note - A5-H2 IBL Hardening Phase 2

Companion references:

- Chinese companion:
  - `docs/FeatureNotes/20260317_A5_IBLHardeningPhase2_ZH.md`

- Date: 2026-03-17
- Work Item ID: A5-H2
- Title: Add explicit mip-chain texture uploads and collapse specular prefilter storage into a single mip-backed resource
- Owner: AI coding session
- Companion Note:
  - `docs/FeatureNotes/20260317_A5_IBLHardeningPhase2_ZH.md`
- Related Plan:
  - `docs/FeatureNotes/20260317_A5_IBLHardeningPhase1.md`
  - `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder.md`
- Path Convention:
  - Repo-relative paths are rooted at the current directory (`C:\glTFRenderer`).
  - Code files should normally start with `glTFRenderer/`; root utility scripts use `scripts/`.

## 1. Scope

- In scope:
  - Extend runtime texture creation so one texture can be initialized from explicit CPU-provided mip payloads.
  - Replace the four independent specular prefilter textures with one mip-backed texture.
  - Keep the current A5 visual baseline stable on both DX and Vulkan.
- Out of scope:
  - Remove the remaining environment bring-up controls in this iteration.
  - Change the environment source path from the current LDR pipeline to HDR in this iteration.
  - Move environment preprocessing from CPU to offline or GPU in this iteration.

## 2. Why This Iteration Expanded Framework Code

- The maintained-path requirement was no longer just "sample fewer textures in one shader."
- A clean A5-H2 implementation needed one renderer-level texture API that can upload explicit mip chains, because the prefilter ladder is authored on CPU and should land as one GPU resource instead of four feature-local handles.
- This iteration therefore widened framework code on purpose:
  - `RendererInterface::TextureDesc` now exposes explicit mip payloads.
  - `ResourceOperator::CreateTexture(...)` can allocate and upload those mip payloads.
  - `RHIUtils::UploadTextureData(...)` now correctly handles row-padding differences between tight CPU images and RHI copy footprints.

## 3. Functional Logic

- Behavior before:
  - `EnvironmentLightingResources` created four independent prefilter textures.
  - The lighting shader bound `environmentPrefilterTex0..3`.
  - Specular IBL sampled every prefilter texture every pixel, then manually selected and lerped.
- Behavior after:
  - `EnvironmentLightingResources` builds one prefilter mip chain with these authored levels:
    - `128x64` for roughness `0.25`
    - `64x32` for roughness `0.50`
    - `32x16` for roughness `0.75`
    - `16x8` for roughness `1.00`
  - The lighting pass now binds a single `environmentPrefilterTex`.
  - The shader keeps the existing low-roughness behavior:
    - roughness in `[0.0, 0.25]` still blends from sharp `environmentTex` to mip `0`
  - The shader simplifies the remaining path:
    - roughness in `(0.25, 1.0]` maps to mip LOD and uses `SampleLevel(...)` on the single prefilter texture
  - The resource binding surface is smaller and the per-pixel specular IBL path is cheaper without changing the approved visual baseline.

## 4. Framework Impact

- `RendererInterface::TextureDesc` gained `mip_levels`, which makes explicit mip uploads a first-class renderer capability instead of feature-local special handling.
- `ResourceOperator::CreateTexture(...)` now supports three valid runtime cases:
  - single-mip upload through `data`
  - explicit mip-chain upload through `mip_levels`
  - empty texture allocation with no initial data
- `RHIUtils::UploadTextureData(...)` now copies only the tight source row width into each padded destination row.
  - This fixes the previous assumption that source row pitch always matched the RHI copy pitch.
  - The fix is important for smaller mip levels where DX12 copy footprints can be wider than tight image rows.

## 5. Files Changed

- `glTFRenderer/RendererCore/Public/Renderer.h`
- `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- `glTFRenderer/RHICore/Private/RHIUtils.cpp`
- `glTFRenderer/RendererDemo/RendererSystem/EnvironmentLightingResources.h`
- `glTFRenderer/RendererDemo/RendererSystem/EnvironmentLightingResources.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- `docs/FeatureNotes/20260317_A5_IBLHardeningPhase2.md`
- `docs/FeatureNotes/20260317_A5_IBLHardeningPhase2_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 6. Validation and Evidence

- Functional checks:
  - `RendererDemo` build completed successfully after the framework and shader changes landed.
  - Approved-baseline compare passed on both DX and Vulkan after the single-texture prefilter path replaced the multi-texture ladder.
- Visual checks:
  - Approved-baseline compare covered:
    - `env_texture_ibl_reference`
    - `ibl_off_reference`
    - `procedural_ibl_reference`
  - No visual mismatch remained at acceptance time.
- Performance checks:
  - This iteration reduced bound prefilter SRVs from `4` to `1`.
  - The specular IBL shader path no longer samples all prefilter levels every pixel.
  - No new perf gate was added in this iteration; the existing A5 compare path remains visual-first.
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/build_a5h2_20260317_160723.stdout.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260317_160724.msbuild.log`
    - `build_logs/rendererdemo_20260317_160724.stdout.log`
    - `build_logs/rendererdemo_20260317_160724.stderr.log`
    - `build_logs/rendererdemo_20260317_160724.binlog`
  - Compare wrapper summary:
    - `build_logs/compare_a5h2_baseline_20260317_162401.stdout.log`
  - Compare outputs:
    - `.tmp/regression_compare_a5_model_viewer_lighting/a5cb_20260317_162402/summary.md`

## 7. Acceptance Result

- Status: PASS
- Acceptance notes:
  - A5-H2 is now implemented as a framework-backed mip-chain path instead of a feature-local multi-SRV workaround.
  - The runtime binding surface is smaller and the specular IBL sampling path is cheaper.
  - The approved A5 baseline still passes on both DX and Vulkan.

## 8. Remaining Risks and Next Tasks

- Known risks after A5-H2:
  - Temporary environment bring-up controls and anonymous parameter packing still remain.
  - The environment source and derived textures are still limited to the current LDR path.
  - Environment preprocessing is still CPU-authored and synchronous during initialization.
- Next tasks:
  - A5-H3: isolate or remove temporary environment bring-up controls from runtime-facing logic.
  - A5-H4: widen the source and derived-resource path toward HDR input and a more scalable preprocessing route.
