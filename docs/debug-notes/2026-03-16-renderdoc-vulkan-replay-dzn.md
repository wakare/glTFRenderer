# Debug Note - RenderDoc Vulkan replay forced to Mesa Dozen

- Date: 2026-03-16
- Scope: `RenderDoc local replay / Vulkan capture portability / Windows tooling`
- Commit: `(working tree, not committed yet)`
- Companion note:
  - `docs/debug-notes/2026-03-16-renderdoc-vulkan-replay-dzn_ZH.md`

## Symptom

Opening a Vulkan `.rdc` captured from `RendererDemo` failed in RenderDoc replay with an error stating that `bufferDeviceAddress` was available but `bufferDeviceAddressCaptureReplay` was not. The dialog reported that the capture was made on `NVIDIA GeForce RTX 4070 Ti SUPER, 591.74.0` but replay was attempted on `Microsoft Direct3D12 (NVIDIA GeForce RTX 4070 Ti SUPER), 26.0.99`.

## Reproduction

- Use a Windows machine that exposes both the native NVIDIA Vulkan ICD and the Microsoft D3D mapping-layer Vulkan ICD (`Dozen`).
- Capture a Vulkan frame from `RendererDemo` with RenderDoc.
- Open the resulting `.rdc` in QRenderDoc.
- In the failing setup, RenderDoc routes replay to `Microsoft Direct3D12 (NVIDIA GeForce RTX 4070 Ti SUPER)` instead of the native NVIDIA Vulkan device and rejects replay with the `bufferDeviceAddressCaptureReplay` error.
- The persisted override lives in `%APPDATA%\\qrenderdoc\\UI.config` under `DefaultReplayOptions`.

## Wrong Hypotheses Or Detours

### Detour 1: The application needed to enable `bufferDeviceAddressCaptureReplay` itself

- Why it looked plausible:
  The error text names the missing capability directly, and the Vulkan backend does globally enable buffer device address usage.
- Why it was not the final cause:
  The native NVIDIA Vulkan device on this machine reports `bufferDeviceAddressCaptureReplay = true`. The failure only happened because replay was redirected to the Dozen-backed device instead of the native NVIDIA Vulkan device.

### Detour 2: RenderDoc was passively choosing Dozen because of loader enumeration order alone

- Why it looked plausible:
  The Vulkan loader on this machine enumerates both `dzn_icd.x64.json` and the native NVIDIA ICD, so an ordering issue looked possible at first.
- Why it was not the final cause:
  RenderDoc's own log explicitly reported `Overriding GPU replay selection` to vendor `nVidia`, device `9989`, driver `"Mesa Dozen"`. This was an active persisted override, not a passive default choice.

### Detour 3: Removing the override once should have been enough

- Why it looked plausible:
  The first manual cleanup removed the override fields from disk, so it looked like the replay configuration had been reset.
- Why it was not the final cause:
  A lingering `renderdoccmd` process was still running. When it exited, it wrote the old in-memory settings back to `%APPDATA%\\qrenderdoc\\UI.config`, restoring the `Mesa Dozen` override.

## Final Root Cause

A persisted local RenderDoc replay override in `%APPDATA%\\qrenderdoc\\UI.config` forced `DefaultReplayOptions` to use `forceGPUDeviceID = 9989`, `forceGPUDriverName = "Mesa Dozen"`, and `forceGPUVendor = 6`. On this machine the Vulkan loader exposes both the native NVIDIA ICD and the Microsoft D3DMappingLayers Dozen ICD. RenderDoc obeyed the saved override and mapped replay to the Dozen-backed `Microsoft Direct3D12 (...)` physical device, which does not provide the `bufferDeviceAddressCaptureReplay` capability required by this capture. The failure was caused by a local replay-tool configuration mismatch, not by the application's Vulkan runtime behavior.

## Final Fix

- Fully exit all `qrenderdoc` and `renderdoccmd` processes before editing RenderDoc settings.
- Remove `forceGPUDeviceID`, `forceGPUDriverName`, and `forceGPUVendor` from `%APPDATA%\\qrenderdoc\\UI.config`.
- Relaunch RenderDoc so replay falls back to automatic GPU selection and picks the native NVIDIA Vulkan device.
- Keep the Dozen ICD installed if it is needed for other workflows. The fix is to clear the stale replay override, not to narrow valid application Vulkan behavior.
- Useful local evidence captured during triage:
  - `%LOCALAPPDATA%\\Temp\\RenderDoc\\RenderDoc_2026.03.16_12.23.33.log`
  - `build_logs/vulkaninfo_full.stdout.txt`
  - `build_logs/vulkaninfo_loader.stderr.txt`

## Validation

- Build result:
  Not applicable. No repository code changes were required to resolve the replay failure.
- Runtime validation:
  Before the fix, RenderDoc logs showed `Overriding GPU replay selection` to `"Mesa Dozen"` and replay on `Microsoft Direct3D12 (NVIDIA GeForce RTX 4070 Ti SUPER)`.
  After removing the persisted override with all RenderDoc processes fully exited, the user reopened the `.rdc` and confirmed that replay worked normally.
- User acceptance:
  The user confirmed that the capture could be replayed after the configuration fix.

## Reflection And Prevention

- When RenderDoc reports Vulkan replay on `Microsoft Direct3D12 (...)`, inspect local replay override settings before changing application Vulkan feature chains.
- Treat `%APPDATA%\\qrenderdoc\\UI.config` as part of the debugging surface area on Windows, especially `DefaultReplayOptions`.
- If a replay-setting edit appears to revert itself, check for a still-running `qrenderdoc` or `renderdoccmd` process before concluding the edit had no effect.
- For similar issues, verify the capture-time GPU, replay-time GPU, and Vulkan ICD list from loader logs before modifying renderer code.
