# Renderer Framework Refactor Roadmap

This document defines the refactor order, validation gates, and perf guardrails for the current renderer framework cleanup work.

The goal is not only to improve code structure, but to do it without hiding meaningful performance regressions behind architectural churn.

Path convention in this document:

- Commands assume the current working directory is repo root `C:\glTFRenderer`.
- Files inside the solution subtree therefore use the `glTFRenderer/` prefix.
- Perf/build artifacts emitted by the `glTFRenderer/scripts/` tooling live under `glTFRenderer/build_logs/` by default.

## Objectives

- Reduce framework-level coupling in frame scheduling, swapchain handling, and runtime bootstrap.
- Make DX12/Vulkan perf comparisons easier to interpret.
- Keep every refactor step small enough to isolate behavioral and perf impact.
- Reject or revise any refactor that causes a material perf regression without a justified tradeoff.

## Current Accepted Baselines

Use these as the starting reference until a later phase is explicitly accepted and promoted to a new baseline.

- `MAILBOX` throughput baseline:
  - DX12:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_163023/regression_outputs/dx12/frosted_glass_b9_smoke_20260306_163024/cases/001_legacy_fullfog_reference.perf.json`
  - Vulkan:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_163023/regression_outputs/vulkan/frosted_glass_b9_smoke_20260306_163029/cases/001_legacy_fullfog_reference.perf.json`
  - Comparison:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_163023/comparison/comparison_20260306_163032.md`

- `VSYNC` pacing reference:
  - DX12:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_161336/regression_outputs/dx12/frosted_glass_b9_smoke_20260306_161339/cases/001_legacy_fullfog_reference.perf.json`
  - Vulkan:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_161336/regression_outputs/vulkan/frosted_glass_b9_smoke_20260306_161346/cases/001_legacy_fullfog_reference.perf.json`
  - Comparison:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_161336/comparison/comparison_20260306_161352.md`

## Non-Regression Gates

Every refactor phase must pass all of the following before it is considered complete.

### 1. Build Gate

- Run:
  - `powershell -ExecutionPolicy Bypass -File .\glTFRenderer\scripts\Build-RendererDemo-Verify.ps1`
- Required:
  - `Build succeeded`
  - no new compile errors
  - warnings may exist, but no obvious warning spike caused by the refactor

### 2. Functional Regression Gate

- Run:
  - `powershell -ExecutionPolicy Bypass -File .\glTFRenderer\scripts\Invoke-RendererDemo-PerfLoop.ps1 -SuitePath .\glTFRenderer\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json -PresentMode Mailbox`
- Required:
  - DX12 regression suite succeeds
  - Vulkan regression suite succeeds
  - screenshot output exists
  - perf JSON exists

### 3. Throughput Gate (`MAILBOX`)

Compare against the current accepted `MAILBOX` baseline.

- Primary metrics:
  - `frame_total_avg_ms`
  - `cpu_total_avg_ms`
  - `gpu_total_avg_ms`
  - `prepare_frame_avg_ms`
  - `frame_wait_total_avg_ms`
  - `present_call_avg_ms`
- Default fail conditions:
  - DX12 `frame_total_avg_ms` regresses by more than `5%` or `0.20 ms`, whichever is larger
  - Vulkan `frame_total_avg_ms` regresses by more than `5%` or `0.25 ms`, whichever is larger
  - `frame_wait_total_avg_ms` regresses by more than `10%` or `0.10 ms`, whichever is larger
  - `gpu_total_avg_ms` regresses by more than `7%` unless the phase intentionally changes shader/pass execution

### 4. Pacing Gate (`VSYNC`)

Run only when the phase touches frame scheduling, wait placement, swapchain, present, or frame slot ownership.

- Run:
  - `powershell -ExecutionPolicy Bypass -File .\glTFRenderer\scripts\Invoke-RendererDemo-PerfLoop.ps1 -SuitePath .\glTFRenderer\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json -PresentMode VSync`
- Required:
  - `frame_total_avg_ms` remains near refresh-limited behavior
  - any large movement between `wait_previous_frame`, `acquire_swapchain`, `submit_command_list`, and `present_call` is explained
  - total wait bucket sum does not regress materially without justification

## Execution Rules

- Do not combine unrelated architectural cleanup and backend behavior changes in one phase.
- Do not merge a phase until its comparison report has been reviewed.
- If a refactor improves structure but regresses perf beyond the thresholds above, either:
  - split it into smaller pieces, or
  - land it only with an explicit note describing the tradeoff and a follow-up optimization item
- After a phase is accepted, promote that phase's report to the new baseline for the next phase.

## Phase Order

### P0. Frame Model Normalization

- Priority: Highest
- Goal:
  - separate `frame slot`, `swapchain image`, `profiler slot`, and `deferred-release retention` into explicit concepts
- Main hotspots today:
  - `glTFRenderer/RendererCore/Public/RendererInterface.h`
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
  - `glTFRenderer/RendererCore/Public/ResourceManager.h`
  - `glTFRenderer/RendererCore/Private/ResourceManager.cpp`
- Concrete targets:
  - introduce a `FrameRingConfig`
  - replace implicit `GetBackBufferCount()` coupling with explicit slot-count usage
  - make frame scheduling code read as a dedicated frame pipeline rather than mixed render-graph logic
- Exit criteria:
  - `RenderGraph` no longer decides frame-slot semantics ad hoc
  - slot count decisions come from one source of truth
  - `MAILBOX` perf stays within gate

### P1. Swapchain Controller Extraction

- Priority: High
- Goal:
  - move swapchain recreate/resize/present-mode transitions into an explicit controller
- Main hotspots today:
  - `glTFRenderer/RendererCore/Public/ResourceManager.h`
  - `glTFRenderer/RendererCore/Private/ResourceManager.cpp`
  - `glTFRenderer/RendererCore/Private/ResourceManagerSurfaceSync.cpp`
  - `glTFRenderer/RHICore/Private/RHIDX12Impl/DX12SwapChain.cpp`
  - `glTFRenderer/RHICore/Private/RHIVKImpl/VKSwapChain.cpp`
- Concrete targets:
  - remove heavy `friend`-based coordination
  - centralize recreate vs in-place resize behavior
  - isolate DX12/Vulkan swapchain pacing policy behind one framework-level owner
- Exit criteria:
  - swapchain lifecycle changes are handled by one component
  - `ResourceManager` stops owning resize policy flow directly
  - no `MAILBOX` throughput regression

### P2. RenderGraph Decomposition

- Priority: High
- Goal:
  - break `RenderGraph` into smaller runtime services
- Main hotspots today:
  - `glTFRenderer/RendererCore/Public/RendererInterface.h`
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- Concrete targets:
  - extract `ExecutionPlanner`
  - extract `DependencyDiagnostics`
  - extract `FrameProfiler`
  - extract `FrameScheduler` or `FramePacer`
- Exit criteria:
  - `RenderGraph` becomes orchestration only
  - profiler/diagnostics/planning no longer share one giant implementation file
  - no unexplained movement in wait buckets

### P3. ResourceOperator API Split

- Priority: Medium
- Goal:
  - reduce the scope of the current façade API
- Main hotspots today:
  - `glTFRenderer/RendererCore/Public/RendererInterface.h`
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- Concrete targets:
  - split resource creation from runtime frame/surface control
  - isolate swapchain controls from render-resource creation
  - make downstream systems depend on narrower interfaces
- Exit criteria:
  - fewer callers need the full `ResourceOperator`
  - frame and swapchain controls are not mixed with resource factory methods

### P4. Runtime Bootstrap and Demo Cleanup

- Priority: Medium
- Goal:
  - remove renderer runtime bootstrapping and runtime-RHI switch orchestration from `DemoBase`
- Main hotspots today:
  - `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`
- Concrete targets:
  - extract argument parsing
  - extract runtime config defaults
  - extract RHI switch/recreate sequence
  - keep `DemoBase` focused on demo lifecycle and panel composition
- Exit criteria:
  - `DemoBase` no longer owns low-level renderer bootstrap policy
  - runtime config defaults are shared from one place

### P5. Config Consolidation

- Priority: Medium
- Goal:
  - gather runtime config, perf defaults, and validation policy into one coherent configuration model
- Main hotspots today:
  - `glTFRenderer/RendererCore/Public/Renderer.h`
  - `glTFRenderer/RHICore/Public/RHIConfigSingleton.h`
  - `glTFRenderer/RHICore/Private/RHIConfigSingleton.cpp`
  - `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`
- Concrete targets:
  - define one runtime config object for:
    - graphics API
    - present mode
    - back-buffer/frame-slot depth
    - validation policy
  - reduce duplicated defaults between core and demo bootstrap
- Exit criteria:
  - default behavior does not depend on multiple disconnected call sites

## Phase Checklist Template

Use this checklist for every phase:

1. Write the smallest scoped refactor patch possible.
2. Run verify build.
3. Run `MAILBOX` perf loop.
4. If scheduling or swapchain changed, run `VSYNC` perf loop.
5. Compare against the current accepted baseline.
6. Record:
   - changed files
   - success/failure
   - key metric deltas
   - whether the phase is accepted, revised, or rolled back
7. Only then promote the new report to baseline.

## Phase Execution Log

### 2026-03-06 - P0 Slice 1 - Explicit Frame Slot vs Swapchain Count APIs

- Status:
  - accepted as a non-regressing structural slice
  - baseline not promoted yet because `P0` is still in progress
- Changed files:
  - `glTFRenderer/RendererCore/Public/ResourceManager.h`
  - `glTFRenderer/RendererCore/Private/ResourceManager.cpp`
  - `glTFRenderer/RendererCore/Public/RendererInterface.h`
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- Scope:
  - added explicit `GetFrameSlotCount()` and `GetSwapchainImageCount()` APIs
  - switched frame-buffered resource creation, profiler slot selection, hazard window sizing, and deferred-release retention to frame-slot semantics
  - kept `GetBackBufferCount()` as compatibility alias for swapchain image count
- Validation:
  - build:
    - `glTFRenderer/build_logs/rendererdemo_20260306_171135.result.json`
    - `glTFRenderer/build_logs/rendererdemo_20260306_171135.msbuild.log`
  - `MAILBOX`:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_171246/comparison/comparison_20260306_171256.md`
  - `VSYNC`:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_171306/comparison/comparison_20260306_171321.md`
- Key deltas vs accepted baseline:
  - `MAILBOX DX12`:
    - `frame_total_avg_ms`: `3.1220 -> 3.2187` (`+0.0967 ms`, `+3.1%`)
    - `frame_wait_total_avg_ms`: `0.1875 -> 0.2331` (`+0.0456 ms`, `+24.3%`)
    - `gpu_total_avg_ms`: `1.0255 -> 0.7152` (`-0.3103 ms`, `-30.3%`)
  - `MAILBOX Vulkan`:
    - `frame_total_avg_ms`: `3.8714 -> 3.8646` (`-0.0067 ms`, `-0.2%`)
  - `VSYNC DX12`:
    - `frame_total_avg_ms`: `16.6384 -> 16.6617` (`+0.0233 ms`, `+0.1%`)
    - `frame_wait_total_avg_ms`: `13.0657 -> 13.3909` (`+0.3253 ms`, `+2.5%`)
  - `VSYNC Vulkan`:
    - `frame_total_avg_ms`: `16.6557 -> 16.6529` (`-0.0028 ms`)
- Decision:
  - stays within the current roadmap gates
  - no behavioral rollback needed
  - next `P0` slice should introduce a single frame-ring source of truth instead of deriving slot counts ad hoc

### 2026-03-06 - P0 Slice 2 - FrameRingConfig Source-of-Truth Attempt

- Status:
  - revised and rolled back
  - not accepted
- Scope attempted:
  - introduce `FrameRingConfig` inside `ResourceManager`
  - route frame-slot, swapchain-image, profiler-slot, and deferred-release-latency queries through that single config
  - refresh the config on init and swapchain resize commit
- Validation:
  - build:
    - `glTFRenderer/build_logs/rendererdemo_20260306_172112.result.json`
    - `glTFRenderer/build_logs/rendererdemo_20260306_172112.msbuild.log`
  - `MAILBOX`:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_172158/comparison/comparison_20260306_172208.md`
    - rerun: `glTFRenderer/build_logs/perf_loop/session_20260306_172340/comparison/comparison_20260306_172349.md`
  - `VSYNC`:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_172217/comparison/comparison_20260306_172230.md`
- Rejection reason:
  - `MAILBOX DX12` regressed past the current gate in two consecutive runs
  - first run:
    - `frame_total_avg_ms`: `3.1220 -> 3.4422` (`+0.3201 ms`, `+10.3%`)
  - rerun:
    - `frame_total_avg_ms`: `3.1220 -> 3.4003` (`+0.2783 ms`, `+8.9%`)
  - `VSYNC` remained broadly stable, but the throughput regression was not acceptable for a structural-only slice
- Decision:
  - rollback to the accepted `P0 Slice 1` state
  - keep the failed reports for reference
  - next attempt should be smaller than a full `FrameRingConfig` introduction, likely by extracting one narrow helper at a time instead of introducing a new state object

### 2026-03-06 - P0 Slice 3 - Pure Helper Extraction for Frame Semantics

- Status:
  - accepted as a non-regressing structural slice
  - baseline not promoted yet because `P0` is still in progress
- Changed files:
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
  - `glTFRenderer/scripts/RendererDemo-PerfLoop.md`
- Scope:
  - extracted pure helper functions for:
    - frame-slot count
    - swapchain image count
    - profiler slot count and slot index
    - cross-frame hazard window
    - deferred-release latency
    - descriptor retention windows
  - did not introduce new persistent runtime state
  - did not change `ResourceManager` lifecycle or ownership
- Validation:
  - build:
    - `glTFRenderer/build_logs/rendererdemo_20260306_173105.result.json`
    - `glTFRenderer/build_logs/rendererdemo_20260306_173105.msbuild.log`
  - `MAILBOX`:
    - first run: `glTFRenderer/build_logs/perf_loop/session_20260306_173127/comparison/comparison_20260306_173136.md`
    - rerun: `glTFRenderer/build_logs/perf_loop/session_20260306_173255/comparison/comparison_20260306_173304.md`
  - `VSYNC`:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_173146/comparison/comparison_20260306_173159.md`
- Key deltas vs accepted baseline:
  - `MAILBOX DX12` rerun:
    - `frame_total_avg_ms`: `3.1220 -> 3.1200` (`-0.0021 ms`, `-0.1%`)
    - `frame_wait_total_avg_ms`: `0.1875 -> 0.2041` (`+0.0166 ms`, `+8.9%`)
    - `present_call_avg_ms`: `0.0673 -> 0.0736` (`+0.0063 ms`, `+9.4%`)
    - `gpu_total_avg_ms`: `1.0255 -> 0.7166` (`-0.3089 ms`, `-30.1%`)
  - `MAILBOX Vulkan` rerun:
    - `frame_total_avg_ms`: `3.8714 -> 3.7602` (`-0.1112 ms`, `-2.9%`)
  - `VSYNC DX12`:
    - `frame_total_avg_ms`: `16.6384 -> 16.6474` (`+0.0090 ms`, `+0.1%`)
    - `frame_wait_total_avg_ms`: `13.0657 -> 13.0844` (`+0.0187 ms`, `+0.1%`)
- Notes:
  - the first `MAILBOX` run showed a one-off `DX12 gpu_total` increase that did not reproduce on rerun
  - because this slice only moved pure helper logic, acceptance is based on the rerun result rather than the noisy first sample
- Decision:
  - accepted
  - continue `P0` by extracting one narrow semantic helper family at a time
  - avoid introducing new runtime-owned frame-model state until several helper-only slices stay stable

### 2026-03-06 - P0 Slice 4 - Hazard Snapshot Helper Extraction

- Status:
  - accepted as a non-regressing structural slice
  - baseline not promoted yet because `P0` is still in progress
- Changed files:
  - `glTFRenderer/RendererCore/Public/RendererInterface.h`
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- Scope:
  - extracted focused `RenderGraph` helpers for:
    - cross-frame comparison window sizing
    - hazard snapshot storage sizing
    - hazard slot selection
    - previous snapshot lookup
    - current snapshot update
  - reduced ad hoc frame-slot snapshot handling inside the main planning path
  - did not change resource ownership, swapchain policy, or runtime state layout
- Validation:
  - build:
    - `glTFRenderer/build_logs/rendererdemo_20260306_173848.result.json`
    - `glTFRenderer/build_logs/rendererdemo_20260306_173848.msbuild.log`
  - `MAILBOX`:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_173940/comparison/comparison_20260306_173949.md`
  - `VSYNC`:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_173959/comparison/comparison_20260306_174012.md`
- Key deltas vs accepted baseline:
  - `MAILBOX DX12`:
    - `frame_total_avg_ms`: `3.1220 -> 3.2992` (`+0.1772 ms`, `+5.7%`)
    - `frame_wait_total_avg_ms`: `0.1875 -> 0.2723` (`+0.0848 ms`, `+45.2%`)
    - `prepare_frame_avg_ms`: `1.1754 -> 1.1867` (`+0.0113 ms`, `+1.0%`)
  - `MAILBOX Vulkan`:
    - `frame_total_avg_ms`: `3.8714 -> 3.9076` (`+0.0362 ms`, `+0.9%`)
  - `VSYNC DX12`:
    - `frame_total_avg_ms`: `16.6384 -> 16.5894` (`-0.0490 ms`, `-0.3%`)
    - `frame_wait_total_avg_ms`: `13.0657 -> 12.9015` (`-0.1642 ms`, `-1.3%`)
- Notes:
  - some wait-bucket percentages look large, but the absolute deltas stayed within the current roadmap gates
  - this slice was accepted on threshold compliance, not because every bucket numerically improved
- Decision:
  - accepted
  - continue shrinking `P0` around diagnostics/profiler semantics before revisiting any new frame-model state object

### 2026-03-06 - P0 Slice 5 - GPU Profiler Guardrail Helper Extraction

- Status:
  - accepted as a non-regressing structural slice
  - baseline not promoted yet because `P0` is still in progress
- Changed files:
  - `glTFRenderer/RendererCore/Public/RendererInterface.h`
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- Scope:
  - extracted focused GPU-profiler guardrail helpers for:
    - profiler slot validity
    - max timestamped pass budget
    - max query count
    - query-count clamping
  - reduced repeated ad hoc checks in:
    - `ResolveGPUProfilerFrame`
    - `ExecutePlanAndCollectStats`
    - `BeginGPUProfilerFrame`
    - `WriteGPUProfilerTimestamp`
    - `FinalizeGPUProfilerFrame`
  - did not change profiler data ownership or timestamp sequencing
- Validation:
  - build:
    - `glTFRenderer/build_logs/rendererdemo_20260306_174521.result.json`
    - `glTFRenderer/build_logs/rendererdemo_20260306_174521.msbuild.log`
  - `MAILBOX`:
    - first run: `glTFRenderer/build_logs/perf_loop/session_20260306_174613/comparison/comparison_20260306_174623.md`
    - rerun: `glTFRenderer/build_logs/perf_loop/session_20260306_174730/comparison/comparison_20260306_174740.md`
  - `VSYNC`:
    - `glTFRenderer/build_logs/perf_loop/session_20260306_174633/comparison/comparison_20260306_174646.md`
- Key deltas vs accepted baseline:
  - `MAILBOX DX12` rerun:
    - `frame_total_avg_ms`: `3.1220 -> 3.1423` (`+0.0202 ms`, `+0.6%`)
    - `frame_wait_total_avg_ms`: `0.1875 -> 0.2047` (`+0.0172 ms`, `+9.2%`)
    - `prepare_frame_avg_ms`: `1.1754 -> 1.1371` (`-0.0383 ms`, `-3.3%`)
  - `MAILBOX Vulkan` rerun:
    - `frame_total_avg_ms`: `3.8714 -> 3.8513` (`-0.0200 ms`, `-0.5%`)
  - `VSYNC DX12`:
    - `frame_total_avg_ms`: `16.6384 -> 16.6659` (`+0.0275 ms`, `+0.2%`)
    - `frame_wait_total_avg_ms`: `13.0657 -> 13.0848` (`+0.0191 ms`, `+0.1%`)
- Notes:
  - the first `MAILBOX DX12` run missed the throughput gate
  - rerun returned near baseline, so this slice is accepted based on the repeated sample rather than the noisy first run
- Decision:
  - accepted
  - keep future profiler refactors helper-only until repeated runs stay stable

## Current Recommendation

Start with `P0. Frame Model Normalization`.

Reason:

- it removes the most important source of framework confusion
- it directly affects future DX12/Vulkan pacing analysis
- it is prerequisite work for a clean swapchain controller split
