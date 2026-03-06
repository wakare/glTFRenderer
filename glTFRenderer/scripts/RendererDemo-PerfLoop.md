# RendererDemo Perf Loop

This repo now has a scriptable DX12/Vulkan perf comparison loop for `RendererDemo`.

## Entry Points

- Build only:
  - `powershell -ExecutionPolicy Bypass -File .\scripts\Build-RendererDemo-Verify.ps1`
- Run one API regression suite:
  - `powershell -ExecutionPolicy Bypass -File .\scripts\Run-RendererDemo-Regression.ps1 -Api DX12 -SuitePath .\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json`
- Compare two finished runs:
  - `powershell -ExecutionPolicy Bypass -File .\scripts\Compare-RendererDemo-Regression.ps1 -DxManifestPath <dx_run_result.json> -VkManifestPath <vk_run_result.json>`
- Full loop:
  - `powershell -ExecutionPolicy Bypass -File .\scripts\Invoke-RendererDemo-PerfLoop.ps1 -SuitePath .\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json -PresentMode Mailbox`

## Output

- Build logs go to `build_logs/` and always include:
  - `.msbuild.log`
  - `.stdout.log`
  - `.stderr.log`
  - `.binlog`
- Perf loop outputs go to `build_logs/perf_loop/session_<timestamp>/`
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

## First Practical Use

For the current `frosted_glass_b9_smoke` mailbox run, the biggest DX12 deltas were in:

- `frame_wait_total`
- `submit_command_list`
- `present_call`
- `non_pass_cpu`

That points to framework-side synchronization / submission cost as the first optimization target, before spending time on frosted-glass shader micro-tuning.
