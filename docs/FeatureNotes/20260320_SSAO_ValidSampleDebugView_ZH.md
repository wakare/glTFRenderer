# SSAO 有效采样调试视图

## 摘要

这次补了一个 SSAO 调试输出模式，可以通过现有的 Tone Map SSAO 调试视图直接查看 `ComputeSSAO(...)` 里的 `valid_sample_count / sample_count`。

## 背景

当 SSAO 看起来几乎没有效果时，一个常见怀疑点是 evaluate pass 把大部分 sample 都过滤掉了，导致 `valid_sample_count` 长期接近 `0`。在这次修改前，运行时只能看到最终 AO 值，无法快速判断这个怀疑是否成立。

## 改动内容

- `RendererSystemSSAO::SSAOGlobalParams` 新增了 `debug_output_mode`
- `SSAO.hlsl` 现在支持两种输出：
  - `0`：正常 AO 输出
  - `1`：`valid_sample_count / sample_count`
- 开启该调试输出时：
  - 无效 SSAO 输入会返回 `0.0`
  - blur pass 会被跳过，确保看到的是原始 `ComputeSSAO(...)` 结果，而不是被模糊后的值
- model viewer 的 snapshot 序列化也会保留 `debug_output_mode`

## 使用方式

1. 打开 `SSAO` 面板
2. 将 `Debug Output` 切到 `Valid Sample Ratio`
3. 打开 `Tone Map` 面板
4. 将 `Debug View` 切到 `SSAO`

结果解释：

- 黑色：`valid_sample_count == 0`
- 灰色：只有部分 sample 有效
- 白色：大部分或全部 sample 有效

## 注意

这个调试输出会直接覆盖 lighting 实际消费的 SSAO 信号，所以它只适合排查，不适合拿来做正常画面对比。
