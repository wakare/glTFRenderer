# DX12 阴影生命周期复查补充记录

配套文档：

- 英文版本：
  - `docs/debug-notes/2026-03-11-dx12-shadow-lifetime-review.md`

- Status: Accepted
- Date: 2026-03-11
- Scope: `RendererDemo -> RendererSystemLighting`，DX12 阴影资源生命周期复查
- Commit: `(working tree, not committed yet)`

## Symptom

当时还没有确认到用户可见的 DX12 伪影。这个问题来自 Vulkan 阴影 ghosting 修复后的后续复查：虽然 DX12 不共享同一种 descriptor-set 架构，但阴影路径里仍有两个生命周期风险点，在 multi-frame-in-flight 条件下可能产生陈旧的跨帧状态。

## Reproduction

- 以已接受的 Vulkan 记录 `docs/debug-notes/2026-03-11-vulkan-shadow-ghosting.md` 为起点。
- 审查 `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp` 中的 DX12 阴影更新路径。
- 重点关注这些对象的逐帧更新：
  - `ViewBuffer_shadow_*`
  - `g_shadowmap_infos`
  - `glTFRenderer/RHICore/Private/RHIDX12Impl/DX12DescriptorHeap.cpp` 中的 DX12 descriptor cache 复用
- 把这些更新方式与 DX12 frame-slot 行为及显式资源释放路径做比对。

在修复前没有拿到稳定的视觉复现；这是一次由 Vulkan 诊断引发的预防性复查。

## Wrong Hypotheses Or Detours

### Detour 1: DX12 看起来是安全的，因为它不复用 Vulkan 风格 descriptor set

- Why it looked plausible:
  DX12 直接把 GPU virtual address 和 descriptor-table handle 绑定进 command list，不会像 Vulkan 那样每帧对同一份 descriptor-set pack 调用 `vkUpdateDescriptorSets`。
- Why it was not the final cause:
  这只能排除与 Vulkan 完全同形态的 bug，并不能证明所有 DX12 阴影输入都具有 frame safety。共享 dynamic buffer 和 descriptor-cache 的生命周期仍然需要单独审查。

### Detour 2: DX12 descriptor cache 一开始更像容量问题，而不是正确性问题

- Why it looked plausible:
  现有 descriptor policy 记录已经提到 DX12 heap 压力和缺乏细粒度回收，因此 cache 最初看上去像一个内存预算问题。
- Why it was not the final cause:
  这个 cache 的 key 直接使用裸 `ID3D12Resource*`，并且只有在整块 heap 释放时才清空条目。如果某个已释放资源的地址后来被复用，旧 cache 条目就可能把陈旧 descriptor handle 返回给新资源实例。

## Final Root Cause

DX12 中实际存在两个独立的生命周期风险：

- `g_shadowmap_infos` 被创建为单 buffer，但每帧都通过 frame-buffered upload helper 更新。在 multi-frame-in-flight 情况下，较新的帧可能会覆盖较旧 command list 还未消费完的阴影元数据。
- DX12 descriptor cache 用裸 `ID3D12Resource*` 存 descriptor，并且在单个 buffer / texture 释放时不会失效对应条目。如果之后新资源实例获得了同一个地址，就可能复用到陈旧 descriptor。

这两个问题虽然不等于 Vulkan 的 descriptor-pack 覆盖，但都属于同一类问题：跨帧或资源释放后的 GPU 状态仍然被隐式共享。

## Final Fix

- 把 `g_shadowmap_infos` 改成通过 `CreateFrameBufferedBuffers(...)` 创建，使每个 frame slot 拥有自己的 shadow-info buffer。
- 移除了应用层只针对 DX12 的 lighting-buffer 策略，把 `g_lightInfos` / `LightInfoConstantBuffer` 在所有 backend 上都改成 frame-buffered。
- 在 `DX12DescriptorHeap` 和 `DX12DescriptorManager` 中新增 `InvalidateResourceDescriptors(ID3D12Resource*)`。
- 在 `DX12Buffer::Release(...)` 和 `DX12Texture::Release(...)` 中，在底层 `ID3D12Resource` 真正释放前先失效对应的缓存 descriptor。

主要改动文件：

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
  默认 `RendererDemo` 验证构建已在本地 perf-loop 会话 `session_20260311_144840` 中通过。
  记录的构建结果工件名：`rendererdemo_20260311_144840.result.json`。
- Runtime validation:
  默认 `MAILBOX + frosted_glass_b9_smoke` 验证已在两个 backend 上通过。
  记录的 DX12 运行工件：本地运行 `regression_dx12_20260311_145052` 产出的 `run_result.json`。
  记录的 Vulkan 运行工件：本地运行 `regression_vulkan_20260311_145056` 产出的 `run_result.json`。
  记录的 comparison 工件：`comparison_20260311_145101.md`。
- User acceptance:
  这次 follow-up 是当前 DX12 复查任务的一部分，修复现已记录完成。

## Reflection And Prevention

- 更关键的问题不是“DX12 是否有和 Vulkan 相同的 descriptor-set bug”，而是“DX12 中还有哪些状态在 in-flight frame 或资源释放后被共享”。
- Frame safety 应从数据所有者层面审查，而不只是从 binding API 层面审查。即使 descriptor 是逐帧的，只要底层 buffer 仍是单缓冲，也可能出问题。
- 应用层资源策略应由生命周期语义决定，而不是由 backend 身份决定。如果某个 backend 先暴露出生命周期 bug，优先修复方式应是框架级 ownership 清理，而不是在 `RendererDemo` 里长期保留 DX12 / Vulkan 分叉。
- 一个有价值的 guardrail 是增加 debug validation 路径，直接报告：
  - 被逐帧更新但仍是 single-buffered 的 buffer
  - 命中过已释放资源的 descriptor-cache 条目
