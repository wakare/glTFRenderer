# SSAO Evaluate 采样 Kernel 优化复盘

## 摘要

这份记录对应 `2026-03-20` 的第四轮 SSAO 优化跟进。

本轮范围：

- 只针对 `SSAO Evaluate`，
- 把每个 sample 都重新生成随机半球方向的做法，改成固定 32-tap 半球 kernel，
- 通过“每像素沿法线旋转一次 kernel”来保留采样去相关，
- 重跑 DX12 和 Vulkan sweep，
- 再补一个更窄的 repeated-default perf suite，用来减少 mixed-case 噪声。

这轮的目标很明确：先只降低 sample 循环里的 ALU 成本，不改遮挡判定公式本身。

## 代码改动

更新文件：

- `glTFRenderer/RendererDemo/Resources/Shaders/SSAO.hlsl`

行为改动：

- `ComputeSSAO(...)` 里不再为每个 sample 调 `rand()`，
- 不再为每个 sample 调 `sampleHemisphereCosine(...)` 现算半球方向，
- 去掉了额外的 per-sample 方向归一化，
- 增加了固定的 32-sample hemisphere kernel，
- 增加了一个基于 interleaved gradient noise 的“每像素单次旋转”，
- 保留了现有的 view-space 投影和遮挡判定逻辑。

这条路线比直接把 occlusion 改成更便宜的 depth-only compare 风险更低。

## 验证

构建命令：

```powershell
powershell -ExecutionPolicy Bypass -File .\glTFRenderer\scripts\Build-RendererDemo-Verify.ps1
```

构建结果：

- 成功
- binlog: `glTFRenderer/build_logs/rendererdemo_20260320_020627.binlog`

本轮相关 regression 输出：

- DX12 sweep:
  - `.tmp/regression_capture/dx12/ssao_model_viewer_sweep_20260320_020732/suite_result.json`
- DX12 sweep rerun:
  - `.tmp/regression_capture/dx12/ssao_model_viewer_sweep_20260320_020840/suite_result.json`
- Vulkan sweep:
  - `.tmp/regression_capture/vulkan/ssao_model_viewer_sweep_20260320_020752/suite_result.json`
- Vulkan sweep rerun:
  - `.tmp/regression_capture/vulkan/ssao_model_viewer_sweep_20260320_020931/suite_result.json`
- DX12 repeated-default perf suite:
  - `.tmp/regression_capture/dx12/ssao_model_viewer_perf_20260320_021111/suite_result.json`
- Vulkan repeated-default perf suite:
  - `.tmp/regression_capture/vulkan/ssao_model_viewer_perf_20260320_021128/suite_result.json`

## 结果

### Sweep 观察

有一组 sweep 的 default case 结果如下：

| Backend | SSAO Evaluate | SSAO Blur X | SSAO Blur Y | SSAO Total |
| --- | ---: | ---: | ---: | ---: |
| DX12 | 0.3503 ms | 0.0326 ms | 0.0312 ms | 0.4142 ms |
| Vulkan | 0.2510 ms | 0.0298 ms | 0.0289 ms | 0.3098 ms |

但 rerun 并不够稳定，不能直接把 mixed-case sweep 当成这轮的最终结论：

- DX12 rerun 的 default 进一步升到了 `0.6045 ms`，
- Vulkan rerun 的 default 升到了 `0.3928 ms`，
- Vulkan rerun 的 `sample_count = 8` 也出现了异常 spike。

所以这轮 mixed-case sweep 更适合当 sanity check，而不是最终指标来源。

### Repeated-default perf suite

repeated-default perf suite 的数据明显更稳定。

DX12：

| Case | SSAO Evaluate | Blur Total | SSAO Total |
| --- | ---: | ---: | ---: |
| `ssao_perf_01` | 0.2474 ms | 0.0590 ms | 0.3064 ms |
| `ssao_perf_02` | 0.2476 ms | 0.0588 ms | 0.3063 ms |
| `ssao_perf_03` | 0.2477 ms | 0.0589 ms | 0.3066 ms |

Vulkan：

| Case | SSAO Evaluate | Blur Total | SSAO Total |
| --- | ---: | ---: | ---: |
| `ssao_perf_01` | 0.2500 ms | 0.0593 ms | 0.3092 ms |
| `ssao_perf_02` | 0.2497 ms | 0.0594 ms | 0.3091 ms |
| `ssao_perf_03` | 0.2492 ms | 0.0596 ms | 0.3089 ms |

可以这样理解：

- repeated-default 路径现在在两个 backend 上都收得很紧，
- blur 成本几乎没动，
- 这次收益确实只出现在预期位置，也就是 `SSAO Evaluate`。

### 和上一版 isolated default 数字对比

本轮前的参考 default 数字：

- DX12 isolated sweep default:
  - `.tmp/regression_capture/dx12/ssao_model_viewer_sweep_20260320_014816/suite_result.json`
  - total `0.3444 ms`，evaluate `0.2812 ms`
- Vulkan isolated sweep default:
  - `.tmp/regression_capture/vulkan/ssao_model_viewer_sweep_20260320_014833/suite_result.json`
  - total `0.3273 ms`，evaluate `0.2685 ms`

当前 repeated-default 结果：

- DX12:
  - 大约 `0.3064 ms` total，`0.2476 ms` evaluate
- Vulkan:
  - 大约 `0.3091 ms` total，`0.2496 ms` evaluate

这意味着大致：

- DX12：total 约 `-0.0378 ms`，evaluate 约 `-0.0336 ms`
- Vulkan：total 约 `-0.0182 ms`，evaluate 约 `-0.0189 ms`

这里要明确一点：这不是完美的 apples-to-apples 对比，因为参考值来自 mixed-case sweep，而新的值来自 repeated-default cases。所以这部分是基于现有数据做的推断，不是严格同条件 benchmark。但方向是自洽的，而且 blur 成本几乎没变，也支持“收益来自 evaluate sampling-path”这个结论。

## 结论

### 这轮完成了什么

- `SSAO Evaluate` 不再为每个 sample 支付随机数和 cosine hemisphere 生成的 ALU 成本。
- evaluate 路径现在有了更便宜、更稳定的采样基底。
- 当前最可信的 default 测量大约是 `0.306-0.309 ms` 的 SSAO total，其中 `Evaluate` 大约 `0.25 ms`。

### 这意味着什么

这次优化方向是成立的，而且风险较低。

同时，它也说明后续优化重心已经变了：

- 剩下的 evaluate 成本，更像是被 depth fetch、sample reprojection 和 sample-position reconstruct 主导，
- 如果还要继续省，下一步更值得动的是 occlusion test 本身，而不只是继续抠 sample 生成的数学开销。

### 建议的下一步

1. 尝试更便宜的 depth-compare occlusion 模型，避免每个 tap 都去完整重建 sample 的 view-space 位置。
2. 后续评估 `0.03 ms` 以内的 SSAO 优化时，优先使用 repeated-default perf suite。
3. mixed-case DX12/Vulkan sweep 的 spike 继续当成 backend 诊断问题看，不要直接当 shader 证据。

## TODO

1. 继续调查为什么 mixed-case 的 DX12 和 Vulkan sweep 在 `SSAO Evaluate` 上仍然有明显波动，即使 capture-window averaging 和 regression isolation 都已经补上了。
2. 检查 case 切换、resource 生命周期、timestamp 采集，或者 backend 特定调度是否污染了 SSAO 的细粒度 pass timing。
3. 如果后续 rerun 里 backend 差异还是很大，就继续把它当成潜在 backend 实现问题记录到后续文档里。
