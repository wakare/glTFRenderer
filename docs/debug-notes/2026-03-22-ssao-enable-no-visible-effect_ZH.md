# 2026-03-22 SSAO 开关勾选后几乎无可见效果

- 范围：`RendererDemo` 的 model viewer SSAO evaluate 路径、lighting 集成、调试可视化
- 配套文档：
  - [2026-03-22-ssao-enable-no-visible-effect.md](D:/Work/DevSpace/glTFRenderer/glTFRenderer/docs/debug-notes/2026-03-22-ssao-enable-no-visible-effect.md)

## Symptom

在 model viewer 里切换 `SSAO -> Enable SSAO` 时，最终画面几乎看不出差异。

初步检查时，UI 开关和参数上传链路看起来是通的，但最终渲染结果仍然基本不变。

## Reproduction

- 应用：`DemoAppModelViewer`
- 场景：默认 Sponza
- Backend：在 Vulkan 验证时被明确观察到，但问题本质在共享的 SSAO 逻辑，而不是 backend 专属的 render-graph 绑定错误
- 步骤：
  - 打开 `SSAO` 面板
  - 切换 `Enable SSAO`
  - 对比最终画面

真正暴露问题的调试路径：

- `SSAO -> Debug Output = Valid Sample Ratio`
- `Tone Map -> Debug View = SSAO`

## Wrong Hypotheses Or Detours

### Detour 1

第一反应是 UI checkbox 没有真正传到 shader，或者 lighting pass 仍然绑定着旧的 SSAO 资源。

后来确认这条判断不成立。UI 确实更新了 `m_global_params.enabled`，shader 也尊重 `enabled == 0u`，lighting pass 读取的也是 `m_ssao->GetOutputs().output`。

### Detour 2

第二个怀疑是 SSAO 其实在工作，只是因为当前实现只影响环境漫反射，所以视觉上很弱。

这个解释只能说明“为什么在某些打光条件下效果可能不明显”，但不是这次问题的主因。调试视图显示 evaluate 阶段几乎把所有 sample 都过滤掉了，所以 AO 信号本身就大多退化成了中性值。

### Detour 3

第一版 view-space SSAO 优化为了降低开销，把原来的世界空间重建和投影改成了基于投影标量的解析近似路径。

这版优化在性能上是有收益的，但也引入了一条更脆弱的重建/重投影路径。新增的 `Valid Sample Ratio` 调试模式把这个问题直接暴露出来了，因为整帧几乎都是黑的。

## Final Root Cause

`ComputeSSAO(...)` 使用的简化版 view-space 重建与重投影路径，没有稳定地对齐引擎运行时真正的相机投影约定。

结果就是大多数 SSAO tap 在验证链路中失败，或者重建出的 sample position 不一致，导致大部分像素的 `valid_sample_count` 长期接近 `0`。一旦出现这种情况，evaluate pass 就会回落到中性 AO 值，所以最终 lighting 中几乎看不出 SSAO 开关带来的差别。

## Final Fix

- 在 `glTFRenderer/RendererDemo/Resources/Shaders/SSAO.hlsl` 中，将解析近似版的 view-space 重建替换为基于 `inverse_projection_matrix` 的矩阵重建
- 将解析近似版的重投影替换为基于 `projection_matrix` 的矩阵重投影
- sample 命中后，改为基于实际 sample texel 重建 sample position，而不是继续沿用中间浮点 UV
- 新增 `Valid Sample Ratio` 调试输出，可直接观察 `valid_sample_count / sample_count`
- 通过现有 tone-map SSAO 调试视图输出这份调试信号
- 将 SSAO 调试状态纳入 model viewer snapshot 序列化
- 扩展 regression 接口，使 SSAO 参数 sweep 与性能测试可以脚本化

## Validation

- Build result:
  - `BuildSucceeded`
  - [rendererdemo_20260320_074943.binlog](D:/Work/DevSpace/glTFRenderer/glTFRenderer/build_logs/rendererdemo_20260320_074943.binlog)
- Runtime validation:
  - 在修复投影链路后，`Valid Sample Ratio` 调试视图从几乎整屏发黑恢复为正常的非零分布
  - 将 `Debug Output` 切回普通 `AO` 后，最终画面中可以看到恢复后的 SSAO 可见效果
- User acceptance:
  - 用户已确认修复后 SSAO 效果恢复

## Reflection And Prevention

- What signal should have been prioritized earlier:
  - 这类问题最有效的首要信号，不是最终 lit frame，而是 `ComputeSSAO(...)` 内部 sample 验证分布本身
- What guardrail or refactor would reduce recurrence:
  - 除非经过充分验证，否则优先复用共享的矩阵重建 helper，不要轻易用手推投影近似公式替代
  - 保留 SSAO 内部调试输出入口，这样 evaluate 阶段失败时无需依赖 RenderDoc 也能快速定位
  - 如果后续 DX12 与 Vulkan 再次出现明显差异，应默认记为 backend 调查 TODO，而不是先假设 shader 数学在所有 backend 上天然一致
- What to check first if a similar symptom appears again:
  - lighting 是否绑定了当前 SSAO 输出资源
  - evaluate 阶段的 `valid_sample_count` 是否为非零
  - SSAO 所用的相机投影与深度约定是否与运行时矩阵保持一致
