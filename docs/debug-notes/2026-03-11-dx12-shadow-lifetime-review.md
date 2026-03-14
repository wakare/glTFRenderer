# DX12 Shadow Lifetime Review Follow-up

- Status: Accepted
- Date: 2026-03-11
- Scope: `RendererDemo -> RendererSystemLighting`, DX12 shadow resource lifetime review
- Commit: `(working tree, not committed yet)`

## Symptom

No user-visible DX12 artifact had been confirmed yet. The issue came from a follow-up review after the Vulkan shadow ghosting fix: DX12 did not share the same descriptor-set architecture, but the shadow path still contained two lifetime hazards that could produce stale cross-frame state under multi-frame-in-flight execution.

## Reproduction

- Start from the accepted Vulkan note in `docs/debug-notes/2026-03-11-vulkan-shadow-ghosting.md`.
- Review the DX12 shadow update path in `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`.
- Focus on per-frame updates for:
  - `ViewBuffer_shadow_*`
  - `g_shadowmap_infos`
  - DX12 descriptor cache reuse in `glTFRenderer/RHICore/Private/RHIDX12Impl/DX12DescriptorHeap.cpp`
- Compare those updates against DX12 frame-slot behavior and explicit resource release paths.

No deterministic visual repro was captured before the fix. This was a preventive review driven by the recent Vulkan diagnosis.

## Wrong Hypotheses Or Detours

### Detour 1: DX12 looked safe because it does not reuse Vulkan-style descriptor sets

- Why it looked plausible:
  DX12 binds GPU virtual addresses and descriptor-table handles directly into the command list. It does not call a Vulkan-style `vkUpdateDescriptorSets` path on one shared descriptor-set pack every frame.
- Why it was not the final cause:
  That only ruled out the exact Vulkan bug shape. It did not prove that all DX12 shadow inputs were frame-safe. Shared dynamic buffers and descriptor-cache lifetime still needed separate review.

### Detour 2: The DX12 descriptor cache looked like a capacity problem, not a correctness problem

- Why it looked plausible:
  The existing descriptor policy notes already discuss DX12 heap pressure and lack of fine-grained reclamation, so the cache first read as a memory-budget issue.
- Why it was not the final cause:
  The cache key used raw `ID3D12Resource*` values and only cleared entries on full heap release. That leaves a correctness hazard if a released resource address is reused later and the stale cache entry returns old descriptor handles for a new resource.

## Final Root Cause

Two separate DX12 lifetime hazards were present:

- `g_shadowmap_infos` was created as a single buffer, but it was updated every frame through the frame-buffered upload helper. In a multi-frame-in-flight configuration, a newer frame could overwrite shadow metadata that an older command list had not finished consuming yet.
- The DX12 descriptor cache stored descriptors by raw `ID3D12Resource*` and never invalidated entries when individual buffers or textures were released. That could allow stale descriptor reuse if a later resource instance received the same address.

Neither issue matched the Vulkan descriptor-pack overwrite exactly, but both live in the same class of "cross-frame or post-release GPU state remains implicitly shared."

## Final Fix

- Changed `g_shadowmap_infos` to use `CreateFrameBufferedBuffers(...)` so each frame slot owns its own shadow-info buffer.
- Removed the application-layer DX12-only lighting-buffer policy and made `g_lightInfos` / `LightInfoConstantBuffer` frame-buffered on all backends.
- Added `InvalidateResourceDescriptors(ID3D12Resource*)` to `DX12DescriptorHeap` and `DX12DescriptorManager`.
- Invalidated cached DX12 descriptors from `DX12Buffer::Release(...)` and `DX12Texture::Release(...)` before the underlying `ID3D12Resource` is released.

Main touched files:

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RHICore/Public/RHIDX12Impl/DX12DescriptorHeap.h`
- `glTFRenderer/RHICore/Private/RHIDX12Impl/DX12DescriptorHeap.cpp`
- `glTFRenderer/RHICore/Public/RHIDX12Impl/DX12DescriptorManager.h`
- `glTFRenderer/RHICore/Private/RHIDX12Impl/DX12DescriptorManager.cpp`
- `glTFRenderer/RHICore/Private/RHIDX12Impl/DX12Buffer.cpp`
- `glTFRenderer/RHICore/Private/RHIDX12Impl/DX12Texture.cpp`
- `glTFRenderer/RendererDemo/RendererModule/RendererModuleLighting.cpp`

## Validation

- Build result:
  Default `RendererDemo` validation build succeeded in local perf-loop session `session_20260311_144840`.
  Recorded result artifact name: `rendererdemo_20260311_144840.result.json`.
- Runtime validation:
  Default `MAILBOX + frosted_glass_b9_smoke` validation succeeded for both backends.
  Recorded DX12 run result artifact: `run_result.json` from local run `regression_dx12_20260311_145052`.
  Recorded Vulkan run result artifact: `run_result.json` from local run `regression_vulkan_20260311_145056`.
  Recorded comparison artifact: `comparison_20260311_145101.md`.
- User acceptance:
  This follow-up was requested as part of the current DX12 review and the fixes are now recorded.

## Reflection And Prevention

- The useful question was not "does DX12 have the same descriptor-set bug as Vulkan?" but "what state in DX12 is still shared across in-flight frames or past resource release?"
- Frame safety should be reviewed at the data-owner level, not only at the binding API level. Per-frame descriptors can still fail if the underlying buffer stays single-buffered.
- Application-layer resource policy should follow lifetime semantics, not backend identity. If one backend exposes a lifetime bug first, the preferred fix is framework-level ownership cleanup, not a permanent DX12/Vulkan split in `RendererDemo`.
- A useful guardrail is a debug validation path that reports:
  - per-frame-updated buffers that are still single-buffered
  - descriptor-cache hits on resources that have passed through release
