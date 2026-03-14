# Vulkan Shadow Ghosting In DemoAppModelViewer

Companion references:

- Chinese companion:
  - `docs/debug-notes/2026-03-11-vulkan-shadow-ghosting_ZH.md`

- Status: Accepted
- Date: 2026-03-11
- Scope: `RendererDemo -> DemoAppModelViewer`, Vulkan only, shadow rendering
- Commit: `3e02104`

## Symptom

When `RendererDemo` ran `DemoAppModelViewer` on Vulkan, shadowed areas showed obvious ghosting and residual bands. The same scene did not reproduce on DX12 under the same camera and light motion.

## Reproduction

- Run `RendererDemo`.
- Open `DemoAppModelViewer`.
- Select the Vulkan backend.
- Use a fixed camera/light state or a recorded snapshot so Vulkan and DX12 can be compared from the same view.
- Move the directional light or let the snapshot replay update the lighting state.

The bug was easiest to see on roof and wall shadow receivers where older shadow state appeared to leak into newer frames.

## Wrong Hypotheses Or Detours

### Detour 1: Depth SRV layout looked suspicious

- Why it looked plausible:
  Vulkan depth textures were being bound through the single-texture descriptor path, and the sampled image layout for depth SRV access was not specialized for depth formats.
- Why it was not the final cause:
  Fixing the Vulkan depth SRV layout was still correct, but it did not remove the cross-frame shadow ghosting. The visual artifact remained after that cleanup.

### Detour 2: Shadow bias or compare math seemed like the likely source

- Why it looked plausible:
  The artifact appeared in shadowed regions, so shadow compare code, PCF, and bias settings were natural first suspects.
- Why it was not the final cause:
  The visual pattern behaved like stale state leaking across frames, not like a stable per-pixel compare error. DX12 also did not reproduce the issue, which pointed away from shared lighting math and toward backend-specific resource lifetime handling.

## Final Root Cause

Vulkan root signatures only owned one pack of descriptor sets. Each new frame updated that same pack through `vkUpdateDescriptorSets`, while older command buffers could still be in flight. As a result, shadow-related bindings for an older frame could be overwritten by a newer frame before the GPU finished consuming them.

Shadow rendering exposed this first because it updates several descriptors every frame, including the view buffer used for shadow sampling, shadow map textures, and shadow metadata buffers. DX12 did not show the same bug because its binding path did not reuse descriptor state in the same way.

## Final Fix

- Made the command list carry an explicit frame-slot index.
- Stamped command lists with the current frame slot from resource management code.
- Changed Vulkan root signatures to allocate one descriptor-set pack per frame slot instead of a single shared pack.
- Changed Vulkan descriptor update and root-signature bind paths to pick descriptor sets by the current frame slot.
- Updated descriptor release code to free the full per-frame allocation set.
- Added `-snapshot=<path>` and `-disable-debug-ui` startup arguments to make backend A/B capture more repeatable during diagnosis.

Main touched files:

- `glTFRenderer/RHICore/Public/RHIInterface/IRHICommandList.h`
- `glTFRenderer/RHICore/Private/RHIInterface/IRHICommandList.cpp`
- `glTFRenderer/RendererCore/Private/ResourceManager.cpp`
- `glTFRenderer/RHICore/Public/RHIVKImpl/VKRootSignature.h`
- `glTFRenderer/RHICore/Private/RHIVKImpl/VKRootSignature.cpp`
- `glTFRenderer/RHICore/Private/RHIVKImpl/VKDescriptorUpdater.cpp`
- `glTFRenderer/RHICore/Private/RHIVKImpl/VulkanUtils.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoBase.h`

## Validation

- Isolated Debug x64 build completed successfully after the fix.
- Vulkan and DX12 were captured from the same replay path for visual comparison.
- The user confirmed that the previous Vulkan ghosting issue was fixed and accepted the result.

## Reflection And Prevention

- The strongest signal was "Vulkan only + DX12 clean + ghosting across frames". That combination should bias earlier investigation toward descriptor lifetime, frame buffering, barriers, and synchronization before tuning shadow math.
- The fix worked because frame slot became an explicit part of descriptor selection. Similar state should remain explicit in APIs instead of being inferred at individual call sites.
- A useful follow-up refactor is to treat per-frame descriptor ownership as a first-class Vulkan root-signature rule, not an optional implementation detail.
- A useful guardrail is a debug validation path that reports when frame-buffered rendering reuses a single descriptor pack across in-flight frames.
