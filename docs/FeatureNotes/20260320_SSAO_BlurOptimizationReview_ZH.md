# SSAO Blur 优化复盘

## 摘要

这次工作聚焦于 `RendererDemo` 当前的 SSAO 路径：

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSSAO.*`
- `glTFRenderer/RendererDemo/Resources/Shaders/SSAO.hlsl`

本次落地的优化刻意选择了低风险路径：

- 把 SSAO 运行参数暴露成可编程接口，
- 把这些参数接入 regression logic pack，
- 把原来的单次 2D bilateral blur 改成 separable bilateral blur（`Blur X` + `Blur Y`）。

目标是先降低 blur 阶段成本，而不是在同一轮里同时重写 SSAO evaluate 阶段。

## 代码改动

### 1. SSAO 参数 API

`RendererSystemSSAO` 现在提供：

- `GetGlobalParams()`
- `SetGlobalParams(...)`

这样 SSAO 参数就不再只能通过调试 UI 改，而是可以被 regression 脚本直接控制。

### 2. Regression 控制

`model_viewer_lighting` 的 `RegressionLogicPack` 现在支持以下 SSAO 参数：

- `ssao_enabled`
- `ssao_radius_world`
- `ssao_intensity`
- `ssao_power`
- `ssao_bias`
- `ssao_thickness`
- `ssao_sample_distribution_power`
- `ssao_blur_depth_reject`
- `ssao_blur_normal_reject`
- `ssao_sample_count`
- `ssao_blur_radius`

这一步的目的，是让 SSAO 的性能 sweep 可以脱离手动 UI 调参。

### 3. Blur 路径重构

改动前，SSAO blur 是：

- 一个全屏 compute pass，
- 2D bilateral kernel，
- 默认 `blur_radius = 2`，
- 等价于 5x5 邻域。

改动后，SSAO blur 变成：

- 一个水平方向 bilateral pass，
- 一个垂直方向 bilateral pass，
- 仍然使用原有的 `depth` / `normal` / `AO` rejection 输入，
- 每像素 tap 数显著下降。

本轮改动还没有重写 SSAO evaluate 本身。

## 测量方法

构建命令：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Build-RendererDemo-Verify.ps1
```

性能采集：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite .tmp\ssao_model_viewer_perf_suite.json `
  -Backend dx `
  -DemoApp DemoAppModelViewer

powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite .tmp\ssao_model_viewer_perf_suite.json `
  -Backend vk `
  -DemoApp DemoAppModelViewer
```

额外 sweep：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite .tmp\ssao_model_viewer_sweep_suite.json `
  -Backend vk `
  -DemoApp DemoAppModelViewer
```

场景与运行条件：

- `DemoAppModelViewer`
- 默认 Sponza 场景
- 固定相机
- 冻结方向光
- 关闭 regression UI

## 改动前基线

原始 blur 实现下测得：

| Backend | SSAO Evaluate | SSAO Blur | SSAO Total |
| --- | ---: | ---: | ---: |
| DX12 | 0.2799 ms | 0.1536 ms | 0.4335 ms |
| Vulkan | 0.2690 ms | 0.1416 ms | 0.4106 ms |

当时的结论已经比较明确：

- SSAO 是当前这一帧里最重的 GPU pass 之一，
- blur 阶段是其中一块可观成本，
- 但 evaluate 阶段仍然是长期主目标。

## 改动后结果

使用新 sweep 里比较稳定的 default case，可得到：

| Backend | SSAO Evaluate | SSAO Blur X | SSAO Blur Y | SSAO Total |
| --- | ---: | ---: | ---: | ---: |
| DX12 | 0.2826 ms | 0.0358 ms | 0.0348 ms | 0.3532 ms |
| Vulkan | 0.2806 ms | 0.0338 ms | 0.0317 ms | 0.3461 ms |

默认配置下的近似改善幅度：

| Backend | Before | After | Delta |
| --- | ---: | ---: | ---: |
| DX12 | 0.4335 ms | 0.3532 ms | -18.5% |
| Vulkan | 0.4106 ms | 0.3461 ms | -15.7% |

## 重要说明

在重复的轻负载采样里，`SSAO Evaluate` 偶尔会出现 spike，DX12 更明显，Vulkan 也偶尔会看到。

现象是：

- `Blur X` / `Blur Y` 的成本下降比较稳定，
- spike 出现在 `SSAO Evaluate`，而不是两个 blur pass，
- 由于整帧负载本来就很轻，单帧 pass CSV 的噪声比理想情况更大。

这意味着：

- blur 优化本身是成立的，
- 但当前这套“只记录 case 末帧 pass CSV”的方法，还不够稳定，不能把每次单次采样都当成完全干净的 benchmark。

因此，上面的结果应该理解成 steady-state 的方向性结论，而不是“每次都一样”的无噪声数据。

## 还有多少优化空间

Vulkan sweep 的分解最清楚，因为它更稳定：

| Case | SSAO Total |
| --- | ---: |
| Default | 0.3461 ms |
| `ssao_blur_radius = 0` | 0.3103 ms |
| `ssao_sample_count = 8` | 0.1813 ms |
| `ssao_enabled = false` | 0.0564 ms |

可以这样理解：

- 当前 separable blur 已经不再是主问题，
- 从 default 再把 blur 几乎拿掉，只能继续省约 `0.0358 ms`，
- 把 sample count 从 `16 -> 8`，可以省约 `0.1648 ms`，
- 完全关闭 SSAO，相比当前 default 可以拿掉约 `0.2897 ms`。

所以，接下来的主要优化空间已经集中到 evaluate 阶段，而不是 blur 阶段。

## 结论

### 已经改进的部分

- blur 阶段成本已经明显下降。
- SSAO 现在更容易通过 regression case 做参数化 benchmark。
- 这次优化改动局部、风险较低，没有改 SSAO 参与 lighting 的语义位置。

### 仍然需要继续做的部分

下一轮真正值得做的优化，应该放在 `ComputeSSAO(...)`，重点看它每个 sample 都在重复做的：

- world position reconstruction，
- world 到 UV 的 reprojection，
- sample world reconstruction 用于 occlusion 验证。

在当前这版 blur 重构之后，这一段仍然是主成本中心。

### 建议的后续动作

1. 给 regression perf 输出补“capture 窗口平均 pass 统计”，不要只看 case 末帧 pass CSV。
2. 把 SSAO evaluate 往更便宜的 view-space / linear-depth 公式重写。
3. 当 `enabled == 0` 时，考虑直接跳过 SSAO pass 注册，而不是保留 full-screen dispatch 后在 shader 内 early-out。
