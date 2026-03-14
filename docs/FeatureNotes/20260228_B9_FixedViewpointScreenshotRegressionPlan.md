# Feature Note - B9 Fixed Viewpoint Regression and Frame Telemetry Plan

Companion references:

- Chinese companion:
  - `docs/FeatureNotes/20260228_B9_FixedViewpointScreenshotRegressionPlan_ZH.md`

- Date: 2026-02-28
- Work Item ID: B9
- Title: Fixed-viewpoint screenshot regression and frame-level performance telemetry system
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
  - `docs/RendererDemo_FrostedGlass_FramePassReference.md`
  - `docs/FeatureNotes/20260227_B8_BlurSourceOptimizationPlan.md`
  - `docs/FeatureNotes/20260228_B9_RegressionWorkflow_Runbook.md`

## 1. Requirement Update

- New requirement:
  - Replace manual visual-only comparison with reproducible fixed-viewpoint screenshot regression.
  - Support iterative rendering optimization with objective diff metrics and artifacts.
  - Capture frame/per-pass performance data in the same workflow for optimization guidance.
  - Keep current demo workflow usable while adding command-line batch validation.
- Scope:
  - Demo-side deterministic capture pipeline (first slice)
  - Rendering framework capture/readback interface (phase 2 upgrade)
  - Rendering framework frame telemetry export interface (phase 2 upgrade)
  - Demo-side fixed viewpoint test case construction/loading
  - CLI capture/compare/report workflow (visual + performance)

## 2. Problem Statement

Current loop relies on manual camera placement and subjective visual judgement:

- difficult to keep camera/panel/light state identical across runs
- hard to quantify whether quality is better/worse
- no stable artifact bundle for PR/commit-level acceptance evidence
- performance optimization lacks per-case frame/per-pass evidence linkage

Target loop:

- one command captures predefined viewpoints and telemetry snapshots
- one command compares current outputs against baseline (visual + performance)
- report provides pass/fail + metrics + diff images + telemetry deltas

## 3. Proposed System Architecture

Three-layer architecture:

1. Framework layer (capture and telemetry primitives)
- unified screenshot request interface for a target render output
- asynchronous readback and deterministic file write
- metadata output (resolution, frame index, commit, GPU info, timestamp)
- frame telemetry snapshot export interface (frame time + pass timing + selected counters)

2. Application layer (test case suite)
- fixed viewpoint case descriptor (scene/camera/panel/global toggles)
- deterministic warmup frame policy
- batch capture runner in RendererDemo
- performance probe profile selection (which metrics are captured and gated)

3. Regression layer (CLI compare/report)
- baseline vs current image metric computation
- baseline vs current performance metric comparison
- threshold-based pass/fail (visual and performance)
- report package: summary json + markdown + diff heatmap + telemetry delta table

## 4. Data Model and Directory Layout

- Repo-root-relative path base for this document: current directory `C:\glTFRenderer`.

## 4.1 Suite descriptor (json)

Example fields:

- `suite_name`
- `global.disable_debug_ui`
- `global.freeze_directional_light`
- `global.disable_panel_input_state_machine`
- `global.disable_prepass_animation`
- `global.default_warmup_frames`
- `global.default_capture_frames`
- `cases[]`:
  - `id`
  - `camera` (`position`, `euler_angles` or `euler_radians`)
  - `warmup_frames` (optional override)
  - `capture_frames` (optional override)
  - `capture_screenshot` (optional override)
  - `capture_perf` (optional override)
  - `logic_pack` (current: `none|frosted_glass`)
  - `logic_args` (for `frosted_glass`: `blur_source_mode`, `full_fog_mode`, `reset_temporal_history`, runtime toggles)

## 4.2 Suggested folder convention

- suite definitions: glTFRenderer/RendererDemo/Resources/RegressionSuites/*.json
- run output root example: glTFRenderer/build_logs/regression/<suite>_<timestamp>/ directory
- per-run files: suite_result.json, cases/<index>_<case>.png, cases/<index>_<case>.pass.csv, cases/<index>_<case>.perf.json

## 5. CLI Workflow

## 5.1 Capture

Current prototype command:

```powershell
RendererDemo.exe DemoAppModelViewer -dx -regression -regression-suite=Resources/RegressionSuites/frosted_glass_b9_smoke.json
```
- Optional output root override:
  - `-regression-output=<path>`

Behavior:

- load suite
- apply each case state
- run warmup frames
- capture output image
- write per-case pass csv
- write per-case perf json (frame + frosted-group aggregate during capture window)
- write suite summary json
- auto close window when suite ends

## 5.2 Compare

Current prototype command:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Compare-RendererRegression.ps1 `
  -Baseline <baseline_run_dir> `
  -Current <current_run_dir> `
  -ReportOut <report_dir> `
  -Profile .\scripts\RegressionCompareProfile.default.json
```

Behavior:

- load baseline/current `suite_result.json`
- compare by case id
- visual metrics: `MAE` / `RMSE` / `PSNR`
- perf metrics: frame/frosted aggregate increase-percent thresholds
- generate:
  - summary JSON
  - summary Markdown
  - `diff/*_absdiff.png`
- non-zero exit on failure

Behavior:

- compute metrics per case (`MAE`, `RMSE`, `PSNR`, optional `SSIM`)
- apply global threshold + optional ROI threshold
- compare performance metrics (frame and optional pass-level deltas) with threshold profile
- output report and diff images
- return non-zero exit code on failure

## 6. Priority and Dependency Plan

Status legend: `Planned`, `In Progress`, `Blocked`, `Accepted`

| Sub-item | Scope | Priority | Depends On | Status | Output |
|---|---|---|---|---|---|
| B9.1 | Framework screenshot API + readback queue | P0 | - | In Progress | current fallback uses window client capture; framework readback pending |
| B9.2 | Framework frame/per-pass telemetry export API | P0 | - | In Progress | current runner emits per-case pass csv + perf json from frame stats |
| B9.3 | Demo fixed-case suite loader and state applier | P0 | B9.1 | Accepted | json suite load + camera pose + logic-pack apply |
| B9.4 | Deterministic warmup and frame gate | P0 | B9.1, B9.3 | Accepted | per-case warmup/capture gate + temporal reset |
| B9.5 | Batch capture command-line entry | P0 | B9.2, B9.3, B9.4 | Accepted | `-regression -regression-suite=...` auto-run and auto-exit |
| B9.6 | Visual compare engine + thresholds | P0 | B9.5 | In Progress | script-based MAE/RMSE/PSNR compare + pass/fail |
| B9.7 | Performance compare engine + thresholds | P0 | B9.2, B9.5 | In Progress | script-based perf delta gate + pass/fail |
| B9.8 | Diff/report generation | P1 | B9.6, B9.7 | In Progress | summary JSON/Markdown + absdiff output |
| B9.9 | CI integration job (capture/compare) | P1 | B9.6, B9.7, B9.8 | Planned | automated visual/perf regression gate |
| B9.10 | Baseline update workflow and policy | P1 | B9.6, B9.7 | Planned | controlled visual/perf baseline refresh |

## 7. Execution Order (Recommended)

Phase 1 (minimum usable visual+perf capture):

1. B9.1
2. B9.2
3. B9.3
4. B9.4
5. B9.5

Phase 2 (objective visual/perf validation):

1. B9.6
2. B9.7
3. B9.8

Phase 3 (team workflow and governance):

1. B9.9
2. B9.10

## 8. Acceptance Checklist

Functional:

- same suite file on same machine produces reproducible captures (small bounded variance)
- each case can be applied without manual camera interaction
- compare command returns deterministic pass/fail for same inputs (visual + performance)

Artifacts:

- each run produces complete image + metadata + performance metrics + report bundle
- each failed visual case includes explicit metric values and diff output path
- each failed perf case includes metric delta and threshold source

Workflow:

- one capture command + one compare command is sufficient for day-to-day iteration
- baseline update is explicit and auditable
- visual and performance gates are configurable per suite/profile

## 9. Risks and Mitigations

- Non-determinism from dynamic scene/light/input:
  - Mitigation: lock input mode, deterministic warmup, optional fixed light animation seed/time.
- TAA/history sensitivity:
  - Mitigation: warmup frames and optional history reset policy per case.
- Cross-GPU numeric drift:
  - Mitigation: machine-specific baseline buckets or relaxed threshold profile by hardware class.
- Performance variance from runtime noise:
  - Mitigation: fixed warmup/sample strategy, repeated sampling with median aggregation, profile-specific tolerances.

## 10. Next Action

- Continue from current prototype:
  - complete B9.1 framework-level GPU readback screenshot API (replace window capture fallback)
  - migrate B9.6/B9.7/B9.8 from script prototype to integrated command workflow (or keep script as stable tooling)
  - add CI gate for capture+compare entry

## 11. Operational Notes (2026-02-28)

- Capture/compare loop is validated with exported case json from Demo UI.
- Runtime launch must use exe directory as working directory:
  - `glTFRenderer/x64/Debug`
- Absolute path input is recommended for `-regression-suite` and `-regression-output`.
- Typical failure causes observed:
  - wrong CWD causing `glTFResources/...` scene load failure
  - wrong CWD causing `Resources/Shaders/...` shader load failure
- Detailed command templates and troubleshooting are documented in:
  - `docs/FeatureNotes/20260228_B9_RegressionWorkflow_Runbook.md`

