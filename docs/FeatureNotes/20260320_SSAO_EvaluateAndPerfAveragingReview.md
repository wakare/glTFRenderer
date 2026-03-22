# SSAO Evaluate Refactor and Perf Averaging Review

## Summary

This note records the second SSAO optimization pass after the earlier blur split work.

The scope of this round was:

- add capture-window averaged pass statistics to `DemoAppModelViewer` regression output,
- extend `ViewBuffer` so screen-space passes can use explicit view/projection data,
- refactor `SSAO.hlsl` evaluate logic from world-space reconstruction toward a cheaper view-space formulation,
- rerun DX12 and Vulkan sweeps with the new measurement method.

This round improved measurement quality clearly.

The shader refactor also simplified the evaluate path, but the measured GPU win was modest:

- Vulkan default `16`-sample SSAO improved by about `0.0180 ms` (`0.3461 -> 0.3281 ms`, about `-5.2%`),
- DX12 did not show a stable improvement large enough to separate from timing noise.

## Code Changes

### 1. Capture-window averaged pass stats

`glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.*` now accumulates pass timings across the full capture window instead of writing only the final frame's pass snapshot.

New outputs:

- `.pass.csv` now reports averaged pass data across the capture window,
- `.perf.json` now includes `pass_stats_avg`,
- per-pass output includes presence ratio, executed ratio, skipped-validation ratio, and GPU-valid ratio.

This makes repeated SSAO captures materially easier to compare than the old "last frame only" method.

### 2. View/projection data in `ViewBuffer`

`glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.*`,
`glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`,
and `glTFRenderer/RendererDemo/Resources/Shaders/SceneViewCommon.hlsl` were extended so shaders now receive:

- `view_matrix`
- `projection_matrix`
- `inverse_projection_matrix`
- `projection_params`

That removed the need for SSAO to depend only on `inverse_view_projection_matrix`.

### 3. SSAO evaluate rewrite

`glTFRenderer/RendererDemo/Resources/Shaders/SSAO.hlsl` now:

- reconstructs center and sample positions in view space,
- transforms the decoded GBuffer normal into view space once per pixel,
- uses analytic projection based on `projection_params` instead of a per-sample projection matrix multiply,
- keeps the same separable blur path from the previous round.

The goal of this rewrite was to remove unnecessary world-space reconstruction work while staying close to the previous occlusion logic.

## Validation

Build:

```powershell
powershell -ExecutionPolicy Bypass -File .\glTFRenderer\scripts\Build-RendererDemo-Verify.ps1
```

Build result:

- success
- binlog: `glTFRenderer/build_logs/rendererdemo_20260320_012025.binlog`

Regression runs used:

- DX12 default perf suite:
  - `.tmp/regression_capture/rc_dx_20260320_012221/ssao_model_viewer_perf_20260320_012222/suite_result.json`
- Vulkan default perf suite:
  - `.tmp/regression_capture/rc_vk_20260320_012246/ssao_model_viewer_perf_20260320_012247/suite_result.json`
- DX12 latest sweep:
  - `.tmp/regression_capture/rc_dx_20260320_012925/ssao_model_viewer_sweep_20260320_012926/suite_result.json`
- Vulkan latest sweep:
  - `.tmp/regression_capture/rc_vk_20260320_012839/ssao_model_viewer_sweep_20260320_012840/suite_result.json`

Additional reruns were used to judge timing stability:

- DX12 noisy rerun:
  - `.tmp/regression_capture/rc_dx_20260320_012554/ssao_model_viewer_sweep_20260320_012555/suite_result.json`
- Vulkan rerun:
  - `.tmp/regression_capture/rc_vk_20260320_012640/ssao_model_viewer_sweep_20260320_012642/suite_result.json`

## Results

### Latest stable sweep snapshot

| Backend | Case | SSAO Evaluate | SSAO Blur X | SSAO Blur Y | SSAO Total |
| --- | --- | ---: | ---: | ---: | ---: |
| DX12 | default | 0.2811 ms | 0.0319 ms | 0.0308 ms | 0.3438 ms |
| DX12 | `blur_radius = 0` | 0.2808 ms | 0.0204 ms | 0.0204 ms | 0.3215 ms |
| DX12 | `sample_count = 8` | 0.1481 ms | 0.0204 ms | 0.0202 ms | 0.1887 ms |
| DX12 | `ssao_enabled = false` | 0.0207 ms | 0.0193 ms | 0.0195 ms | 0.0595 ms |
| Vulkan | default | 0.2687 ms | 0.0301 ms | 0.0293 ms | 0.3281 ms |
| Vulkan | `blur_radius = 0` | 0.2685 ms | 0.0206 ms | 0.0201 ms | 0.3092 ms |
| Vulkan | `sample_count = 8` | 0.1413 ms | 0.0201 ms | 0.0205 ms | 0.1819 ms |
| Vulkan | `ssao_enabled = false` | 0.0182 ms | 0.0191 ms | 0.0189 ms | 0.0562 ms |

### Comparison against the earlier blur-only baseline

The most useful apples-to-apples comparison is Vulkan, because it was much more repeatable.

Previous blur-only Vulkan sweep from the earlier note:

- default: `0.3461 ms`
- `blur_radius = 0`: `0.3103 ms`
- `sample_count = 8`: `0.1813 ms`
- `ssao_enabled = false`: `0.0564 ms`

Current Vulkan sweep:

- default: `0.3281 ms`
- `blur_radius = 0`: `0.3092 ms`
- `sample_count = 8`: `0.1819 ms`
- `ssao_enabled = false`: `0.0562 ms`

Delta:

- default: `-0.0180 ms` (`-5.2%`)
- `blur_radius = 0`: `-0.0011 ms`
- `sample_count = 8`: `+0.0006 ms`
- `ssao_enabled = false`: `-0.0002 ms`

Interpretation:

- the residual blur cost is basically unchanged,
- the disabled-path overhead is unchanged,
- the `8`-sample path is effectively unchanged,
- the measurable win shows up mainly in the upper half of the default `16`-sample evaluate path.

In other words, this refactor did not change the whole SSAO cost envelope. It only shaved a small amount off the heavier default evaluate workload.

## Measurement Notes

### What improved

The new capture-window averaging is worth keeping.

Compared with the old last-frame-only pass CSV:

- `pass_stats_avg` is far better for lightweight GPU passes,
- blur timings are now consistently comparable across runs,
- the perf JSON is directly scriptable for later comparisons.

### What did not fully improve

DX12 still showed large inter-run variance in `SSAO Evaluate`.

Observed behavior:

- one DX12 rerun stayed near `0.3438 ms` total,
- another DX12 rerun spiked to about `0.8484 ms` total for the same default sweep case,
- the spike was concentrated in `SSAO Evaluate`, not in `Blur X` / `Blur Y`.

So the capture-window averaging fixed the old final-frame problem, but it did not make DX12 fine-grained SSAO timing fully trustworthy.

Vulkan is currently the better backend for SSAO micro-benchmark decisions in this scene.

## Conclusions

### What was achieved

- SSAO regression output now records capture-window averaged pass data.
- `SSAO.hlsl` no longer depends on world-space reconstruction for its main evaluate logic.
- The view-space rewrite produced a small but repeatable Vulkan improvement on the default `16`-sample path.

### What this implies

The limited gain is a strong sign that the current SSAO path is no longer dominated only by matrix math.

This is an inference from the measurements, not a direct profiler proof, but the most likely explanation is:

- texture fetch latency and depth/normal bandwidth are now a large share of the remaining cost,
- the heavy default path is still sample-count sensitive,
- small math cleanups alone will not produce another large win.

### Recommended next steps

1. Skip SSAO blur passes entirely when `blur_radius == 0`, and skip all SSAO passes when `enabled == 0`, instead of paying dispatch overhead for early-out shaders.
2. If more savings are required, consider a larger algorithmic step such as half-resolution SSAO, hierarchical depth assistance, or a cheaper depth-only occlusion approximation.
3. Treat DX12 SSAO pass timing as a separate diagnostics problem; do not use it alone for sub-`0.02 ms` optimization decisions.

## TODO

1. Investigate the large DX12 vs Vulkan stability gap in `SSAO Evaluate` timing observed on `2026-03-20`. This may indicate backend-specific implementation issues rather than pure SSAO shader cost.
2. Check whether the divergence comes from backend-specific timestamp/profiler behavior, dispatch scheduling, resource barriers, or other DX12-only runtime details before making more SSAO micro-optimizations based on DX12 data alone.
