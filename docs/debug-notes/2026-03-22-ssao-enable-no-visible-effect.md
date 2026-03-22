# 2026-03-22 SSAO Enable No Visible Effect

- Scope: `RendererDemo` model viewer SSAO evaluate path, lighting integration, debug visualization
- Companion note:
  - [2026-03-22-ssao-enable-no-visible-effect_ZH.md](D:/Work/DevSpace/glTFRenderer/glTFRenderer/docs/debug-notes/2026-03-22-ssao-enable-no-visible-effect_ZH.md)

## Symptom

In the model viewer, toggling `SSAO -> Enable SSAO` produced little or no visible difference in the final frame.

Initial inspection suggested the UI toggle and parameter upload path were functional, but the rendered result remained effectively unchanged.

## Reproduction

- App: `DemoAppModelViewer`
- Scene: default Sponza setup
- Backend: reproduced during Vulkan validation, but the issue was in shared SSAO logic rather than backend-only render-graph wiring
- Steps:
  - open the `SSAO` panel
  - toggle `Enable SSAO`
  - compare the final frame

Useful debug path that exposed the real fault:

- `SSAO -> Debug Output = Valid Sample Ratio`
- `Tone Map -> Debug View = SSAO`

## Wrong Hypotheses Or Detours

### Detour 1

The first suspicion was that the enable checkbox was not reaching the shader or the lighting pass was still bound to a stale SSAO resource.

That turned out to be false. The UI updated `m_global_params.enabled`, the shader respected `enabled == 0u`, and the lighting pass sampled `m_ssao->GetOutputs().output`.

### Detour 2

The next suspicion was that SSAO was active but visually weak because it only modulated diffuse environment lighting.

That explanation was partially true for why the effect can be subtle in some lighting conditions, but it was not the main bug here. The debug view showed the evaluate path was rejecting almost all samples, so the AO signal itself was mostly collapsing to the neutral value.

### Detour 3

The first view-space SSAO optimization replaced world-space reconstruction and projection with a simplified analytical path based on cached projection scalars.

That optimization reduced cost, but it also introduced a more fragile reconstruction and reprojection path. The new `Valid Sample Ratio` debug mode made this visible immediately because the frame became almost entirely black.

## Final Root Cause

`ComputeSSAO(...)` was using a simplified analytical view-space reconstruction and reprojection path that did not reliably match the engine's real camera projection conventions.

As a result, most projected SSAO taps failed the validation chain or reconstructed to inconsistent sample positions, which kept `valid_sample_count` near zero across most pixels. When that happened, the evaluate pass returned the neutral AO value, so toggling SSAO had little visible effect in the final lighting result.

## Final Fix

- Replaced the analytical view-space reconstruction in `glTFRenderer/RendererDemo/Resources/Shaders/SSAO.hlsl` with matrix-based reconstruction through `inverse_projection_matrix`
- Replaced analytical reprojection with matrix-based reprojection through `projection_matrix`
- Reconstructed fetched sample positions from the actual sampled texel instead of the intermediate floating-point UV
- Added `Valid Sample Ratio` debug output so `valid_sample_count / sample_count` can be visualized directly
- Routed that debug output through the existing tone-map SSAO debug view
- Preserved SSAO debug state in model-viewer snapshot serialization
- Extended regression plumbing so SSAO parameter sweeps and perf runs are scriptable

## Validation

- Build result:
  - `BuildSucceeded`
  - [rendererdemo_20260320_074943.binlog](D:/Work/DevSpace/glTFRenderer/glTFRenderer/build_logs/rendererdemo_20260320_074943.binlog)
- Runtime validation:
  - `Valid Sample Ratio` debug view changed from almost fully black to a normal non-zero distribution after the projection fix
  - final rendering showed restored visible SSAO response after returning `Debug Output` to normal `AO`
- User acceptance:
  - user confirmed that the repaired path now shows visible SSAO effect

## Reflection And Prevention

- What signal should have been prioritized earlier:
  - the first reliable debugging signal was not the final lit frame but the sample-validation distribution inside `ComputeSSAO(...)`
- What guardrail or refactor would reduce recurrence:
  - prefer shared matrix-based reconstruction helpers over hand-derived projection shortcuts unless they are validated per backend
  - keep SSAO internal debug outputs available so evaluate-stage failures are observable without RenderDoc
  - if future DX12 and Vulkan behavior diverges materially, record it as a backend investigation TODO instead of assuming shader math is backend-neutral
- What to check first if a similar symptom appears again:
  - whether lighting is sampling the current SSAO output resource
  - whether `valid_sample_count` is non-zero in the evaluate pass
  - whether the camera projection and depth conventions used by SSAO match the actual runtime matrices
