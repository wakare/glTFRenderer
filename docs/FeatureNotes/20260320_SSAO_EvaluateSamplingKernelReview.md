# SSAO Evaluate Sampling Kernel Review

## Summary

This note records the fourth SSAO optimization follow-up on `2026-03-20`.

Scope of this round:

- focus only on `SSAO Evaluate`,
- replace per-sample RNG-based hemisphere generation with a fixed 32-tap hemisphere kernel,
- keep per-pixel variation by rotating the kernel once around the normal basis,
- rerun DX12 and Vulkan sweeps,
- add a narrower repeated-default perf run to judge the hot path with less mixed-case noise.

The intent was to reduce ALU cost inside the sample loop without changing the occlusion test itself.

## Code Changes

Updated file:

- `glTFRenderer/RendererDemo/Resources/Shaders/SSAO.hlsl`

Behavior changes:

- removed per-sample `rand()` use from `ComputeSSAO(...)`,
- removed per-sample `sampleHemisphereCosine(...)` generation,
- removed the extra per-sample direction normalization step,
- introduced a fixed 32-sample hemisphere kernel,
- introduced a single per-pixel interleaved-gradient-noise rotation around the view normal,
- kept the existing view-space projection and occlusion evaluation logic.

This is intentionally a lower-risk optimization than changing the occlusion model to a cheaper depth-only compare.

## Validation

Build command:

```powershell
powershell -ExecutionPolicy Bypass -File .\glTFRenderer\scripts\Build-RendererDemo-Verify.ps1
```

Build result:

- success
- binlog: `glTFRenderer/build_logs/rendererdemo_20260320_020627.binlog`

Relevant regression outputs from this round:

- DX12 sweep:
  - `.tmp/regression_capture/dx12/ssao_model_viewer_sweep_20260320_020732/suite_result.json`
- DX12 sweep rerun:
  - `.tmp/regression_capture/dx12/ssao_model_viewer_sweep_20260320_020840/suite_result.json`
- Vulkan sweep:
  - `.tmp/regression_capture/vulkan/ssao_model_viewer_sweep_20260320_020752/suite_result.json`
- Vulkan sweep rerun:
  - `.tmp/regression_capture/vulkan/ssao_model_viewer_sweep_20260320_020931/suite_result.json`
- DX12 repeated-default perf suite:
  - `.tmp/regression_capture/dx12/ssao_model_viewer_perf_20260320_021111/suite_result.json`
- Vulkan repeated-default perf suite:
  - `.tmp/regression_capture/vulkan/ssao_model_viewer_perf_20260320_021128/suite_result.json`

## Results

### Sweep observations

One sweep pair produced the following default-case result:

| Backend | SSAO Evaluate | SSAO Blur X | SSAO Blur Y | SSAO Total |
| --- | ---: | ---: | ---: | ---: |
| DX12 | 0.3503 ms | 0.0326 ms | 0.0312 ms | 0.4142 ms |
| Vulkan | 0.2510 ms | 0.0298 ms | 0.0289 ms | 0.3098 ms |

However, reruns were not stable enough to use mixed-case sweep data as the primary conclusion source:

- DX12 rerun default rose further to `0.6045 ms`,
- Vulkan rerun default rose to `0.3928 ms`,
- Vulkan rerun `sample_count = 8` also spiked unexpectedly.

So the mixed-case sweep was useful as a sanity check, but not as the final metric for this round.

### Repeated-default perf suite

The repeated-default perf suite was much more stable.

DX12:

| Case | SSAO Evaluate | Blur Total | SSAO Total |
| --- | ---: | ---: | ---: |
| `ssao_perf_01` | 0.2474 ms | 0.0590 ms | 0.3064 ms |
| `ssao_perf_02` | 0.2476 ms | 0.0588 ms | 0.3063 ms |
| `ssao_perf_03` | 0.2477 ms | 0.0589 ms | 0.3066 ms |

Vulkan:

| Case | SSAO Evaluate | Blur Total | SSAO Total |
| --- | ---: | ---: | ---: |
| `ssao_perf_01` | 0.2500 ms | 0.0593 ms | 0.3092 ms |
| `ssao_perf_02` | 0.2497 ms | 0.0594 ms | 0.3091 ms |
| `ssao_perf_03` | 0.2492 ms | 0.0596 ms | 0.3089 ms |

Interpretation:

- the repeated-default path is now very tightly clustered on both backends,
- the blur cost stayed effectively unchanged,
- the savings show up exactly where expected: in `SSAO Evaluate`.

### Comparison against the previous isolated default numbers

Reference default-case numbers before this round:

- DX12 isolated sweep default:
  - `.tmp/regression_capture/dx12/ssao_model_viewer_sweep_20260320_014816/suite_result.json`
  - total `0.3444 ms`, evaluate `0.2812 ms`
- Vulkan isolated sweep default:
  - `.tmp/regression_capture/vulkan/ssao_model_viewer_sweep_20260320_014833/suite_result.json`
  - total `0.3273 ms`, evaluate `0.2685 ms`

Current repeated-default result:

- DX12:
  - about `0.3064 ms` total, `0.2476 ms` evaluate
- Vulkan:
  - about `0.3091 ms` total, `0.2496 ms` evaluate

This suggests roughly:

- DX12: about `-0.0378 ms` total and `-0.0336 ms` evaluate,
- Vulkan: about `-0.0182 ms` total and `-0.0189 ms` evaluate.

This comparison is an inference, not a perfect apples-to-apples benchmark, because the reference came from a mixed-case sweep while the new measurement comes from repeated default cases. Even so, the direction is coherent and the unchanged blur cost supports the conclusion that the gain came from the evaluate sampling path.

## Conclusions

### What this round achieved

- `SSAO Evaluate` no longer spends ALU on per-sample RNG and cosine-hemisphere generation.
- The evaluate path now has a cheaper and more stable sampling basis.
- The best current default measurement is about `0.306-0.309 ms` total SSAO, with `Evaluate` around `0.25 ms`.

### What this implies

This optimization looks worthwhile and low-risk.

It also changes the optimization picture:

- the remaining evaluate cost is now even more dominated by depth fetches, sample reprojection, and sample-position reconstruction,
- if more savings are needed, the next meaningful step is likely to simplify the occlusion test itself rather than only reducing sample-generation math.

### Recommended next step

1. Test a cheaper depth-compare occlusion model that avoids reconstructing the full sample view-space position for every tap.
2. Keep using repeated-default perf suites when judging sub-`0.03 ms` SSAO changes.
3. Continue treating mixed-case DX12/Vulkan sweep spikes as a backend diagnostics issue, not as shader-only evidence.

## TODO

1. Investigate why mixed-case DX12 and Vulkan sweeps still show large `SSAO Evaluate` variance even after capture-window averaging and regression isolation fixes.
2. Check whether case transitions, resource lifetime, timestamp collection, or backend-specific scheduling are contaminating fine-grained SSAO pass timing.
3. If the backend gap remains large in future reruns, treat it as a possible backend implementation issue and keep recording it in follow-up notes.
