# RendererDemo Perf Loop

This repo now has a scriptable DX12/Vulkan perf comparison loop for `RendererDemo`.

Path convention in this document:

- Commands assume the current working directory is repo root `C:\glTFRenderer`.
- Files inside the solution subtree therefore use the `glTFRenderer/` prefix.
- Runtime-only arguments such as `Resources/...` remain relative to `RendererDemo.exe`.

Related planning document:

- `glTFRenderer/scripts/RendererFramework-RefactorRoadmap.md`

## Entry Points

- Build only:
  - `powershell -ExecutionPolicy Bypass -File .\glTFRenderer\scripts\Build-RendererDemo-Verify.ps1`
- Run one API regression suite:
  - `powershell -ExecutionPolicy Bypass -File .\glTFRenderer\scripts\Run-RendererDemo-Regression.ps1 -Api DX12 -SuitePath .\glTFRenderer\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json`
- Compare two finished runs:
  - `powershell -ExecutionPolicy Bypass -File .\glTFRenderer\scripts\Compare-RendererDemo-Regression.ps1 -DxManifestPath <dx_run_result.json> -VkManifestPath <vk_run_result.json>`
- Full loop:
  - `powershell -ExecutionPolicy Bypass -File .\glTFRenderer\scripts\Invoke-RendererDemo-PerfLoop.ps1 -SuitePath .\glTFRenderer\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json -PresentMode Mailbox`

## Output

- Build logs go to `glTFRenderer/build_logs/` and always include:
  - `.msbuild.log`
  - `.stdout.log`
  - `.stderr.log`
  - `.binlog`
- Perf loop outputs go to `glTFRenderer/build_logs/perf_loop/session_<timestamp>/`
- Each API run emits a `run_result.json`
- Each suite emits a `suite_result.json`
- Each case emits:
  - screenshot
  - pass CSV
  - perf JSON
- Comparison emits:
  - `comparison_<timestamp>.json`
  - `comparison_<timestamp>.md`

## Perf Data Captured

Regression perf JSON now includes both pass-level and framework-level timing:

- total CPU / GPU
- frosted glass CPU / GPU
- frame total
- execute passes
- non-pass CPU
- frame wait total
- wait previous frame
- acquire command list
- acquire swapchain
- execution planning
- blit to swapchain
- submit command list
- present call
- prepare frame
- finalize submission

This makes it possible to separate pass cost from framework scheduling / present cost.

## Important Notes

- `RendererDemo.exe` expects the first runtime argument to be the demo name.
  - Current automation launches `DemoAppModelViewer`.
- Keep `--no-assert-dialog` enabled for automation runs.
- Use `MAILBOX` for uncapped API comparisons unless the test explicitly wants `VSYNC`.
- Do not stream build or regression logs to chat; inspect the generated files instead.
- The current comparison script is intended for same-suite DX12 vs Vulkan runs with overlapping case ids.
- API validation can heavily distort steady-state timing, especially on DX12.
  - Default behavior is now validation-off for perf runs.
  - Set `GLTF_RHI_VALIDATION=1` to re-enable API validation on both backends.
  - Set `GLTF_DX12_GPU_VALIDATION=1` to additionally enable DX12 GPU-based validation.
  - Set `GLTF_DX12_SYNC_QUEUE_VALIDATION=1` to additionally enable DX12 synchronized queue validation.

## Refactor Workflow

When using this loop to validate framework refactors:

- Always compare against the latest accepted baseline listed in `glTFRenderer/scripts/RendererFramework-RefactorRoadmap.md`
- Always run `MAILBOX` after each phase
- Also run `VSYNC` when the phase touches:
  - frame scheduling
  - wait placement
  - swapchain resize / recreate
  - present mode handling
  - frame-slot ownership
- Do not accept a refactor phase if it causes a material regression in:
  - `frame_total_avg_ms`
  - `frame_wait_total_avg_ms`
  - `prepare_frame_avg_ms`
  - `present_call_avg_ms`
  without an explicit explanation and follow-up plan

## Refactor Failure Learnings

Recent `P0` refactor attempts exposed a few practical rules for keeping framework cleanup safe:

- Prefer pure helper extraction before introducing new persistent runtime state.
  - A `FrameRingConfig` source-of-truth attempt regressed `MAILBOX DX12` throughput enough to fail the roadmap gate, even though `VSYNC` stayed broadly stable.
- Do not merge “semantic cleanup” and “state ownership change” in the same slice.
  - If the code currently derives counts on demand, first centralize the derivation logic in helpers.
  - Move to stored runtime state only after that helper-based version has proved stable.
- For structural-only slices, treat repeated `MAILBOX DX12` regressions as real until disproved.
  - If the same slice misses the gate in two runs, roll it back and split it smaller.
- Use `VSYNC` as a secondary sanity check, not as the acceptance signal for throughput-sensitive cleanup.
  - A slice can look stable under `VSYNC` while still harming uncapped `MAILBOX` throughput.
- Record failed slices in `glTFRenderer/scripts/RendererFramework-RefactorRoadmap.md`.
  - Failed attempts are useful because they constrain how the next refactor should be cut.

## First Practical Use

For the current `frosted_glass_b9_smoke` mailbox run, the biggest DX12 deltas were in:

- `frame_wait_total`
- `submit_command_list`
- `present_call`
- `non_pass_cpu`

That points to framework-side synchronization / submission cost as the first optimization target, before spending time on frosted-glass shader micro-tuning.

## Optimization Finding: Validation Bias

The first meaningful steady-state DX12 optimization came from removing default validation overhead from perf runs.

- Root cause:
  - DX12 factory initialization had been enabling `EnableDebugLayer`, GPU-based validation, and synchronized command queue validation by default.
  - This disproportionately inflated `submit_command_list`, `present_call`, `finalize_submission`, and overall `frame_wait_total`.
- Result on `frosted_glass_b9_smoke` with `MAILBOX`:
  - DX12 `submit_command_list_avg_ms`: `1.8240 -> 0.0647`
  - DX12 `present_call_avg_ms`: `0.3650 -> 0.1487`
  - DX12 `frame_wait_total_avg_ms`: `2.4349 -> 0.4745`
  - DX12 `frame_total_avg_ms`: `7.1260 -> 4.0494`
- Interpretation:
  - Before comparing DX12 vs Vulkan framework scheduling, first make sure both backends are on a comparable validation/debug path.
  - Otherwise the perf loop mainly measures validation overhead instead of renderer architecture cost.
