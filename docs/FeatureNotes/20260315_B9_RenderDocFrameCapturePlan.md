# Feature Note - B9 RenderDoc Frame Capture Integration Plan

Companion references:

- Chinese companion:
  - `docs/FeatureNotes/20260315_B9_RenderDocFrameCapturePlan_ZH.md`

- Date: 2026-03-15
- Work Item ID: B9
- Title: RenderDoc frame-capture integration for regression workflow
- Owner: AI coding session
- Related Plan:
  - `docs/FeatureNotes/20260228_B9_FixedViewpointScreenshotRegressionPlan.md`
  - `docs/FeatureNotes/20260228_B9_RegressionWorkflow_Runbook.md`
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Requirement Update

- New requirement:
  - Add optional automatic RenderDoc `.rdc` frame capture to the existing `RendererDemo` regression workflow.
  - Keep screenshot/perf capture behavior unchanged; RenderDoc is an additional artifact, not a replacement.
  - Prefer integration with the existing `-regression` and suite-driven batch flow instead of a separate capture path.

## 2. Problem Statement

- Current B9 flow already produces deterministic case artifacts:
  - screenshot `.png`
  - pass timing `.pass.csv`
  - per-case summary `.perf.json`
- Current screenshot capture is a window-client fallback and does not preserve GPU command/state history.
- When a visual or performance regression is detected, there is no aligned GPU-debug artifact that can be replayed in RenderDoc.
- Ad hoc manual RenderDoc capture is too slow and too error-prone for repeated regression work:
  - wrong frame selected
  - screenshot and `.rdc` do not match
  - DX12/Vulkan teardown behavior becomes harder to reason about

## 3. Design Goals

- Primary goals:
  - Generate `.rdc` as an optional per-case artifact in the same regression run.
  - Keep frame selection deterministic and aligned with the existing warmup/capture gate.
  - Make the feature fail-soft when RenderDoc is not installed or not injected.
  - Preserve legal runtime behavior; do not narrow supported rendering paths just to make capture succeed.

- Non-goals for the first slice:
  - No RenderDoc-based image diff engine.
  - No mandatory CI dependency on RenderDoc installation.
  - No custom replay automation inside RenderDoc for v1.

## 4. Integration Architecture

- Framework layer:
  - Add a lightweight optional `RenderDocCaptureService` in `glTFRenderer/RendererCore`.
  - Load `renderdoc_app.h` API dynamically and treat the service as a runtime helper similar in lifecycle sensitivity to other GPU-backed services.
  - Expose a small request-driven interface from `RenderGraph`:
    - set capture file path template
    - arm a capture for a specific upcoming frame
    - query whether the last request succeeded and which file was produced

- Frame-lifecycle placement:
  - Arm capture before the target frame starts executing.
  - Start frame capture at frame begin or immediately before render-graph execution for the armed frame.
  - End frame capture after final submit/present for that same frame.
  - On shutdown or runtime rebuild, wait for frame completion and GPU idle before releasing the service.

- Demo/regression layer:
  - Extend regression suite config with an optional RenderDoc capture switch.
  - Allow future per-case control while keeping suite-level defaults.
  - Record `.rdc` output path and capture status into `suite_result.json`.

- Script/workflow layer:
  - Keep `Compare-RendererRegression.ps1` focused on screenshot/perf comparison.
  - Surface `.rdc` paths in summary artifacts so failed cases can be opened directly in RenderDoc.

## 5. Proposed Contract Changes

- CLI:
  - add `-renderdoc-ui` to preload RenderDoc before device initialization for the shared DemoBase manual-capture UI without forcing regression artifacts
  - add `-renderdoc-capture` to enable RenderDoc capture globally for a run
  - add `-renderdoc-required` to fail initialization if RenderDoc API is unavailable
  - keep `-regression` / `-regression-suite` / `-regression-output` as the main entry

- Suite schema:
  - extend capture config with:
    - `capture_renderdoc`
    - `renderdoc_capture_frame_offset`
    - `keep_renderdoc_on_success`
  - extend suite-level defaults with:
    - `default_capture_renderdoc`
    - `default_renderdoc_capture_frame_offset`
    - `default_keep_renderdoc_on_success`

- Per-case output:
  - case-scoped `.rdc` artifact written under the regression run `cases/` directory
  - `suite_result.json` fields:
    - `renderdoc_capture_path`
    - `renderdoc_capture_success`
    - `renderdoc_capture_retained`
    - `renderdoc_capture_keep_on_success`
    - `renderdoc_capture_error`

## 6. Priority and Dependency Plan

Status legend: `Planned`, `In Progress`, `Blocked`, `Accepted`

| Sub-item | Scope | Priority | Dependency | Status | Output |
|---|---|---|---|---|---|
| B9.R1 | Third-party RenderDoc API bootstrap | P0 | - | Accepted | `renderdoc_app.h` integration + dynamic loader |
| B9.R2 | Framework capture service and frame hooks | P0 | B9.R1 | Accepted | request/arm/start/end/teardown path in `RendererCore` |
| B9.R3 | Regression schema + CLI enable path | P0 | B9.R2 | Accepted | suite/argument parsing and opt-in behavior |
| B9.R4 | Per-case `.rdc` artifact export and summary wiring | P0 | B9.R2, B9.R3 | Accepted | `.rdc` files + `suite_result.json` fields |
| B9.R5 | Runbook/script updates | P1 | B9.R4 | Accepted | documented capture command and artifact interpretation |
| B9.R6 | Validation on DX12 and Vulkan | P0 | B9.R4 | Accepted | successful capture and clean shutdown on both backends |

## 7. Execution Order (Recommended)

Phase 1 (minimum usable integration):

1. B9.R1
2. B9.R2
3. B9.R3
4. B9.R4

Phase 2 (workflow hardening):

1. B9.R5
2. B9.R6

## 8. Acceptance Checklist

Functional:

- Same regression suite can optionally emit `.rdc` per selected case.
- `.png`, `.perf.json`, and `.rdc` correspond to the same intended case/frame.
- Missing RenderDoc runtime does not break normal screenshot/perf capture unless `-renderdoc-required` is set.
- `suite_result.json` records capture success/failure and artifact path.

Runtime safety:

- DX12 run exits cleanly after RenderDoc capture.
- Vulkan run exits cleanly after RenderDoc capture.
- Runtime rebuild/shutdown order still waits for frame completion and GPU idle before GPU-helper teardown.

Workflow:

- Day-to-day regression still needs one capture command and one compare command.
- Failed cases have enough metadata to open the matching `.rdc` directly for diagnosis.

## 9. Risks and Mitigations

- Frame mismatch between screenshot/perf and `.rdc`:
  - Mitigation: tie capture arming to absolute frame index in framework, not to late case finalization only.

- RenderDoc availability differs across machines:
  - Mitigation: default fail-soft mode; explicit `-renderdoc-required` for strict environments.

- Large artifact size:
  - Mitigation: keep capture opt-in and per-case selectable; successful cases can now prune `.rdc` automatically through `keep_renderdoc_on_success=false`.

- Backend-specific shutdown regressions:
  - Mitigation: reuse existing frame-complete and GPU-idle shutdown ordering, validate on both DX12 and Vulkan before considering the feature usable.

## 10. Current Status

- Minimum integration is complete in `RendererCore`, `RendererDemo`, and regression suite parsing.
- Workflow hardening is complete through `scripts/Capture-RendererRegression.ps1`, `scripts/Compare-RendererRegression.ps1`, and the B9 runbook updates.
- Unified validation entrypoint is available through `scripts/Validate-RendererRegression.ps1`, so DX12 and Vulkan can be exercised through the same build/capture/compare flow.
- `DemoBase` now exposes shared manual RenderDoc controls under `Runtime / Diagnostics > RenderDoc`, including one-frame capture, replay auto-open, and reopen-last-capture actions.
- The shared manual UI remains opt-in through `-renderdoc-ui`, so plain non-RenderDoc runs keep their original perf behavior.
- Runtime RHI recreation now re-runs the target-backend RenderDoc preload step when RenderDoc startup flags are active, so switching between DX12 and Vulkan stays aligned with the same opt-in capture contract.
- Validation has been completed on RenderDoc 1.43 for:
  - DX12 required capture path
  - Vulkan automatic implicit-layer activation path
  - compare-summary retention of RenderDoc metadata
- Screenshot capture hardening is complete:
  - RenderDoc overlay is masked off during regression capture
  - screenshot export now prefers `PrintWindow` before GDI `BitBlt`, which removes normal desktop occlusion from repeatability testing
- Compare workflow hardening is complete:
  - cases with successful RenderDoc `.rdc` capture automatically skip perf threshold evaluation
  - compare summaries retain RenderDoc metadata and retained artifact paths for follow-up debugging
- Regression suite schema hardening is complete:
  - `renderdoc_capture_frame_offset` can delay the aligned RenderDoc/screenshot/pass-csv finalization frame
  - `keep_renderdoc_on_success=false` can prune successful `.rdc` artifacts while keeping summary metadata and compare-side `rdc-pruned` notes
- Script-path hardening is complete:
  - capture and unified validation now use compact internal run directories (`rc_*` / `rv_*`) to avoid Windows path-length failures on `.pass.csv`, `.perf.json`, and `.rdc`
  - unified validation also reaps child script processes once `STATUS=` has been emitted, preventing wrapper hangs after successful completion

## 11. Operational Notes (2026-03-15)

- This plan intentionally extends B9 instead of creating a parallel capture pipeline.
- Existing screenshot capture remains a separate fallback path and should not be conflated with RenderDoc `.rdc` generation.
- Initial implementation should prioritize deterministic alignment and safe teardown over convenience features.
- Local validation on the current workstation confirmed:
  - DX12 automatic RenderDoc preload and capture succeed end-to-end.
  - Vulkan automatic implicit-layer activation succeeds end-to-end when a registered RenderDoc 1.43+ installation is present.
  - Older or incomplete RenderDoc installations may still require launching from RenderDoc UI or another already-injected environment.
  - RenderDoc capture perturbs perf numbers enough that compare now treats `.rdc` cases as visual-debug captures, not perf gates.
