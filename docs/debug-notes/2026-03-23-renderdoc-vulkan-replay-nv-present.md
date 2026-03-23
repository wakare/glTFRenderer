# Debug Note - RenderDoc Vulkan replay crash in NVIDIA present layer

Companion references:

- Chinese companion:
  - `docs/debug-notes/2026-03-23-renderdoc-vulkan-replay-nv-present_ZH.md`

- Status: Accepted
- Date: 2026-03-23
- Scope: `RenderDoc / QRenderDoc / Vulkan replay on affected NVIDIA machines`
- Commit: `(documentation-only note; no repo code change)`
- Companion note:
  - `docs/debug-notes/2026-03-23-renderdoc-vulkan-replay-nv-present_ZH.md`

## Symptom

On some NVIDIA machines, Vulkan capture generation succeeded, but opening the resulting `.rdc` in `qrenderdoc.exe` crashed immediately. RenderDoc could show the bug reporter dialog even though the target application had already finished capture successfully.

The minidump pointed at a replay-side access violation in NVIDIA's Vulkan stack:

- process: `qrenderdoc.exe`
- exception: `0xc0000005`
- faulting module: `nvoglv64.dll`
- important upper frames: `NvPresent64!NVP_Init_Vulkan`, then RenderDoc replay frames

## Reproduction

- Use Vulkan to capture a frame successfully from the target application.
- Open the generated `.rdc` in `qrenderdoc.exe` on the affected machine.
- On the failing setup, `qrenderdoc.exe` crashes during replay initialization before the capture opens.

Observed diagnostic signals during investigation:

- The RenderDoc session log showed successful Vulkan injection, capture, disk write, and handoff to QRenderDoc.
- The dump showed the crash in replay, not in the target process.

## Wrong Hypotheses Or Detours

### Detour 1: The target application's Vulkan capture path was corrupting the capture

- Why it looked plausible:
  The bug reporter dialog appeared near the capture workflow, so it was natural to suspect our Vulkan capture integration first.
- Why it was not the final cause:
  The RenderDoc log showed capture success end-to-end, including writing the `.rdc` to disk. The dump process was `qrenderdoc.exe`, not the target application.

### Detour 2: Disabling the NVIDIA App overlay toggle removed the NVIDIA Vulkan replay hook

- Why it looked plausible:
  The crash stack contained `NvPresent64`, so disabling the visible NVIDIA overlay settings looked like the direct fix.
- Why it was not the final cause:
  Even after the overlay toggle was turned off, `vulkaninfo` still enumerated `VK_LAYER_NV_present`, and replay still crashed.

## Final Root Cause

The replay crash was caused by NVIDIA's Vulkan driver layer environment on the affected machine, not by the captured application's frame contents.

More specifically:

- `qrenderdoc.exe` replay still loaded `VK_LAYER_NV_present` and `VK_LAYER_NV_optimus`.
- The NVIDIA manifest for `VK_LAYER_NV_present` resolved to `nvoglv64.dll`.
- Replay initialization then crashed inside `nvoglv64!vkGetInstanceProcAddr`, with `NvPresent64!NVP_Init_Vulkan` on the stack.

This means the failure happened in the Vulkan replay environment around QRenderDoc, not in our application's capture-side logic.

## Final Fix

The first accepted workaround was to launch QRenderDoc with NVIDIA's Vulkan present and Optimus layers disabled through their supported environment variables:

```powershell
$env:DISABLE_LAYER_NV_PRESENT_1='1'
$env:DISABLE_LAYER_NV_OPTIMUS_1='1'
& 'C:\Program Files\RenderDoc\qrenderdoc.exe' 'path\to\capture.rdc'
```

Later, `RendererDemo`'s RenderDoc UI launch path was updated to apply the same two environment variables automatically for UI-triggered replay launch in `glTFRenderer/RendererCore/Private/RendererInterface.cpp`, inside `RenderGraph::OpenRenderDocCaptureInReplayUI(...)`.

Scope of the repository-side mitigation:

- It covers the shared `DemoBase` operations that auto-open RenderDoc after capture or open the last capture from the in-app UI.
- It does not change standalone `qrenderdoc.exe` launches from Explorer, command line shells, or other external tools. Those paths still need the manual environment-variable workaround when the affected NVIDIA layer stack is present.

Supporting evidence gathered during investigation:

- `vulkaninfo` still enumerated `VK_LAYER_NV_present` after the NVIDIA App overlay toggle had been disabled.
- The NVIDIA Vulkan manifest for the active display driver package declared `VK_LAYER_NV_present`, and its `library_path` resolved to `nvoglv64.dll`.
- The same manifest exposed `DISABLE_LAYER_NV_PRESENT_1` and `DISABLE_LAYER_NV_OPTIMUS_1` as supported disable switches.

## Validation

- Build result:
  `RendererDemo` isolated verify build succeeded after the UI replay-launch mitigation was added.
- Runtime validation:
  Replaying the same `.rdc` still crashed without the NVIDIA layer-disable environment variables, but replay succeeded when `DISABLE_LAYER_NV_PRESENT_1=1` and `DISABLE_LAYER_NV_OPTIMUS_1=1` were set.
- User acceptance:
  The user confirmed that replay opened normally with the environment-variable workaround before the repository-side mitigation was added.

## Reflection And Prevention

- The highest-value early signal was the process name in the dump. Once the crash was known to be inside `qrenderdoc.exe`, capture-side application code should have been deprioritized.
- For RenderDoc Vulkan failures that differ across machines, check the actual Vulkan layer environment with `vulkaninfo` before assuming a RenderDoc-version mismatch.
- A visible overlay toggle is not enough evidence that the associated Vulkan layer is gone. Confirm the active layer list and, when available, prefer vendor-supported disable environment variables for minimal repro isolation.
