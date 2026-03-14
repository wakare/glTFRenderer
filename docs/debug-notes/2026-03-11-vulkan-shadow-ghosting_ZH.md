# DemoAppModelViewer 中的 Vulkan 阴影 Ghosting

配套文档：

- 英文版本：
  - `docs/debug-notes/2026-03-11-vulkan-shadow-ghosting.md`

- Status: Accepted
- Date: 2026-03-11
- Scope: `RendererDemo -> DemoAppModelViewer`，仅 Vulkan，阴影渲染
- Commit: `3e02104`

## Symptom

当 `RendererDemo` 在 Vulkan 后端运行 `DemoAppModelViewer` 时，阴影区域会出现明显的 ghosting 和残留条带。相同场景在同样的相机和光照运动下，在 DX12 上无法复现。

## Reproduction

- 启动 `RendererDemo`。
- 打开 `DemoAppModelViewer`。
- 选择 Vulkan backend。
- 使用固定的 camera / light 状态，或者加载录制好的 snapshot，以便让 Vulkan 和 DX12 从同一视角对比。
- 移动 directional light，或让 snapshot replay 更新 lighting state。

这个 bug 在屋顶和墙面的 shadow receiver 上最容易看到，因为旧帧的 shadow 状态看起来会泄漏到新帧里。

## Wrong Hypotheses Or Detours

### Detour 1: Depth SRV layout 看起来很可疑

- Why it looked plausible:
  Vulkan depth texture 是通过单纹理 descriptor 路径绑定的，而用于 depth SRV 访问的 sampled image layout 没有针对 depth format 做特殊处理。
- Why it was not the final cause:
  修复 Vulkan depth SRV layout 本身是正确的，但并没有消除跨帧 shadow ghosting。清理之后视觉问题仍然存在。

### Detour 2: Shadow bias 或 compare 数学看起来更像根因

- Why it looked plausible:
  伪影出现在 shadowed region，自然会优先怀疑 shadow compare、PCF 和 bias 设置。
- Why it was not the final cause:
  这个视觉模式更像跨帧陈旧状态泄漏，而不是稳定的逐像素 compare 错误。DX12 也没有复现同样问题，这说明问题更可能来自 backend-specific 的资源生命周期处理，而不是共享的 lighting 数学。

## Final Root Cause

Vulkan root signature 只拥有一份 descriptor-set pack。每一帧都会通过 `vkUpdateDescriptorSets` 更新这一份 pack，而旧 command buffer 可能仍在 flight 中。结果是：旧帧的 shadow 相关绑定，可能在 GPU 消费完成之前就被新帧覆盖。

之所以先在 shadow 渲染里暴露出来，是因为它每帧会更新多个 descriptor，包括用于 shadow sampling 的 view buffer、shadow map texture，以及 shadow metadata buffer。DX12 没有暴露同样 bug，是因为它的绑定路径不会以同样方式复用 descriptor 状态。

## Final Fix

- 让 command list 显式携带 frame-slot index。
- 从资源管理代码中，把当前 frame slot 标记到 command list 上。
- 把 Vulkan root signature 改成按 frame slot 分配 descriptor-set pack，而不是只保留一份共享 pack。
- 让 Vulkan descriptor update 和 root-signature bind 路径都按当前 frame slot 选择 descriptor set。
- 更新 descriptor release 代码，在释放时处理整套 per-frame 分配。
- 增加 `-snapshot=<path>` 和 `-disable-debug-ui` 启动参数，方便在排查时做后端 A/B 对比。

主要改动文件：

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

- 修复后，Isolated Debug x64 build 成功完成。
- Vulkan 和 DX12 都在同一条 replay 路径下抓取，用于视觉对比。
- 用户确认之前的 Vulkan ghosting 问题已经修复，并接受结果。

## Reflection And Prevention

- 最强的信号其实是“仅 Vulkan 复现 + DX12 干净 + 伪影跨帧残留”。看到这一组合时，应更早把调查重心放到 descriptor 生命周期、frame buffering、barrier 和同步，而不是先调 shadow 数学。
- 这次修复有效，是因为 frame slot 成为了 descriptor 选择的显式输入。类似状态应继续在 API 里保持显式，而不是散落在单个调用点里隐式推断。
- 一个值得跟进的重构方向是：把 per-frame descriptor ownership 提升为 Vulkan root-signature 的一等规则，而不是可选实现细节。
- 一个有价值的 guardrail 是增加 debug validation 路径，当 frame-buffered 渲染仍在跨 in-flight frame 复用同一份 descriptor pack 时直接报警。
