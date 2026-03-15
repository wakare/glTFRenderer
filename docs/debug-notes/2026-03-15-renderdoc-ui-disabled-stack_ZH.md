# Debug Note - RenderDoc UI Disabled Stack 崩溃

- Date: 2026-03-15
- Scope: `RendererDemo -> DemoBase -> Runtime / Diagnostics > RenderDoc`
- Commit: `(working tree, not committed yet)`
- 配套文档：
  - `docs/debug-notes/2026-03-15-renderdoc-ui-disabled-stack.md`

## Symptom

给 `DemoBase` 增加共享的 RenderDoc UI 之后，点击 `Capture Current Frame` 可能会让程序在 `ImGui::EndDisabled()` 里崩溃。实际观察到的调用栈指向 `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp` 里的 RenderDoc UI 分支，而不是 RenderDoc API 本身。

## Reproduction

- 启动带 debug UI 的 `RendererDemo`。
- 打开 `Runtime / Diagnostics > RenderDoc`。
- 点击 `Capture Current Frame`。
- 在有问题的版本里，程序会在 `imgui.cpp` 的 `ImGui::EndDisabled()` 处断下或崩溃。

## Wrong Hypotheses Or Detours

### Detour 1: RenderDoc replay 拉起或 capture 完成回调破坏了 UI 状态

- Why it looked plausible:
  崩溃是在新增 RenderDoc UI 之后出现的，第一反应很容易怀疑 capture 或 replay-open 新路径。
- Why it was not the final cause:
  调用栈在 ImGui 的 disabled stack 记账逻辑里就已经出错，甚至还没走到 replay launch。问题在按钮点击这一帧就能复现，不需要等 capture finalize。

### Detour 2: `TickFrame` 里的 pending capture polling 造成了 UI 栈错配

- Why it looked plausible:
  `PollPendingRenderDocCapture()` 会在后续帧修改状态字符串和 capture 状态，看起来像是容易把 UI 状态弄乱的地方。
- Why it was not the final cause:
  崩溃发生在点击当帧，同一次 `DrawDebugUI()` 调用里就已经把 ImGui 栈配坏了，和后续 polling 是否完成 capture 无关。

## Final Root Cause

RenderDoc UI 直接用可变成员状态来同时决定 `ImGui::BeginDisabled()` 和 `ImGui::EndDisabled()`。点击 `Capture Current Frame` 时，`QueueManualRenderDocCapture()` 会在同一帧把 `m_renderdoc_manual_capture_pending` 从 `false` 改成 `true`。结果就是：这一帧可能没有执行 `BeginDisabled()`，却执行了 `EndDisabled()`，最终把 ImGui 的 disabled stack 弄坏，并在 `ImGui::EndDisabled()` 里崩溃。

## Final Fix

- 先把 disabled 条件快照成当前帧局部布尔值：
  - `disable_capture_button`
  - `disable_open_last_capture`
- 整个 `BeginDisabled()` / `EndDisabled()` 配对都只依赖这些稳定快照，不再直接读会在按钮回调里变化的成员状态。
- 真实的状态更新仍然保留在 `QueueManualRenderDocCapture()` 和 replay-open helper 里，但这些更新不能再反过来影响当前帧的 ImGui 栈配对。
- 另外顺手把 RenderDoc 生命周期也加固了一次：当进程启用了 RenderDoc 启动参数时，runtime RHI recreate 现在会在目标 backend 创建设备前重新执行 preload，避免 DX12/Vulkan 切换后共享手动 UI 只在冷启动时有效。

涉及文件：

- `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoBase.h`

## Validation

- Build result:
  修复后 `RendererDemo` 的 isolated verify build 成功。
  记录工件：`.tmp/build_renderdoc_uibase_fix_disabled.stdout.log`
- Runtime validation:
  修复后带 RenderDoc 的 DX12/Vulkan regression 验证都通过。
  记录工件：`.tmp/validate_renderdoc_demobase_ui_v2/rv_20260315_162713/summary.json`
- User acceptance:
  用户先复现了这次崩溃，随后确认 disabled-stack 修复后问题已经解决。

## Reflection And Prevention

- 任何依赖可变 runtime 状态的 ImGui `Begin*` / `End*` 配对，只要按钮回调可能在当前帧修改该状态，就必须先快照成局部变量。
- 只要 UI 会 arm 异步工作，就不要在同一分支里两次直接读取会变化的成员状态来决定栈式 ImGui API。
- 一个实用的 guardrail 是：每次新增 `BeginDisabled()` / `EndDisabled()` 时，都问一句“这个条件会不会在当前 frame 的按钮回调里变化？”如果会，立刻改成局部快照。
