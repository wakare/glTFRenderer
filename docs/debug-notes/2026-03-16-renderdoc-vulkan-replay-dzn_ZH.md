# Debug Note - RenderDoc Vulkan replay 被固定到 Mesa Dozen

- Date: 2026-03-16
- Scope: `RenderDoc 本地 replay / Vulkan capture 可移植性 / Windows 工具链`
- Commit: `(working tree, not committed yet)`
- 配套文档：
  - `docs/debug-notes/2026-03-16-renderdoc-vulkan-replay-dzn.md`

## Symptom

打开从 `RendererDemo` 导出的 Vulkan `.rdc` 时，RenderDoc replay 失败，并提示 `bufferDeviceAddress` 可用，但 `bufferDeviceAddressCaptureReplay` 不可用。错误弹窗同时显示 capture 设备是 `NVIDIA GeForce RTX 4070 Ti SUPER, 591.74.0`，而 replay 设备却变成了 `Microsoft Direct3D12 (NVIDIA GeForce RTX 4070 Ti SUPER), 26.0.99`。

## Reproduction

- 在一台同时暴露原生 NVIDIA Vulkan ICD 和 Microsoft D3D mapping-layer Vulkan ICD (`Dozen`) 的 Windows 机器上复现。
- 用 RenderDoc 从 `RendererDemo` 抓取 Vulkan 帧。
- 在 QRenderDoc 里打开生成的 `.rdc`。
- 在出错配置下，RenderDoc 会把 replay 路由到 `Microsoft Direct3D12 (NVIDIA GeForce RTX 4070 Ti SUPER)`，而不是原生 NVIDIA Vulkan 设备，并因此报出 `bufferDeviceAddressCaptureReplay` 错误。
- 持久化的覆盖项位于 `%APPDATA%\\qrenderdoc\\UI.config` 的 `DefaultReplayOptions` 下。

## Wrong Hypotheses Or Detours

### Detour 1: 应用侧需要自己显式启用 `bufferDeviceAddressCaptureReplay`

- Why it looked plausible:
  报错文本直接点名了缺失能力，而当前 Vulkan 后端也确实全局启用了 buffer device address，用这个方向起步很自然。
- Why it was not the final cause:
  这台机器上的原生 NVIDIA Vulkan 设备实际支持 `bufferDeviceAddressCaptureReplay = true`。真正出错的原因不是应用少开了 feature，而是 replay 被导向了 Dozen 后端而不是原生 NVIDIA Vulkan 设备。

### Detour 2: RenderDoc 只是因为 loader 枚举顺序而被动选中了 Dozen

- Why it looked plausible:
  本机 Vulkan loader 同时枚举到了 `dzn_icd.x64.json` 和原生 NVIDIA ICD，一开始看起来像是驱动顺序问题。
- Why it was not the final cause:
  RenderDoc 自己的日志明确写了 `Overriding GPU replay selection`，目标是 vendor `nVidia`、device `9989`、driver `"Mesa Dozen"`。这不是默认枚举顺序导致的被动选择，而是持久化 override 在主动生效。

### Detour 3: 第一次删掉 override 之后应该就已经修好

- Why it looked plausible:
  第一次手动清理时，磁盘上的配置字段已经被删掉，看起来 replay 配置已经恢复默认。
- Why it was not the final cause:
  当时仍有一个残留的 `renderdoccmd` 进程在运行。它退出时把内存里的旧设置重新写回 `%APPDATA%\\qrenderdoc\\UI.config`，于是 `Mesa Dozen` override 又被恢复了。

## Final Root Cause

本地 RenderDoc replay 配置 `%APPDATA%\\qrenderdoc\\UI.config` 中残留了一个持久化 override，把 `DefaultReplayOptions` 固定到了 `forceGPUDeviceID = 9989`、`forceGPUDriverName = "Mesa Dozen"`、`forceGPUVendor = 6`。而这台机器的 Vulkan loader 同时暴露了原生 NVIDIA ICD 和 Microsoft D3DMappingLayers 的 Dozen ICD。RenderDoc 按照这份保存下来的 override，把 replay 映射到了 Dozen 支持的 `Microsoft Direct3D12 (...)` 物理设备上；该设备不具备这份 capture 所需的 `bufferDeviceAddressCaptureReplay` 能力，最终导致 replay 失败。根因是本地 replay 工具配置和 capture 设备不匹配，不是应用 Vulkan 运行时逻辑出错。

## Final Fix

- 编辑 RenderDoc 配置前，先完全退出所有 `qrenderdoc` 和 `renderdoccmd` 进程。
- 从 `%APPDATA%\\qrenderdoc\\UI.config` 中移除 `forceGPUDeviceID`、`forceGPUDriverName` 和 `forceGPUVendor`。
- 重启 RenderDoc，让 replay 回退到自动选卡，并重新选择原生 NVIDIA Vulkan 设备。
- 不需要为了这次问题卸载 Dozen ICD；如果其他工作流需要它，可以继续保留。真正需要修的是过期的 replay override，而不是缩窄应用合法的 Vulkan 行为。
- 这次定位过程中保留的本地证据：
  - `%LOCALAPPDATA%\\Temp\\RenderDoc\\RenderDoc_2026.03.16_12.23.33.log`
  - `build_logs/vulkaninfo_full.stdout.txt`
  - `build_logs/vulkaninfo_loader.stderr.txt`

## Validation

- Build result:
  不适用。这次修复 replay 失败不需要改仓库代码，也不需要重新构建工程。
- Runtime validation:
  修复前，RenderDoc 日志明确显示 `Overriding GPU replay selection` 到 `"Mesa Dozen"`，并把 replay 跑在 `Microsoft Direct3D12 (NVIDIA GeForce RTX 4070 Ti SUPER)` 上。
  在完全退出全部 RenderDoc 进程后移除持久化 override，用户重新打开 `.rdc` 并确认 replay 恢复正常。
- User acceptance:
  用户确认配置修复后，capture 已经可以正常 replay。

## Reflection And Prevention

- 如果 RenderDoc 报告 Vulkan replay 跑在 `Microsoft Direct3D12 (...)` 上，应该先检查本地 replay override，而不是立刻去改应用的 Vulkan feature chain。
- 在 Windows 上，`%APPDATA%\\qrenderdoc\\UI.config` 应该被视为调试面的一部分，尤其要留意 `DefaultReplayOptions`。
- 如果某次 replay 配置修改看起来“自己又变回去了”，先检查是否还有 `qrenderdoc` 或 `renderdoccmd` 进程存活，不要过早判断修改无效。
- 下次遇到类似问题，优先核对三件事：capture-time GPU、replay-time GPU、以及 loader 日志里枚举到的 Vulkan ICD 列表，再决定是否需要调整渲染器代码。
