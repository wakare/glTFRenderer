# SSAO 跳过 Dispatch 与 Regression 隔离修复复盘

## 摘要

这份记录对应 `2026-03-20` 的第三轮 SSAO 跟进工作，时间点在前面的 blur 拆分和 evaluate 重构之后。

本轮范围包括：

- 当 `blur_radius == 0` 时直接跳过 SSAO blur dispatch，
- 当 blur 被跳过时，让 lighting 直接绑定 raw SSAO 输出，
- disabled 路径沿用同一套输出路由，不再支付 blur dispatch 成本，
- 给 regression harness 补上“每个 case 前恢复 suite baseline snapshot”的隔离机制，
- 用 capture-window 平均 pass 统计重新跑 DX12 和 Vulkan 的 VSync sweep。

这轮最关键的两个结果是：

- skip-dispatch 路径确实在 `blur_radius = 0` 和 `ssao_enabled = false` 的 case 上带来了预期收益，
- regression harness 确实存在 case 隔离 bug，导致一轮中间复测必须废弃，因为 `ssao_blur0` 把 `blur_radius = 0` 泄漏到了 `ssao_sample8`。

## 代码改动

### SSAO pass 路由

更新文件：

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSSAO.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSSAO.cpp`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`

行为改动：

- `RendererSystemSSAO::GetOutputs()` 现在会动态返回当前真正生效的 SSAO 输出，
- 只有在 `enabled != 0` 且 `blur_radius > 0` 时，blur pass 才会被注册和 dispatch，
- lighting 每帧都会重新绑定当前有效的 SSAO 输出，而不是假设永远存在 blur 目标。

这样 default 路径保持不变，但 `blur_radius = 0` 和 disabled case 不再为无意义的 blur dispatch 付费。

### Regression 状态隔离

更新文件：

- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.h`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`

行为改动：

- `ModelViewerStateSnapshot` 现在会保存 SSAO 全局参数，
- snapshot JSON 的导入导出现在也会保留 SSAO 状态，
- regression 启动时会先捕获一次 suite baseline snapshot，
- 每个 case 开始前都会先恢复这份 baseline，再应用 case 自己的 override。

这才是真正修掉 cross-case 污染的地方。

## 验证

构建命令：

```powershell
powershell -ExecutionPolicy Bypass -File .\glTFRenderer\scripts\Build-RendererDemo-Verify.ps1
```

构建结果：

- 成功
- binlog: `glTFRenderer/build_logs/rendererdemo_20260320_014750.binlog`

最终可用的 isolated regression 输出：

- DX12:
  - `.tmp/regression_capture/dx12/ssao_model_viewer_sweep_20260320_014816/suite_result.json`
- Vulkan:
  - `.tmp/regression_capture/vulkan/ssao_model_viewer_sweep_20260320_014833/suite_result.json`

不应再拿来做最终 `sample_count = 8` 结论的中间复测：

- DX12:
  - `.tmp/regression_capture/dx12/ssao_model_viewer_sweep_20260320_014356/suite_result.json`
- Vulkan:
  - `.tmp/regression_capture/vulkan/ssao_model_viewer_sweep_20260320_014412/suite_result.json`

这两组中间结果发生在“snapshot 序列化补全之后、baseline restore 补上之前”，依然存在 case 之间的状态泄漏。

## 结果

### 最终 isolated sweep

| Backend | Case | SSAO Evaluate | SSAO Blur X | SSAO Blur Y | SSAO Total |
| --- | --- | ---: | ---: | ---: | ---: |
| DX12 | default | 0.2812 ms | 0.0319 ms | 0.0314 ms | 0.3444 ms |
| DX12 | `blur_radius = 0` | 0.2819 ms | skipped | skipped | 0.2819 ms |
| DX12 | `sample_count = 8` | 0.1485 ms | 0.0317 ms | 0.0314 ms | 0.2116 ms |
| DX12 | `ssao_enabled = false` | 0.0209 ms | skipped | skipped | 0.0209 ms |
| Vulkan | default | 0.2685 ms | 0.0298 ms | 0.0289 ms | 0.3273 ms |
| Vulkan | `blur_radius = 0` | 0.2682 ms | skipped | skipped | 0.2682 ms |
| Vulkan | `sample_count = 8` | 0.1413 ms | 0.0297 ms | 0.0296 ms | 0.2006 ms |
| Vulkan | `ssao_enabled = false` | 0.0183 ms | skipped | skipped | 0.0183 ms |

### 相对 skip-dispatch 之前 sweep 的差值

本轮对比基线：

- DX12: `.tmp/regression_capture/rc_dx_20260320_012925/ssao_model_viewer_sweep_20260320_012926/suite_result.json`
- Vulkan: `.tmp/regression_capture/rc_vk_20260320_012839/ssao_model_viewer_sweep_20260320_012840/suite_result.json`

最关键的变化：

- DX12 default: `0.3438 -> 0.3444 ms`（`+0.0006 ms`，基本可视为噪声）
- DX12 `blur_radius = 0`: `0.3215 -> 0.2819 ms`（`-0.0396 ms`，约 `-12.3%`）
- DX12 `ssao_enabled = false`: `0.0595 -> 0.0209 ms`（`-0.0386 ms`，约 `-64.9%`）
- Vulkan default: `0.3281 -> 0.3273 ms`（`-0.0008 ms`）
- Vulkan `blur_radius = 0`: `0.3092 -> 0.2682 ms`（`-0.0410 ms`，约 `-13.3%`）
- Vulkan `ssao_enabled = false`: `0.0562 -> 0.0183 ms`（`-0.0379 ms`，约 `-67.4%`）

可以这样理解：

- 这次 skip-dispatch 优化在非默认路径上是成立的，
- default case 基本没变，这本来就是预期结果，因为 blur 仍然是合法输出路径的一部分，
- disabled 路径已经便宜很多，但还没到理论最小，因为现在仍然保留了一个会 early-out 的 `SSAO Evaluate` dispatch。

### Regression harness 更正

skip-dispatch 改完后的第一轮复测里，`ssao_sample8` 一度没有出现 `Blur X` 和 `Blur Y`。

那个结果是无效的。

根因是：

- regression case 之间没有恢复 suite baseline 状态，
- `ssao_blur0` 把 `blur_radius = 0` 写进了运行态，
- `ssao_sample8` 只覆盖了 `sample_count`，
- 所以 `blur_radius = 0` 直接泄漏到了下一个 case。

现在最终 isolated sweep 已经证明修复生效：

- `ssao_sample8` 在两个 backend 上都重新带回了 blur pass，
- 只有 `blur_radius = 0` 和 `ssao_enabled = false` 才会跳过 blur dispatch。

## 结论

### 这轮完成了什么

- SSAO 在 blur 不需要时，已经不会再支付 blur dispatch 开销。
- lighting 已经能正确跟踪当前真正有效的 SSAO 输出，不再假设固定的 blur 目标。
- regression sweep 重新具备了 case 级隔离性，后续 SSAO 参数扫描的数据可以信任。

### 还剩什么

- `ssao_enabled = false` 仍然会 dispatch `SSAO Evaluate` 再 early-out，所以 disabled 路径还不是绝对最小成本。
- DX12 和 Vulkan 仍然不适合作为完全等价的 SSAO 微基准 backend。

## TODO

1. 当 `enabled == 0` 时，把 `SSAO Evaluate` dispatch 也跳掉，考虑改成绑定常量白色 AO 输出或其他无 dispatch 路径。
2. 继续调查 `2026-03-20` 这轮复测里 `SSAO Evaluate` 在 DX12 和 Vulkan 之间表现出的明显稳定性差异。这个差异仍然大到足以怀疑 backend 特定实现存在问题，而不只是正常噪声。
3. 在继续基于 DX12 单独做 SSAO 微优化之前，先检查这种 backend 分歧是否来自 timestamp/profiler 行为、dispatch 调度、resource barrier，或者其他 DX12 运行时细节。
