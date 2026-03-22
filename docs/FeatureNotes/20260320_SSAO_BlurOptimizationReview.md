# SSAO Blur Optimization Review

## Summary

This change focused on the current `RendererDemo` SSAO path in `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSSAO.*`
and `glTFRenderer/RendererDemo/Resources/Shaders/SSAO.hlsl`.

The implemented optimization was intentionally low-risk:

- expose SSAO runtime parameters as a programmable API,
- hook those parameters into regression logic packs,
- replace the old single-pass 2D bilateral blur with a separable bilateral blur (`Blur X` + `Blur Y`).

The goal was to reduce the cost of the blur stage first, without rewriting the SSAO evaluate stage in the same change.

## Code Changes

### 1. SSAO parameter API

`RendererSystemSSAO` now exposes:

- `GetGlobalParams()`
- `SetGlobalParams(...)`

This makes SSAO settings scriptable in regression runs instead of being UI-only.

### 2. Regression control

`RegressionLogicPack` for `model_viewer_lighting` now accepts SSAO overrides:

- `ssao_enabled`
- `ssao_radius_world`
- `ssao_intensity`
- `ssao_power`
- `ssao_bias`
- `ssao_thickness`
- `ssao_sample_distribution_power`
- `ssao_blur_depth_reject`
- `ssao_blur_normal_reject`
- `ssao_sample_count`
- `ssao_blur_radius`

This was added so SSAO cost sweeps can be run without manual UI interaction.

### 3. Blur path refactor

Before this change, SSAO blur was:

- one full-screen compute pass,
- 2D bilateral kernel,
- default `blur_radius = 2`,
- effectively a 5x5 neighborhood.

After this change, SSAO blur is:

- one horizontal bilateral pass,
- one vertical bilateral pass,
- same rejection inputs (`depth`, `normal`, `AO`),
- much lower tap count per output pixel.

This does not yet rewrite the SSAO evaluate pass itself.

## Measurement Method

Build:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Build-RendererDemo-Verify.ps1
```

Perf capture:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite .tmp\ssao_model_viewer_perf_suite.json `
  -Backend dx `
  -DemoApp DemoAppModelViewer

powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite .tmp\ssao_model_viewer_perf_suite.json `
  -Backend vk `
  -DemoApp DemoAppModelViewer
```

Additional sweep:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite .tmp\ssao_model_viewer_sweep_suite.json `
  -Backend vk `
  -DemoApp DemoAppModelViewer
```

Scene/setup:

- `DemoAppModelViewer`
- default Sponza scene
- fixed camera
- directional light frozen
- regression UI disabled

## Baseline Before This Change

Measured with the original blur implementation:

| Backend | SSAO Evaluate | SSAO Blur | SSAO Total |
| --- | ---: | ---: | ---: |
| DX12 | 0.2799 ms | 0.1536 ms | 0.4335 ms |
| Vulkan | 0.2690 ms | 0.1416 ms | 0.4106 ms |

The original review conclusion was already clear:

- SSAO was one of the heaviest GPU passes in this frame,
- the blur stage was a meaningful part of that cost,
- the evaluate stage was still the dominant long-term target.

## After This Change

Using stable default-case captures from the new sweep:

| Backend | SSAO Evaluate | SSAO Blur X | SSAO Blur Y | SSAO Total |
| --- | ---: | ---: | ---: | ---: |
| DX12 | 0.2826 ms | 0.0358 ms | 0.0348 ms | 0.3532 ms |
| Vulkan | 0.2806 ms | 0.0338 ms | 0.0317 ms | 0.3461 ms |

Approximate default-case improvement:

| Backend | Before | After | Delta |
| --- | ---: | ---: | ---: |
| DX12 | 0.4335 ms | 0.3532 ms | -18.5% |
| Vulkan | 0.4106 ms | 0.3461 ms | -15.7% |

## Important Caveat

Repeated light-load captures showed occasional spikes in `SSAO Evaluate`, especially on DX12, and sometimes on Vulkan.

Observed pattern:

- the blur passes became consistently cheaper,
- the spikes appeared in `SSAO Evaluate`, not in `Blur X` or `Blur Y`,
- because the whole workload is very light, single-frame pass timing is noisier than ideal.

This means:

- the blur optimization itself is real,
- but single-frame pass CSV data is not stable enough to claim a perfectly clean per-run average on every backend.

For this reason, the blur result above should be treated as the steady-state direction, not as a fully noise-free benchmark.

## Remaining Optimization Space

The Vulkan sweep is the clearest decomposition because it was more stable:

| Case | SSAO Total |
| --- | ---: |
| Default | 0.3461 ms |
| `ssao_blur_radius = 0` | 0.3103 ms |
| `ssao_sample_count = 8` | 0.1813 ms |
| `ssao_enabled = false` | 0.0564 ms |

Interpretation:

- current separable blur is no longer the main problem,
- blur removal from default only saves about `0.0358 ms`,
- halving sample count from `16 -> 8` saves about `0.1648 ms`,
- fully disabling SSAO removes about `0.2897 ms` from the current default path.

So the remaining opportunity is now concentrated in the evaluate stage, not the blur stage.

## Conclusions

### What improved

- The blur stage became materially cheaper.
- SSAO is now easier to benchmark and tune through regression cases.
- The optimization was localized and did not require changing SSAO lighting semantics.

### What still needs work

The next meaningful optimization should target `ComputeSSAO(...)`, especially the repeated per-sample:

- world-position reconstruction,
- world-to-UV reprojection,
- sample-world reconstruction for occlusion validation.

That stage is still the main cost center after this blur refactor.

### Recommended follow-up

1. Add pass-window averaging to regression perf output instead of only final-frame pass CSV.
2. Rework SSAO evaluate toward a cheaper view-space or linear-depth formulation.
3. Consider skipping SSAO pass registration entirely when `enabled == 0` instead of dispatching early-out passes.
