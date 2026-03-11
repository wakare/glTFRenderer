# B9 Regression Workflow Runbook (Fixed Viewpoint Screenshot + Compare)

- Date: 2026-02-28
- Scope: `RendererDemo` frosted-glass visual/perf iteration
- Purpose: provide a deterministic, repeatable loop for baseline capture, current capture, and diff/perf compare
- Path base: repo-root-relative paths below assume the current directory is `C:\glTFRenderer`

## 1. Prerequisites

- Built executable exists:
  - `glTFRenderer/x64/Debug/RendererDemo.exe`
- Regression case json exists (exported from Demo UI):
  - default: `glTFRenderer/x64/Debug/build_logs/regression_case_exports/*.json`
- Compare script exists:
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

## 4. Capture Baseline/Current (CLI)

Recommended pattern (quiet logging):

```powershell
$repoRoot = (Get-Location).Path
$exeWorkDir = Join-Path $repoRoot "glTFRenderer\x64\Debug"
$exe = ".\RendererDemo.exe"
$suite = Join-Path $exeWorkDir "build_logs\regression_case_exports\0_20260228_185013.json"

$baselineOut = Join-Path $repoRoot ".tmp\regression_opt_baseline"
$currentOut = Join-Path $repoRoot ".tmp\regression_opt_current"
$baselineStdout = Join-Path $repoRoot ".tmp\capture_baseline.stdout.log"
$baselineStderr = Join-Path $repoRoot ".tmp\capture_baseline.stderr.log"
$currentStdout = Join-Path $repoRoot ".tmp\capture_current.stdout.log"
$currentStderr = Join-Path $repoRoot ".tmp\capture_current.stderr.log"

Push-Location $exeWorkDir
& $exe DemoAppModelViewer -dx -regression "-regression-suite=$suite" "-regression-output=$baselineOut" `
  > $baselineStdout `
  2> $baselineStderr

& $exe DemoAppModelViewer -dx -regression "-regression-suite=$suite" "-regression-output=$currentOut" `
  > $currentStdout `
  2> $currentStderr
Pop-Location
```

Expected artifacts for each run:

- <run_dir>/suite_result.json
- <run_dir>/cases/001_<case_id>.png
- <run_dir>/cases/001_<case_id>.pass.csv
- <run_dir>/cases/001_<case_id>.perf.json

## 5. Compare Baseline vs Current

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

## 6. Build Verification (Quiet)

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

## 7. Issues Encountered and Fixes

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

## 8. Minimal Daily Loop

1. Export/refresh case json from UI.
2. Capture baseline or reuse existing baseline run.
3. Make code/shader change.
4. Verify build via `Build-RendererDemo-Verify.ps1`.
5. Capture current run with same suite json and same CWD rule.
6. Run compare script and inspect `summary.json` + `diff/*.png`.
7. Decide keep/revert by visual + perf thresholds.
