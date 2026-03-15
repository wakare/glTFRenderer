# B9 Regression Workflow Runbook (Fixed Viewpoint Screenshot + Compare)

Companion references:

- Chinese companion:
  - `docs/FeatureNotes/20260228_B9_RegressionWorkflow_Runbook_ZH.md`

- Date: 2026-02-28
- Scope: `RendererDemo` frosted-glass visual/perf iteration
- Purpose: provide a deterministic, repeatable loop for baseline capture, current capture, and diff/perf compare
- Path base: repo-root-relative paths below assume the current directory is `C:\glTFRenderer`

## 1. Prerequisites

- Built executable exists:
  - `glTFRenderer/x64/Debug/RendererDemo.exe`
- Regression case json exists (exported from Demo UI):
  - default: `glTFRenderer/x64/Debug/build_logs/regression_case_exports/*.json`
- Capture/compare scripts exist:
  - `scripts/Validate-RendererRegression.ps1`
  - `scripts/Capture-RendererRegression.ps1`
  - `scripts/Compare-RendererRegression.ps1`
  - `scripts/RegressionCompareProfile.default.json`

## 2. Critical Runtime Path Rule

Always launch `RendererDemo.exe` with **working directory set to exe directory**:

- repo-root-relative: `glTFRenderer/x64/Debug`

Reason:

- runtime shader includes are loaded by relative path `Resources/Shaders/...`
- scene glTF path is relative (`glTFResources/...`)
- only exe directory satisfies both lookups simultaneously in current setup

## 3. Export Case JSON (UI)

In Demo panel:

- set target camera/panel state
- click `Export Current Regression Case JSON`
- record output path shown in UI (`Last Exported Case JSON`)

## 4. One-Shot Validation

Use the unified validation entrypoint when the goal is to validate the whole regression flow without adding backend-specific manual steps:

```powershell
$repoRoot = (Get-Location).Path
$suite = Join-Path $repoRoot "glTFRenderer\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json"

powershell -ExecutionPolicy Bypass -File .\scripts\Validate-RendererRegression.ps1 `
  -Suite $suite `
  -Backends dx,vk `
  -OutputBase ".tmp\regression_validate" `
  -RenderDocCapture `
  -RenderDocRequired
```

Expected top-level outputs:

- `STATUS=ValidationPassed` when build, capture, and compare all pass for every requested backend
- `SUMMARY_JSON=...` and `SUMMARY_MD=...` under the validation output root
- `BACKEND_DX_*` / `BACKEND_VK_*` lines that point to the per-backend baseline/current run directories and compare summaries

Notes:

- The validation script builds once, then runs the same capture/compare flow for every backend in `-Backends`.
- Use `-SkipBuild` when you already trust the current binary and only want to re-run capture/compare.
- RenderDoc flags are applied uniformly to every backend; no extra RHI-specific validation step is required.

## 5. Capture Baseline/Current (CLI)

Recommended pattern (quiet logging):

```powershell
$repoRoot = (Get-Location).Path
$suite = Join-Path $repoRoot "glTFRenderer\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json"

powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite $suite `
  -Backend dx `
  -OutputBase ".tmp\regression_opt_baseline"

powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite $suite `
  -Backend dx `
  -OutputBase ".tmp\regression_opt_current"
```

The capture script prints summary lines such as `STATUS=...`, `RUN_DIR=...`, `SUMMARY=...`, `STDOUT=...`, and `STDERR=...`. Use `RUN_DIR` for compare input and inspect the redirected logs only when the status is not `RunSucceeded`.
It also uses compact internal invocation directories such as `rc_dx_<timestamp>` to reduce Windows path-length failures when the chosen output base is already deep.

Manual fallback:

```powershell
$repoRoot = (Get-Location).Path
$exeWorkDir = Join-Path $repoRoot "glTFRenderer\x64\Debug"
$exe = ".\RendererDemo.exe"
$suite = Join-Path $repoRoot "glTFRenderer\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json"
$baselineOut = Join-Path $repoRoot ".tmp\regression_opt_baseline_manual"

Push-Location $exeWorkDir
& $exe DemoAppModelViewerFrostedGlass -dx -disable-debug-ui --no-assert-dialog -regression "-regression-suite=$suite" "-regression-output=$baselineOut"
Pop-Location
```

Optional RenderDoc flags:

- Add `-renderdoc-ui` to preload the RenderDoc runtime before device initialization without forcing regression capture. This is the recommended startup flag when you want to use the shared DemoBase RenderDoc UI manually.
- Add `-renderdoc-capture` to request `.rdc` artifacts for cases that enable `capture_renderdoc`, or to force RenderDoc capture for all cases in the run.
- Add `-renderdoc-required` when the run should fail immediately if the RenderDoc API is unavailable.
- `Capture-RendererRegression.ps1` forwards both switches directly to `RendererDemo.exe`.
- DX12 first checks the normal DLL search path, then falls back to the registered RenderDoc installation next to `renderdoc.json`.
- Vulkan does not use manual `LoadLibrary` injection. When a registered RenderDoc 1.43+ installation is detected, the app enables RenderDoc's Vulkan implicit layer before device initialization. Older or incomplete installations still require launching from RenderDoc UI or an equivalent injected environment.

Expected artifacts for each run:

- <run_dir>/suite_result.json
- <run_dir>/cases/001_<case_id>.png
- <run_dir>/cases/001_<case_id>.pass.csv
- <run_dir>/cases/001_<case_id>.perf.json
- optional: <run_dir>/cases/001_<case_id>_frameXXXX.rdc

Notes:

- RenderDoc strips the requested extension and appends a frame-based suffix to the final `.rdc` file name.
- Suite `global` now supports:
  - `default_capture_renderdoc`
  - `default_renderdoc_capture_frame_offset`
  - `default_keep_renderdoc_on_success`
- Per-case `capture` blocks now support:
  - `capture_renderdoc`
  - `renderdoc_capture_frame_offset`
  - `keep_renderdoc_on_success`
- `renderdoc_capture_frame_offset` delays the RenderDoc/screenshot/pass-csv finalization frame by the requested number of frames, so those artifacts stay aligned to the same later frame.
- `keep_renderdoc_on_success=false` prunes successful `.rdc` files after summary export. In that mode `suite_result.json` still records capture success and frame index, but leaves `renderdoc_capture_path` empty and marks `renderdoc_capture_retained=false`.
- `suite_result.json` records the actual RenderDoc capture path, capture frame index, retention state, and capture error fields per case.
- Regression screenshots now prefer `PrintWindow(PW_RENDERFULLCONTENT | PW_CLIENTONLY)` and only fall back to `BitBlt`, which reduces desktop-window contamination when runs are launched from the normal Windows desktop.

Manual UI capture:

- Launch any debug-ui-enabled demo with `-renderdoc-ui` when you want the shared RenderDoc controls to be available without turning the whole run into a regression capture.
- Open `Runtime / Diagnostics > RenderDoc` in the Demo panel.
- `Capture Current Frame` arms a one-frame `.rdc` capture through the shared `DemoBase` path.
- `Auto Open Replay UI After Capture` opens the resulting `.rdc` in RenderDoc automatically after the frame finishes.
- `Open Last Capture In RenderDoc` reopens the most recent successful capture path recorded by the runtime.
- When `-renderdoc-ui` or other RenderDoc startup flags are active, runtime RHI recreation now re-runs the target-backend preload step before device creation, so DX12/Vulkan switching stays on the same opt-in path.
- The shared UI and regression automation use the same RenderDoc service. If a regression case already armed a capture, the manual button will report that another capture is pending instead of overlapping requests.

## 6. Compare Baseline vs Current

```powershell
$repoRoot = (Get-Location).Path
$baselineRun = Join-Path $repoRoot ".tmp\regression_opt_baseline\0_suite_XXXXXXXX_XXXXXX"
$currentRun = Join-Path $repoRoot ".tmp\regression_opt_current\0_suite_XXXXXXXX_XXXXXX"
$reportOut = Join-Path $repoRoot ".tmp\regression_opt_compare"
$profile = Join-Path $repoRoot "scripts\RegressionCompareProfile.default.json"

powershell -ExecutionPolicy Bypass -File .\scripts\Compare-RendererRegression.ps1 `
  -Baseline $baselineRun `
  -Current  $currentRun `
  -ReportOut $reportOut `
  -Profile $profile `
  > .tmp\compare.stdout.log 2> .tmp\compare.stderr.log
```

Expected report:

- summary JSON under .tmp/regression_opt_compare/
- summary Markdown under .tmp/regression_opt_compare/
- diff images under .tmp/regression_opt_compare/diff/

RenderDoc note:

- compare still uses the normal screenshot diff path
- perf checks are skipped automatically for cases where either side produced a successful RenderDoc capture, because `.rdc` capture perturbs timing enough to make perf thresholds unreliable
- `summary.json` keeps baseline/current RenderDoc metadata, including retained paths when `.rdc` artifacts are still present
- when a successful capture was intentionally pruned via `keep_renderdoc_on_success=false`, compare summaries mark the case as `rdc-pruned` instead of linking a missing file path

## 7. Build Verification (Quiet)

Use isolated verify build script:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Build-RendererDemo-Verify.ps1 > .tmp\build.stdout.log 2> .tmp\build.stderr.log
```

Then read only summary lines from wrapper stdout:

- `STATUS=...`
- `WARNINGS=...`
- `ERRORS=...`
- `TXT=...`
- `STD=...`
- `ERR=...`
- `BIN=...`

## 8. Issues Encountered and Fixes

### 7.1 Scene load crash (wrong CWD)

Symptom:

- exception in glTF loading path (`Sponza.gltf`) during startup
- run exits early with non-zero code

Root cause:

- process launched from repository root or another directory where `glTFResources/...` is not resolvable

Fix:

- run process with CWD = `glTFRenderer/x64/Debug`
- pass `-regression-suite` as absolute path

### 7.2 Shader load/compile failure (wrong CWD)

Symptom:

- exception around shader source load (`glTFRenderer/RendererDemo/Resources/Shaders/ModelRenderingShader.hlsl`)

Root cause:

- shader relative path resolved from wrong working directory

Fix:

- same as above: CWD must be exe directory

### 7.3 PDB lock build failure (`C1041`)

Symptom:

- intermittent build failure: cannot open `vc143.pdb`

Root cause:

- concurrent build host process contention

Fix:

- rerun quiet verify build (usually recovers)
- keep `/m:1 /nr:false` and isolated verify output policy

### 7.4 AIChat output flood risk

Symptom:

- terminal/chat lag or freeze when command prints large logs

Fix:

- always redirect stdout/stderr to files
- only report status, warning/error counts, key diagnostics, log paths

## 9. Minimal Daily Loop

1. Export/refresh case json from UI.
2. Capture baseline or reuse existing baseline run.
3. Make code/shader change.
4. Verify build via `Build-RendererDemo-Verify.ps1`.
5. Capture current run with same suite json and same CWD rule.
6. Run compare script and inspect `summary.json` + `diff/*.png`.
7. Decide keep/revert by visual + perf thresholds.
