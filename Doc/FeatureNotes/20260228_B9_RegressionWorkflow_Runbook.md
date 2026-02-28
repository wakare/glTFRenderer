# B9 Regression Workflow Runbook (Fixed Viewpoint Screenshot + Compare)

- Date: 2026-02-28
- Scope: `RendererDemo` frosted-glass visual/perf iteration
- Purpose: provide a deterministic, repeatable loop for baseline capture, current capture, and diff/perf compare

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

- `D:\Work\DevSpace\glTFRenderer\glTFRenderer\glTFRenderer\x64\Debug`

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
$exeWorkDir = "D:\Work\DevSpace\glTFRenderer\glTFRenderer\glTFRenderer\x64\Debug"
$exe = ".\RendererDemo.exe"
$suite = "D:\Work\DevSpace\glTFRenderer\glTFRenderer\glTFRenderer\x64\Debug\build_logs\regression_case_exports\0_20260228_185013.json"

$baselineOut = "D:\Work\DevSpace\glTFRenderer\glTFRenderer\.tmp\regression_opt_baseline"
$currentOut = "D:\Work\DevSpace\glTFRenderer\glTFRenderer\.tmp\regression_opt_current"

Push-Location $exeWorkDir
& $exe DemoAppModelViewer -dx -regression "-regression-suite=$suite" "-regression-output=$baselineOut" `
  > "D:\Work\DevSpace\glTFRenderer\glTFRenderer\.tmp\capture_baseline.stdout.log" `
  2> "D:\Work\DevSpace\glTFRenderer\glTFRenderer\.tmp\capture_baseline.stderr.log"

& $exe DemoAppModelViewer -dx -regression "-regression-suite=$suite" "-regression-output=$currentOut" `
  > "D:\Work\DevSpace\glTFRenderer\glTFRenderer\.tmp\capture_current.stdout.log" `
  2> "D:\Work\DevSpace\glTFRenderer\glTFRenderer\.tmp\capture_current.stderr.log"
Pop-Location
```

Expected artifacts for each run:

- `<run_dir>/suite_result.json`
- `<run_dir>/cases/001_<case_id>.png`
- `<run_dir>/cases/001_<case_id>.pass.csv`
- `<run_dir>/cases/001_<case_id>.perf.json`

## 5. Compare Baseline vs Current

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Compare-RendererRegression.ps1 `
  -Baseline "D:\Work\DevSpace\glTFRenderer\glTFRenderer\.tmp\regression_opt_baseline\0_suite_XXXXXXXX_XXXXXX" `
  -Current  "D:\Work\DevSpace\glTFRenderer\glTFRenderer\.tmp\regression_opt_current\0_suite_XXXXXXXX_XXXXXX" `
  -ReportOut "D:\Work\DevSpace\glTFRenderer\glTFRenderer\.tmp\regression_opt_compare" `
  -Profile "D:\Work\DevSpace\glTFRenderer\glTFRenderer\scripts\RegressionCompareProfile.default.json" `
  > .tmp\compare.stdout.log 2> .tmp\compare.stderr.log
```

Expected report:

- `.tmp/regression_opt_compare/summary.json`
- `.tmp/regression_opt_compare/summary.md`
- `.tmp/regression_opt_compare/diff/*_absdiff.png`

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

- exception around shader source load (`Resources/Shaders/ModelRenderingShader.hlsl`)

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
