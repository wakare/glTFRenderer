# Feature Note - A5.6 Model Viewer Regression Validation

Companion references:

- Chinese companion:
  - `docs/FeatureNotes/20260316_A5_ModelViewerRegressionValidation_ZH.md`

- Date: 2026-03-16
- Work Item ID: A5.6
- Title: Add model-viewer regression validation coverage for the A5 environment-lighting baseline
- Owner: AI coding session
- Companion Note:
  - `docs/FeatureNotes/20260316_A5_ModelViewerRegressionValidation_ZH.md`
- Related Plan:
  - `docs/FeatureNotes/20260228_B9_FixedViewpointScreenshotRegressionPlan.md`
  - `docs/FeatureNotes/20260228_B9_RegressionWorkflow_Runbook.md`
  - `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder.md`
- Path Convention:
  - Repo-relative paths are rooted at the current directory (`C:\glTFRenderer`).
  - Code files should normally start with `glTFRenderer/`; root utility scripts use `scripts/`.

## 1. Scope

- In scope:
  - Expose programmable read/write access to `RendererSystemLighting::LightingGlobalParams`.
  - Extend `DemoAppModelViewer` snapshots to capture and replay environment-lighting state.
  - Add a screenshot-first regression runner to plain `DemoAppModelViewer`.
  - Extend `Regression::ApplyLogicPack(...)` with `model_viewer_lighting`.
  - Add and execute a dedicated A5 smoke suite for environment-lighting validation.
- Out of scope:
  - RenderDoc automation parity for plain `DemoAppModelViewer`.
  - Baseline/current image diff comparison automation.

## 2. Functional Logic

- Behavior before:
  - Plain `DemoAppModelViewer` did not expose environment-lighting state through its non-render snapshot path.
  - The reusable regression workflow was effectively centered on the frosted-glass app path.
  - Environment-lighting validation for A5 was limited to build success and ad hoc runtime inspection.
- Behavior after:
  - `RendererSystemLighting` exposes normalized `GetGlobalParams()` and `SetGlobalParams(...)` helpers.
  - `DemoAppModelViewer` snapshots now include:
    - camera and viewport state,
    - directional-light state,
    - environment-lighting global params.
  - Plain `DemoAppModelViewer` now accepts:
    - `-regression`,
    - `-regression-suite=<path>`,
    - `-regression-output=<path>`.
  - A regression run now:
    - loads the suite JSON,
    - applies fixed camera and lighting logic-pack state,
    - warms up for a deterministic frame window,
    - exports screenshot, pass CSV, perf JSON, and `suite_result.json`,
    - closes the app automatically when the suite finishes.
  - Backward compatibility is preserved by `has_lighting_state`, so older snapshot JSON files without lighting payload still deserialize correctly.

## 3. Algorithm and Runtime Design

- Passes/modules touched:
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.h`
  - `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`
  - `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewerFrostedGlass.cpp`
  - `glTFRenderer/RendererDemo/Regression/RegressionLogicPack.h`
  - `glTFRenderer/RendererDemo/Regression/RegressionLogicPack.cpp`
  - `glTFRenderer/RendererDemo/Resources/RegressionSuites/model_viewer_lighting_a5_ibl_smoke.json`
- Core runtime changes:
  - Lighting state capture/replay:
    - `ModelViewerStateSnapshot` now carries `has_lighting_state` and `lighting_global_params`.
    - `ApplyModelViewerStateSnapshot(...)` replays environment-lighting state when present.
  - Regression execution path:
    - `ConfigureRegressionRunFromArguments(...)` parses the CLI contract.
    - `TickRegressionAutomation()` sequences warmup, capture, export, and shutdown.
    - regression output uses the existing screenshot-regression artifact shape:
      - `cases/*.png`
      - `cases/*.pass.csv`
      - `cases/*.perf.json`
      - `suite_result.json`
  - Screenshot capture:
    - plain model-viewer capture uses `PrintWindow(...)` with a `BitBlt` fallback and saves PNG via `glTFImageIOUtil`.
  - Logic pack support:
    - `model_viewer_lighting` configures:
      - `environment_enabled`
      - `use_environment_texture`
      - `ibl_diffuse_intensity`
      - `ibl_specular_intensity`
      - `ibl_horizon_exponent`
      - `environment_texture_intensity`
      - optional procedural colors
      - `directional_light_speed_radians`
  - App-path isolation:
    - `DemoAppModelViewerFrostedGlass` now rebuilds its runtime directly during init so the frosted-specific regression path is not double-configured through the base class.

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.h`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewerFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Regression/RegressionLogicPack.h`
- `glTFRenderer/RendererDemo/Regression/RegressionLogicPack.cpp`
- `glTFRenderer/RendererDemo/Resources/RegressionSuites/model_viewer_lighting_a5_ibl_smoke.json`
- `docs/FeatureNotes/20260316_A5_ModelViewerRegressionValidation.md`
- `docs/FeatureNotes/20260316_A5_ModelViewerRegressionValidation_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 5. Validation and Evidence

- Functional checks:
  - Build completed successfully for `RendererDemo`.
  - No new warnings were attributed to the A5.6 files; the remaining warnings are pre-existing numeric-conversion warnings in renderer modules.
  - DX and Vulkan runtime regression capture both completed successfully for all smoke-suite cases.
- Visual checks:
  - Screenshot export path produced three PNG captures on DX and three PNG captures on Vulkan for the A5 smoke suite.
  - Manual visual review and baseline/current diff comparison were not executed in this iteration.
- Performance checks:
  - Per-case perf JSON export completed successfully.
  - No cross-run perf comparison was executed in this iteration.
- Documentation checks:
  - Doc-reference validation still reports `75` historical issues.
  - The new A5.6 English/Chinese notes and README index entries did not appear in the validator error list.
- Evidence files/links:
  - Build wrapper summary:
    - `build_logs/build_verify_20260316_a56.stdout.log`
    - `build_logs/build_verify_20260316_a56.stderr.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260316_173254.msbuild.log`
    - `build_logs/rendererdemo_20260316_173254.stdout.log`
    - `build_logs/rendererdemo_20260316_173254.stderr.log`
    - `build_logs/rendererdemo_20260316_173254.binlog`
  - Regression capture wrapper summary:
    - `build_logs/capture_a56_dx.stdout.log`
    - `build_logs/capture_a56_dx.stderr.log`
    - `build_logs/capture_a56_vk_20260316_174242.stdout.log`
    - `build_logs/capture_a56_vk_20260316_174242.stderr.log`
  - Doc-reference validation logs:
    - `build_logs/validate_docs_20260316_174452.stdout.log`
    - `build_logs/validate_docs_20260316_174452.stderr.log`
  - Regression output:
    - `.tmp/regression_a56/rc_dx_20260316_173359/model_viewer_lighting_a5_ibl_smoke_20260316_173401/suite_result.json`
    - `.tmp/regression_a56/rc_dx_20260316_173359/model_viewer_lighting_a5_ibl_smoke_20260316_173401/cases/001_ibl_off_reference.png`
    - `.tmp/regression_a56/rc_dx_20260316_173359/model_viewer_lighting_a5_ibl_smoke_20260316_173401/cases/002_procedural_ibl_reference.png`
    - `.tmp/regression_a56/rc_dx_20260316_173359/model_viewer_lighting_a5_ibl_smoke_20260316_173401/cases/003_env_texture_ibl_reference.png`
    - `.tmp/regression_a56_vk/rc_vk_20260316_174242/model_viewer_lighting_a5_ibl_smoke_20260316_174245/suite_result.json`
    - `.tmp/regression_a56_vk/rc_vk_20260316_174242/model_viewer_lighting_a5_ibl_smoke_20260316_174245/cases/001_ibl_off_reference.png`
    - `.tmp/regression_a56_vk/rc_vk_20260316_174242/model_viewer_lighting_a5_ibl_smoke_20260316_174245/cases/002_procedural_ibl_reference.png`
    - `.tmp/regression_a56_vk/rc_vk_20260316_174242/model_viewer_lighting_a5_ibl_smoke_20260316_174245/cases/003_env_texture_ibl_reference.png`

### 5.1 Build

- Command:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Build-RendererDemo-Verify.ps1
```

- Result:
  - `STATUS=BuildSucceeded`
  - `WARNINGS=38`
  - `ERRORS=0`

### 5.2 DX Runtime Capture

- Command:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite glTFRenderer\RendererDemo\Resources\RegressionSuites\model_viewer_lighting_a5_ibl_smoke.json `
  -Backend dx `
  -DemoApp DemoAppModelViewer `
  -OutputBase .tmp\regression_a56
```

- Result:
  - `STATUS=RunSucceeded`
  - `EXITCODE=0`
  - `DEMO_APP=DemoAppModelViewer`
  - `CASES=3`
  - `RESULTS=3`
  - `FAILED=0`
  - `SUITE_SUCCESS=True`
  - `render_device=DX12`
  - `present_mode=VSYNC`
  - `renderdoc_capture_available=False`

### 5.3 Vulkan Runtime Capture

- Command:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite glTFRenderer\RendererDemo\Resources\RegressionSuites\model_viewer_lighting_a5_ibl_smoke.json `
  -Backend vk `
  -DemoApp DemoAppModelViewer `
  -OutputBase .tmp\regression_a56_vk
```

- Result:
  - `STATUS=RunSucceeded`
  - `EXITCODE=0`
  - `DEMO_APP=DemoAppModelViewer`
  - `CASES=3`
  - `RESULTS=3`
  - `FAILED=0`
  - `SUITE_SUCCESS=True`
  - `render_device=Vulkan`
  - `present_mode=VSYNC`
  - `renderdoc_capture_available=False`

### 5.4 Doc Reference Validation

- Command:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Validate-DocReferences.ps1
```

- Result:
  - validator exit code: `1`
  - scanned markdown files: `87`
  - checked references: `1057`
  - reported issues: `75`
  - issue profile: unchanged historical missing files under older `build_logs`, `.tmp`, and machine-local RenderDoc paths
  - A5.6 note files and README index updates did not appear in the validator error list

## 6. Acceptance Result

- Status: PASS (implementation, build, DX runtime capture, and Vulkan runtime capture scope)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: `38` (pre-existing)
  - Error count: `0`
  - DX regression suite status: `3/3` cases passed
  - Vulkan regression suite status: `3/3` cases passed
  - A5 environment-lighting validation is no longer blocked on the frosted-glass app path.

## 7. Risks and Follow-up

- Known risks:
  - Plain `DemoAppModelViewer` regression is currently screenshot-first and does not yet mirror the frosted path's RenderDoc automation.
  - The smoke suite currently covers one fixed Sponza camera view, so material and angle coverage is still narrow.
- Next tasks:
  - Add baseline/current compare usage for the new model-viewer artifacts.
  - Expand the suite with rough metals, grazing highlights, and additional viewpoints if A5 regression coverage needs to become a long-lived gate.

## 8. Next-Phase Plan

1. Compare-path hookup
   - Make the current `model_viewer_lighting_a5_ibl_smoke` outputs consumable by the existing baseline/current compare workflow with no manual file reshaping.
   - Capture and store approved DX and Vulkan baselines from the current smoke suite.
   - Acceptance target:
     - one compare invocation can report zero unexpected diffs for the current three cases on both backends.
2. Coverage expansion
   - Add targeted validation views for:
     - rough metal response,
     - glossy dielectric at grazing angles,
     - environment-texture-heavy reflections.
   - Acceptance target:
     - the suite can exercise the BRDF LUT and roughness-prefilter ladder with visibly distinct highlight behavior.
3. Deferred items
   - Keep RenderDoc automation parity as a later follow-up, not the next blocking step.
