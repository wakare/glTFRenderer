# Debug Note - RenderDoc Vulkan replay 在 NVIDIA present layer 中崩溃

配套文档：

- 英文版本：
  - `docs/debug-notes/2026-03-23-renderdoc-vulkan-replay-nv-present.md`

- 状态：Accepted
- 日期：2026-03-23
- 范围：`RenderDoc / QRenderDoc / 受影响 NVIDIA 机器上的 Vulkan replay`
- 提交：`（仅文档记录，无仓库代码修改）`
- 配套文档：
  - `docs/debug-notes/2026-03-23-renderdoc-vulkan-replay-nv-present.md`

## Symptom

在部分 NVIDIA 机器上，Vulkan 截帧生成本身成功，但把生成出来的 `.rdc` 用 `qrenderdoc.exe` 打开时会立刻崩溃。即使目标应用已经成功完成 capture，RenderDoc 仍可能弹出 bug reporter 对话框。

minidump 指向的是 NVIDIA Vulkan 栈中的 replay 侧访问冲突：

- 进程：`qrenderdoc.exe`
- 异常：`0xc0000005`
- 崩溃模块：`nvoglv64.dll`
- 关键上层栈帧：`NvPresent64!NVP_Init_Vulkan`，然后才是 RenderDoc replay 相关栈帧

## Reproduction

- 在目标应用里用 Vulkan 成功抓取一帧。
- 在出问题的机器上用 `qrenderdoc.exe` 打开生成的 `.rdc`。
- 在失败环境中，`qrenderdoc.exe` 会在 replay 初始化阶段崩溃，capture 还没真正打开。

排查时观察到的诊断信号：

- RenderDoc session log 显示 Vulkan 注入、capture、写盘以及移交给 QRenderDoc 都成功完成。
- dump 显示崩溃发生在 replay 进程，而不是目标应用进程。

## Wrong Hypotheses Or Detours

### Detour 1: 目标应用的 Vulkan capture 路径把 capture 内容写坏了

- Why it looked plausible:
  bug reporter 对话框出现在 capture 工作流附近，第一反应很容易怀疑是我们自己的 Vulkan 截帧接入有问题。
- Why it was not the final cause:
  RenderDoc 日志明确显示 capture 从注入到写出 `.rdc` 都成功完成，而 dump 里的进程名是 `qrenderdoc.exe`，不是目标应用。

### Detour 2: 把 NVIDIA App 里的 overlay 开关关掉后，NVIDIA 的 Vulkan replay hook 就不会再生效

- Why it looked plausible:
  崩溃栈里有 `NvPresent64`，因此先去关闭可见的 NVIDIA overlay 设置是很自然的动作。
- Why it was not the final cause:
  即使 overlay toggle 关闭后，`vulkaninfo` 仍然能枚举到 `VK_LAYER_NV_present`，replay 也仍然会崩。

## Final Root Cause

真正的崩溃原因是受影响机器上的 NVIDIA Vulkan driver layer 环境，而不是被截取应用的帧内容。

更具体地说：

- `qrenderdoc.exe` 的 replay 过程中仍然加载了 `VK_LAYER_NV_present` 和 `VK_LAYER_NV_optimus`
- `VK_LAYER_NV_present` 对应的 NVIDIA manifest 最终解析到 `nvoglv64.dll`
- replay 初始化随后在 `nvoglv64!vkGetInstanceProcAddr` 内崩溃，栈上能看到 `NvPresent64!NVP_Init_Vulkan`

这说明失败点发生在 QRenderDoc 周围的 Vulkan replay 环境，而不是我们的应用 capture 侧逻辑。

## Final Fix

这个问题最先被接受的 workaround 是：在启动 QRenderDoc 时，通过 NVIDIA 自己支持的环境变量禁用 Vulkan present 和 Optimus layers：

```powershell
$env:DISABLE_LAYER_NV_PRESENT_1='1'
$env:DISABLE_LAYER_NV_OPTIMUS_1='1'
& 'C:\Program Files\RenderDoc\qrenderdoc.exe' 'path\to\capture.rdc'
```

之后，`RendererDemo` 的 RenderDoc UI 打开路径已经在 `glTFRenderer/RendererCore/Private/RendererInterface.cpp` 的 `RenderGraph::OpenRenderDocCaptureInReplayUI(...)` 中自动应用这两个环境变量，用于 UI 触发的 replay launch。

这个仓库侧 mitigation 的覆盖范围是：

- 覆盖共享 `DemoBase` 里的“capture 后自动打开 RenderDoc”以及“在应用内 UI 里打开最近一次 capture”操作
- 不覆盖从资源管理器、命令行或其他外部工具直接启动 `qrenderdoc.exe` 的路径；这些路径在遇到同样的 NVIDIA layer 环境时，仍然需要手动设置环境变量 workaround

排查过程中的支撑证据：

- 即使 NVIDIA App overlay toggle 已经关闭，`vulkaninfo` 仍然能枚举到 `VK_LAYER_NV_present`
- 当前生效的 NVIDIA 显示驱动包里的 Vulkan manifest 声明了 `VK_LAYER_NV_present`，其 `library_path` 最终解析到 `nvoglv64.dll`
- 同一个 manifest 还声明了 `DISABLE_LAYER_NV_PRESENT_1` 和 `DISABLE_LAYER_NV_OPTIMUS_1` 两个受支持的禁用开关

## Validation

- Build result:
  增加 UI replay-launch mitigation 后，`RendererDemo` 的 isolated verify build 已通过。
- Runtime validation:
  不设置 NVIDIA layer-disable 环境变量时，打开同一个 `.rdc` 仍会崩溃；设置 `DISABLE_LAYER_NV_PRESENT_1=1` 和 `DISABLE_LAYER_NV_OPTIMUS_1=1` 后，同一个 replay 可以正常打开。
- User acceptance:
  在加入仓库侧 mitigation 之前，用户已经确认手动设置环境变量 workaround 后，replay 可以正常工作。

## Reflection And Prevention

- 最应该优先抓住的信号其实是 dump 里的进程名。只要确认崩的是 `qrenderdoc.exe`，capture 侧应用代码的优先级就应该立刻下降。
- 对于跨机器表现不一致的 RenderDoc Vulkan 问题，先用 `vulkaninfo` 核对实际 Vulkan layer 环境，比先怀疑 RenderDoc 版本不一致更高效。
- UI 上能看到的 overlay 开关关闭，不代表对应的 Vulkan layer 已经从 loader 环境中消失。应当直接核对 active layer 列表，并在供应商提供支持时优先使用官方环境变量做最小化隔离验证。
