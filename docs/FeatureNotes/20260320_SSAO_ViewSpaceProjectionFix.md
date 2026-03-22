# SSAO View-Space Projection Fix

## Summary

During SSAO debugging, the `Valid Sample Ratio` visualization showed an almost fully black frame, which indicated that `ComputeSSAO(...)` was rejecting nearly all samples before occlusion accumulation.

This round replaced the simplified analytical view-space reconstruction and reprojection path with matrix-based reconstruction using `inverse_projection_matrix` and reprojection using `projection_matrix`.

## Symptom

- `SSAO -> Debug Output = Valid Sample Ratio`
- `Tone Map -> Debug View = SSAO`

The frame appeared almost entirely black, implying `valid_sample_count` stayed near zero across most pixels.

## Likely Cause

The previous optimization path relied on simplified projection math derived from cached projection scalars. That path was more fragile than the original matrix-based reconstruction and could diverge from the runtime camera projection conventions or backend-specific clip-space behavior.

Even when the algebra is theoretically valid, a mismatch in sign conventions, depth interpretation, or reprojection coordinates is enough to collapse the SSAO sample validation path.

## Change

- `GetViewPositionFromUV(...)` now reconstructs view position through `inverse_projection_matrix`
- `ProjectViewPositionToUV(...)` now reprojects through `projection_matrix`
- evaluate sampling reconstructs the fetched sample position from the actual sampled texel instead of the intermediate floating-point UV

## Expected Outcome

This should make the SSAO evaluate path follow the exact same projection conventions as the camera buffer and avoid backend-sensitive drift introduced by the analytical approximation.

## Validation

- Verification build succeeded
- Binlog: `build_logs/rendererdemo_20260320_074943.binlog`

## Next Check

Re-run:

1. `SSAO -> Debug Output = Valid Sample Ratio`
2. `Tone Map -> Debug View = SSAO`

If the image is no longer near-black, the failure was in the view-space reconstruction or projection path.

If it is still near-black, the next suspects are:

- `along_ray` / `along_normal` rejection thresholds
- sample direction basis construction
- normal-space convention mismatch
