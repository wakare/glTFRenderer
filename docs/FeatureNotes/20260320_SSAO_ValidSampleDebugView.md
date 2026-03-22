# SSAO Valid Sample Debug View

## Summary

Added an SSAO debug output mode for inspecting `valid_sample_count / sample_count` from `ComputeSSAO(...)` directly through the existing tone-map SSAO debug view.

## Why

When SSAO appears to have little or no visual effect, one failure mode is that the evaluate pass rejects nearly all samples and `valid_sample_count` stays near zero. Before this change, the pipeline only exposed the final AO factor, which made that hypothesis hard to verify quickly in the running demo.

## What Changed

- `RendererSystemSSAO::SSAOGlobalParams` now includes `debug_output_mode`.
- `SSAO.hlsl` supports:
  - `0`: normal AO output
  - `1`: `valid_sample_count / sample_count`
- When the debug output mode is enabled:
  - invalid SSAO inputs return `0.0`
  - blur passes are skipped so the view reflects raw `ComputeSSAO(...)` output instead of a blurred result
- model-viewer snapshot serialization now preserves `debug_output_mode`

## How To Use

1. Open the `SSAO` panel.
2. Set `Debug Output` to `Valid Sample Ratio`.
3. Open the `Tone Map` panel.
4. Set `Debug View` to `SSAO`.

Interpretation:

- black: `valid_sample_count == 0`
- gray: some samples remain valid
- white: most or all samples remain valid

## Important Note

This debug output overrides the SSAO signal that lighting consumes. It is intended only for inspection, not for normal rendering comparisons.
