# Debug Note - directional light zero-speed flicker

- Status: Accepted
- Date: 2026-03-22
- Scope: `RendererDemo -> DemoAppModelViewer / RendererModuleLighting / frame-buffered light constants`
- Commit: `(this commit)`
- 配套文档：
  - `docs/debug-notes/2026-03-22-directional-light-zero-speed-flicker.md`

## Symptom

在 `DemoAppModelViewer` 里，把 `Directional Light Speed` 设成 `0` 时会偶发闪烁；把同一个参数改成很小的非零值后，闪烁又会消失。

## Reproduction

- 启动 `RendererDemo`。
- 打开 `DemoAppModelViewer`。
- 保持相机基本固定，方便观察光照是否稳定。
- 把 `Directional Light Speed` 设为 `0.0`。
- 观察光照并不会始终稳定，偶尔会出现闪烁。
- 再把同一个参数改成很小的非零值，例如 `0.001` 到 `0.01` rad/s。
- 观察闪烁消失，虽然肉眼看上去光照仍然几乎静止。

## Wrong Hypotheses Or Detours

### Detour 1: `speed = 0` 的旋转数学或方向 epsilon 判定本身不稳定

- Why it looked plausible:
  `DemoAppModelViewer` 在 `speed == 0` 和 `speed != 0` 时确实走了不同的更新路径，而且只有当方向变化超过 epsilon 才会调用 `UpdateLight()`。
- Why it was not the final cause:
  真正的方向计算本身是稳定的。问题不在 `sin/cos` 或 normalize，而在于光源停止变化后，不同 frame slot 上保存的 GPU light data 并不一致。

### Detour 2: 闪烁的主要根因应该在 directional shadow matrix 更新链

- Why it looked plausible:
  这个现象看起来很像阴影不稳定，而 `RendererSystemLighting` 里确实存在 shadow 相关的 per-frame view-buffer 更新。
- Why it was not the final cause:
  更底层的问题其实先出现在主 light-info buffer 上。shadow 更新会让现象更容易被看到，但不是触发旧状态交替的必要条件。

## Final Root Cause

`RendererModuleLighting` 把 light data 存在 frame-buffered buffer 里，但 `UploadAllLightInfos()` 之前调用的是 `UploadFrameBufferedBufferData()`，这个接口只会把数据写到“当前 frame slot”。

写完当前 slot 之后，模块就把 `m_need_upload_light_infos` 清掉了。结果是：一次 light change 可能只刷新了一个 slot，而其他 in-flight slot 仍然保留旧的光照常量。lighting pass 每帧又会绑定当前 frame slot，所以随着 frame slot 轮换，渲染会在“新 light data”和“旧 light data”之间来回切换。这个 bug 在 `Directional Light Speed = 0` 时最容易暴露，因为光照很快停止变化，后续不再有新的 upload 去补齐剩余 slot；而很小的非零 speed 会持续把 light 标成 dirty，间接把所有 slot 逐步刷新，所以现象被掩盖了。

## Final Fix

- 更新了 `glTFRenderer/RendererDemo/RendererModule/RendererModuleLighting.cpp`。
- 调整 `UploadAllLightInfos()`，让 dirty light upload 不再只写当前 slot，而是写到以下所有 handle：
  - `m_light_buffer_handles`
  - `m_light_count_buffer_handles`
- 保留原有 dirty flag 语义，但让上传范围和 lighting pass 的 frame-buffered 绑定模型保持一致。

## Validation

- Build result:
  `RendererDemo` 验证构建通过，`0` 个 error，`54` 个 warning。
  日志：
  - `build_logs/rendererdemo_20260322_193011.msbuild.log`
  - `build_logs/rendererdemo_20260322_193011.stdout.log`
  - `build_logs/rendererdemo_20260322_193011.stderr.log`
  - `build_logs/rendererdemo_20260322_193011.binlog`
- Runtime validation:
  这次回合里没有再跑新的自动化视觉抓取。当前验证依据是复现链路核对、upload/binding 范围不匹配已经确认、以及验证构建通过。
- User acceptance:
  在看过诊断和补丁后，用户确认问题看起来已经修复，并要求提交这组改动。

## Reflection And Prevention

- What signal should have been prioritized earlier:
  最关键的信号其实是“`speed = 0` 会闪，而极小非零 speed 不闪”。这个模式更像是跨 frame 的陈旧状态刷新问题，而不是纯粹的着色数学或随机数值不稳定。
- What guardrail or refactor would reduce recurrence:
  对 frame-buffered 资源，应该提供显式的“写所有 slot”辅助路径；对于可能在一次修改后立刻静止的状态，不能默认只写当前 slot。
- What to check first if a similar symptom appears again:
  先对比 producer 的 upload 范围和 consumer 的绑定范围。如果 pass 是按 frame slot 取资源，而 producer 只更新当前 slot，就要优先怀疑 in-flight frame 间的新旧状态交替。
