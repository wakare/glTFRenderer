# Debug Note - BasePass metallic was never written into the GBuffer

- Status: Accepted
- Date: 2026-03-22
- Scope: `RendererDemo -> BasePass / material import / GBuffer metallic-roughness packing`
- Commit: `(working tree, not committed yet)`
- Companion note:
  - `docs/debug-notes/2026-03-22-basepass-metallic-gbuffer_ZH.md`

## Symptom

In `RendererDemo`, `SceneLighting.hlsl` reads `albedoTex.w` as metallic, but the observed BasePass output looked as if the `w` channel never actually contained metallic data from the material.

## Reproduction

- Use the maintained `RendererDemo` BasePass path:
  - `glTFRenderer/RendererDemo/Resources/Shaders/ModelRenderingShader.hlsl`
  - `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
- Inspect the BasePass write path and the lighting read path together.
- Compare the intended packing:
  - `albedoTex.w -> metallic`
  - `normalTex.w -> roughness`
- Then trace the material source through:
  - `glTFRenderer/RendererScene/Private/RendererSceneGraph.cpp`
  - `glTFRenderer/RendererDemo/RendererModule/RendererModuleMaterial.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleMaterial.hlsl`

## Wrong Hypotheses Or Detours

### Detour 1: The bug was only in the lighting read side

- Why it looked plausible:
  `SceneLighting.hlsl` explicitly reads `float metallic = albedo_buffer_data.w;`, so it was natural to suspect that the compute lighting pass was binding or sampling the wrong texture channel.
- Why it was not the final cause:
  The lighting pass was reading exactly what BasePass had written. The main failure was earlier: BasePass never packed metallic into `SV_TARGET0.w`, and roughness was not packed into `SV_TARGET1.w` either.

### Detour 2: The bug was only a missing metallic-roughness texture sample

- Why it looked plausible:
  `SampleMetallicRoughnessTexture(...)` returned only texture `.bg`, which already looked suspicious for glTF materials that also carry `metallicFactor` and `roughnessFactor`.
- Why it was not the final cause:
  That was only half of the bug. Even if the sample function had returned the right value, `ModelRenderingShader.hlsl` still wrote plain albedo into `output.color` and left `output.normal.w` at `0.0`.

## Final Root Cause

Two separate breaks in the same data path made the BasePass metallic channel appear unreadable:

1. `glTFRenderer/RendererDemo/Resources/Shaders/ModelRenderingShader.hlsl` did not write metallic or roughness into the GBuffer. `output.color` was assigned directly from `SampleAlbedoTexture(...)`, so `SV_TARGET0.w` contained base-color alpha instead of metallic, while `SV_TARGET1.w` stayed `0.0` instead of roughness.
2. The `RendererDemo` material path modeled base color and metallic-roughness as either a texture or a factor, instead of preserving the glTF rule that texture data and factors coexist and must be multiplied together. When a texture existed, `baseColorFactor`, `metallicFactor`, and `roughnessFactor` were effectively dropped before the GPU material buffer was built.

## Final Fix

- Added a combined texture-plus-factor constructor to `MaterialParameter` so the `RendererScene` import layer can preserve both values together.
- Updated `glTFRenderer/RendererScene/Private/RendererSceneGraph.cpp` to pass:
  - `baseColorTexture * baseColorFactor`
  - `metallicRoughnessTexture.bg * float2(roughnessFactor, metallicFactor)` in the stored factor layout
- Updated `glTFRenderer/RendererDemo/RendererModule/RendererModuleMaterial.cpp` to always upload the factor fields even when a texture handle is present.
- Updated `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleMaterial.hlsl` so:
  - albedo texture samples are multiplied by `info.albedo`
  - metallic-roughness samples are multiplied by `info.metallicAndRoughness.bg`
- Updated `glTFRenderer/RendererDemo/Resources/Shaders/ModelRenderingShader.hlsl` so BasePass now writes:
  - `SV_TARGET0.xyz = base color`
  - `SV_TARGET0.w = metallic`
  - `SV_TARGET1.xyz = encoded world normal`
  - `SV_TARGET1.w = roughness`

## Validation

- Build result:
  `RendererDemo` verification build succeeded with `0` errors and `43` warnings.
  Logs:
  - `build_logs/rendererdemo_20260322_191051.msbuild.log`
  - `build_logs/rendererdemo_20260322_191051.stdout.log`
  - `build_logs/rendererdemo_20260322_191051.stderr.log`
  - `build_logs/rendererdemo_20260322_191051.binlog`
- Runtime validation:
  No new runtime capture or RenderDoc verification was run in this turn. Validation here is limited to source-path inspection plus a successful verification build.
- User acceptance:
  After the diagnosis and code fix were reviewed, the user asked for this change set to be committed separately.

## Reflection And Prevention

- What signal should have been prioritized earlier:
  When a downstream pass reads a packed GBuffer channel correctly but the value still looks wrong, verify the BasePass write site before spending time on the consumer side.
- What guardrail or refactor would reduce recurrence:
  The material abstraction used by `RendererDemo` should not collapse glTF PBR inputs into a texture-or-factor choice when the format requires texture-times-factor semantics.
- What to check first if a similar symptom appears again:
  First inspect the exact GBuffer write statements and the CPU-to-GPU material packing for the affected channel, then confirm whether the sampled value is the raw texture value, the factor, or the correct product of both.
