# SSAO Evaluate 重构与性能均值统计复盘

## 摘要

这份记录对应 blur 拆分之后的第二轮 SSAO 优化。

本轮范围包括：

- 给 `DemoAppModelViewer` 的 regression 输出补 capture-window 平均 pass 统计，
- 扩展 `ViewBuffer`，让屏幕空间 pass 能直接拿到 view/projection 数据，
- 把 `SSAO.hlsl` 的 evaluate 逻辑从 world-space 重建改成更轻的 view-space 公式，
- 用新的统计方法重新跑 DX12 和 Vulkan sweep。

这一轮最明确的收益，是测量方法本身变可靠了。

shader 重构也让 evaluate 路径更干净，但实际 GPU 收益不大：

- Vulkan 默认 `16` sample 的 SSAO 从 `0.3461 ms` 降到 `0.3281 ms`，约 `-5.2%`，
- DX12 没有出现足够稳定、可以和噪声明确区分开的收益。

## 代码改动

### 1. capture-window 平均 pass 统计

`glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.*` 现在会对整个 capture 窗口内的 pass timing 做累计，而不是只写 case 最后一帧的 pass 快照。

新的输出包括：

- `.pass.csv` 现在写的是 capture-window 平均值，
- `.perf.json` 新增了 `pass_stats_avg`，
- 每个 pass 还会带上 presence ratio、executed ratio、skipped-validation ratio、GPU-valid ratio。

这让 SSAO 这种轻量 pass 的多次采样，比以前“只看末帧”更容易比较。

### 2. `ViewBuffer` 补齐 view/projection 数据

`glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.*`、
`glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`、
以及 `glTFRenderer/RendererDemo/Resources/Shaders/SceneViewCommon.hlsl`
现在会给 shader 提供：

- `view_matrix`
- `projection_matrix`
- `inverse_projection_matrix`
- `projection_params`

这样 SSAO 就不需要只依赖 `inverse_view_projection_matrix`。

### 3. SSAO evaluate 重写

`glTFRenderer/RendererDemo/Resources/Shaders/SSAO.hlsl` 现在：

- 在 view space 里重建中心点和 sample 点，
- 每个像素只把 GBuffer normal 转一次到 view space，
- 通过 `projection_params` 做解析投影，不再每个 sample 做一次投影矩阵乘，
- 继续沿用上一轮已经拆好的 separable blur。

这轮重写的目标，是在尽量不改 AO 判定逻辑的前提下，把不必要的 world-space 重建拿掉。

## 验证

构建：

```powershell
powershell -ExecutionPolicy Bypass -File .\glTFRenderer\scripts\Build-RendererDemo-Verify.ps1
```

构建结果：

- 成功
- binlog: `glTFRenderer/build_logs/rendererdemo_20260320_012025.binlog`

本轮使用的 regression 输出：

- DX12 默认 perf suite:
  - `.tmp/regression_capture/rc_dx_20260320_012221/ssao_model_viewer_perf_20260320_012222/suite_result.json`
- Vulkan 默认 perf suite:
  - `.tmp/regression_capture/rc_vk_20260320_012246/ssao_model_viewer_perf_20260320_012247/suite_result.json`
- DX12 最新 sweep:
  - `.tmp/regression_capture/rc_dx_20260320_012925/ssao_model_viewer_sweep_20260320_012926/suite_result.json`
- Vulkan 最新 sweep:
  - `.tmp/regression_capture/rc_vk_20260320_012839/ssao_model_viewer_sweep_20260320_012840/suite_result.json`

另外还保留了两组稳定性判断用的 rerun：

- DX12 噪声 rerun:
  - `.tmp/regression_capture/rc_dx_20260320_012554/ssao_model_viewer_sweep_20260320_012555/suite_result.json`
- Vulkan rerun:
  - `.tmp/regression_capture/rc_vk_20260320_012640/ssao_model_viewer_sweep_20260320_012642/suite_result.json`

## 结果

### 最新稳定 sweep 快照

| Backend | Case | SSAO Evaluate | SSAO Blur X | SSAO Blur Y | SSAO Total |
| --- | --- | ---: | ---: | ---: | ---: |
| DX12 | default | 0.2811 ms | 0.0319 ms | 0.0308 ms | 0.3438 ms |
| DX12 | `blur_radius = 0` | 0.2808 ms | 0.0204 ms | 0.0204 ms | 0.3215 ms |
| DX12 | `sample_count = 8` | 0.1481 ms | 0.0204 ms | 0.0202 ms | 0.1887 ms |
| DX12 | `ssao_enabled = false` | 0.0207 ms | 0.0193 ms | 0.0195 ms | 0.0595 ms |
| Vulkan | default | 0.2687 ms | 0.0301 ms | 0.0293 ms | 0.3281 ms |
| Vulkan | `blur_radius = 0` | 0.2685 ms | 0.0206 ms | 0.0201 ms | 0.3092 ms |
| Vulkan | `sample_count = 8` | 0.1413 ms | 0.0201 ms | 0.0205 ms | 0.1819 ms |
| Vulkan | `ssao_enabled = false` | 0.0182 ms | 0.0191 ms | 0.0189 ms | 0.0562 ms |

### 和上一轮 blur-only 基线对比

真正比较干净、可复现的 apples-to-apples 对比，还是 Vulkan。

上一轮 blur-only 的 Vulkan sweep 是：

- default: `0.3461 ms`
- `blur_radius = 0`: `0.3103 ms`
- `sample_count = 8`: `0.1813 ms`
- `ssao_enabled = false`: `0.0564 ms`

本轮 Vulkan sweep 是：

- default: `0.3281 ms`
- `blur_radius = 0`: `0.3092 ms`
- `sample_count = 8`: `0.1819 ms`
- `ssao_enabled = false`: `0.0562 ms`

差值：

- default: `-0.0180 ms`（`-5.2%`）
- `blur_radius = 0`: `-0.0011 ms`
- `sample_count = 8`: `+0.0006 ms`
- `ssao_enabled = false`: `-0.0002 ms`

可以这样理解：

- blur 的剩余成本几乎没变，
- disabled path 的残余开销几乎没变，
- `8` sample 路径几乎没变，
- 真正看到收益的，主要是默认 `16` sample evaluate 路径里“后半段更重的那部分”。

也就是说，这轮重构没有改掉整个 SSAO 成本轮廓，只是在默认高 sample 配置下削掉了一小块 evaluate 成本。

## 测量说明

### 哪些地方确实变好了

新的 capture-window 平均统计值得保留。

和旧的“只看末帧 pass CSV”相比：

- `pass_stats_avg` 对轻量 GPU pass 更可靠，
- blur timing 现在跨运行更容易对齐，
- perf JSON 也更适合后续脚本化对比。

### 哪些地方还没有完全解决

DX12 的 `SSAO Evaluate` 仍然会出现很大的跨运行波动。

观察到的现象：

- 一次 DX12 rerun 仍然接近 `0.3438 ms` 总成本，
- 另一次 DX12 rerun 的同一个 default case 升到了约 `0.8484 ms`，
- spike 主要集中在 `SSAO Evaluate`，而不是 `Blur X` / `Blur Y`。

所以，capture-window 平均统计虽然修掉了“末帧采样”这个问题，但还没有让 DX12 的细粒度 SSAO timing 变成完全可信的微基准。

在当前这个场景里，Vulkan 更适合用来判断 SSAO 的微优化收益。

## 结论

### 这轮已经完成的事

- SSAO regression 输出现在有 capture-window 平均 pass 数据了。
- `SSAO.hlsl` 的主要 evaluate 路径已经不再依赖 world-space 重建。
- view-space 重写在 Vulkan 默认 `16` sample 路径上，带来了小幅但可复现的收益。

### 这意味着什么

这次收益有限，基本说明当前 SSAO 已经不再只是被矩阵运算主导。

这是基于测量结果的推断，不是直接 profiler 证据，但最合理的解释是：

- depth / normal 纹理访问延迟和带宽，已经占了剩余成本里的较大比例，
- 默认重配置依然主要受 sample count 影响，
- 再做一轮“小幅数学化简”，很难再拿到大的收益。

### 建议的下一步

1. 当 `blur_radius == 0` 时直接跳过 blur pass；当 `enabled == 0` 时直接跳过整条 SSAO pass，而不是继续 dispatch 再在 shader 里 early-out。
2. 如果还需要继续省，考虑更大一级的算法改动，例如 half-resolution SSAO、hierarchical depth 辅助，或者更便宜的 depth-only AO 近似。
3. 把 DX12 SSAO pass timing 当成独立诊断问题处理，在修好之前，不要只靠它判断 `0.02 ms` 级别的小优化。

## TODO

1. 调查 `2026-03-20` 这轮复测里 `SSAO Evaluate` 在 DX12 和 Vulkan 之间表现出的明显稳定性差异。这个现象可能不是纯 shader 成本问题，而是 backend 特定实现存在问题的信号。
2. 在继续基于 DX12 数据做 SSAO 微优化之前，先检查这种分歧是否来自 backend 特定的 timestamp/profiler 行为、dispatch 调度、resource barrier，或者其他 DX12 运行时细节。
