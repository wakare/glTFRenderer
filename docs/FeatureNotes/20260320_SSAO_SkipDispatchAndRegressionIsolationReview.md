# SSAO Skip-Dispatch and Regression Isolation Review

## Summary

This note records the third SSAO follow-up on `2026-03-20`, after the earlier blur split and evaluate refactor work.

Scope of this round:

- skip SSAO blur dispatches when `blur_radius == 0`,
- route lighting to the raw SSAO output when blur is skipped,
- keep the disabled path on the same routing so it no longer pays blur dispatch cost,
- fix regression case state leakage by restoring a suite baseline snapshot before every case,
- rerun DX12 and Vulkan VSync sweeps with capture-window averaged pass statistics.

The two important outcomes were:

- the skip-dispatch path produced the expected savings for `blur_radius = 0` and `ssao_enabled = false`,
- the regression harness had a real isolation bug, and one intermediate SSAO sweep had to be discarded because `ssao_blur0` leaked `blur_radius = 0` into `ssao_sample8`.

## Code Changes

### SSAO pass routing

Updated files:

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSSAO.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSSAO.cpp`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`

Behavior changes:

- `RendererSystemSSAO::GetOutputs()` now reports the active SSAO output dynamically,
- blur passes are registered and dispatched only when `enabled != 0` and `blur_radius > 0`,
- lighting now binds the active SSAO output every frame instead of assuming the blurred target is always valid.

This keeps the default path unchanged, but removes unnecessary blur dispatch overhead from the `blur_radius = 0` and disabled cases.

### Regression state isolation

Updated files:

- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.h`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`

Behavior changes:

- `ModelViewerStateSnapshot` now includes SSAO global parameters,
- snapshot JSON import/export now preserves SSAO state,
- regression startup captures a suite baseline snapshot once,
- every case now restores that baseline before applying case-specific overrides.

This fixes the actual source of cross-case contamination.

## Validation

Build command:

```powershell
powershell -ExecutionPolicy Bypass -File .\glTFRenderer\scripts\Build-RendererDemo-Verify.ps1
```

Build result:

- success
- binlog: `glTFRenderer/build_logs/rendererdemo_20260320_014750.binlog`

Final isolated regression runs:

- DX12:
  - `.tmp/regression_capture/dx12/ssao_model_viewer_sweep_20260320_014816/suite_result.json`
- Vulkan:
  - `.tmp/regression_capture/vulkan/ssao_model_viewer_sweep_20260320_014833/suite_result.json`

Intermediate reruns that should not be used for final `sample_count = 8` conclusions:

- DX12:
  - `.tmp/regression_capture/dx12/ssao_model_viewer_sweep_20260320_014356/suite_result.json`
- Vulkan:
  - `.tmp/regression_capture/vulkan/ssao_model_viewer_sweep_20260320_014412/suite_result.json`

Those intermediate runs were produced after the snapshot-serialization fix but before baseline restore was added. They still leaked state between cases.

## Results

### Final isolated sweep

| Backend | Case | SSAO Evaluate | SSAO Blur X | SSAO Blur Y | SSAO Total |
| --- | --- | ---: | ---: | ---: | ---: |
| DX12 | default | 0.2812 ms | 0.0319 ms | 0.0314 ms | 0.3444 ms |
| DX12 | `blur_radius = 0` | 0.2819 ms | skipped | skipped | 0.2819 ms |
| DX12 | `sample_count = 8` | 0.1485 ms | 0.0317 ms | 0.0314 ms | 0.2116 ms |
| DX12 | `ssao_enabled = false` | 0.0209 ms | skipped | skipped | 0.0209 ms |
| Vulkan | default | 0.2685 ms | 0.0298 ms | 0.0289 ms | 0.3273 ms |
| Vulkan | `blur_radius = 0` | 0.2682 ms | skipped | skipped | 0.2682 ms |
| Vulkan | `sample_count = 8` | 0.1413 ms | 0.0297 ms | 0.0296 ms | 0.2006 ms |
| Vulkan | `ssao_enabled = false` | 0.0183 ms | skipped | skipped | 0.0183 ms |

### Delta versus the pre-skip-dispatch sweep

Reference sweep before this round:

- DX12: `.tmp/regression_capture/rc_dx_20260320_012925/ssao_model_viewer_sweep_20260320_012926/suite_result.json`
- Vulkan: `.tmp/regression_capture/rc_vk_20260320_012839/ssao_model_viewer_sweep_20260320_012840/suite_result.json`

Most relevant deltas:

- DX12 default: `0.3438 -> 0.3444 ms` (`+0.0006 ms`, effectively noise)
- DX12 `blur_radius = 0`: `0.3215 -> 0.2819 ms` (`-0.0396 ms`, about `-12.3%`)
- DX12 `ssao_enabled = false`: `0.0595 -> 0.0209 ms` (`-0.0386 ms`, about `-64.9%`)
- Vulkan default: `0.3281 -> 0.3273 ms` (`-0.0008 ms`)
- Vulkan `blur_radius = 0`: `0.3092 -> 0.2682 ms` (`-0.0410 ms`, about `-13.3%`)
- Vulkan `ssao_enabled = false`: `0.0562 -> 0.0183 ms` (`-0.0379 ms`, about `-67.4%`)

Interpretation:

- the skip-dispatch path does exactly what it should for non-default cases,
- the default case does not improve materially, which is expected because blur is still part of the legal output path,
- the disabled path is much cheaper now, but it still pays one lightweight `SSAO Evaluate` dispatch because the raw-output route is still shader-driven.

### Regression harness correction

The first rerun after the skip-dispatch change showed `ssao_sample8` without `Blur X` and `Blur Y`.

That result was invalid.

Root cause:

- regression cases were not restoring a suite baseline state,
- `ssao_blur0` set `blur_radius = 0`,
- `ssao_sample8` only overrode `sample_count`,
- so `blur_radius = 0` leaked into the next case.

The final isolated sweep above confirms the fix:

- `ssao_sample8` now correctly includes blur passes again on both backends,
- only `blur_radius = 0` and `ssao_enabled = false` skip blur dispatches.

## Conclusions

### What this round achieved

- SSAO now avoids blur dispatch overhead when blur is not needed.
- Lighting correctly tracks the active SSAO output instead of assuming a fixed blurred target.
- Regression sweeps are now case-isolated again, so SSAO parameter sweeps are trustworthy.

### What remains

- `ssao_enabled = false` still dispatches `SSAO Evaluate` and early-outs, so the disabled path is not yet the absolute minimum cost.
- DX12 and Vulkan still do not behave equally well as micro-benchmark backends for SSAO timing.

## TODO

1. Skip the `SSAO Evaluate` dispatch too when `enabled == 0`, likely by binding a constant-white AO output or another no-dispatch path.
2. Investigate the large DX12 vs Vulkan stability gap in `SSAO Evaluate` timing observed on `2026-03-20`. This still looks large enough to indicate backend-specific implementation issues, not just normal run-to-run noise.
3. Check whether that backend divergence comes from timestamp/profiler behavior, dispatch scheduling, resource barriers, or other DX12-only runtime details before using DX12 alone for further SSAO micro-optimization decisions.
