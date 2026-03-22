# SSAO 视空间投影修正

## 摘要

在这轮 SSAO 排查中，`Valid Sample Ratio` 调试视图几乎整帧发黑，说明 `ComputeSSAO(...)` 在进入遮挡累积前就把绝大多数 sample 过滤掉了。

这次修改把之前的“简化版 view-space 重建与重投影”替换成了基于 `inverse_projection_matrix` 和 `projection_matrix` 的矩阵路径。

## 现象

设置：

- `SSAO -> Debug Output = Valid Sample Ratio`
- `Tone Map -> Debug View = SSAO`

结果几乎整屏为黑色，意味着大多数像素的 `valid_sample_count` 长期接近 `0`。

## 高概率原因

上一版优化后的 SSAO evaluate 依赖了一套基于投影标量的简化公式。这条路径相比原始矩阵重建更脆弱，一旦和运行时相机投影约定或 backend 的 clip-space 细节有偏差，就会导致 sample 验证链路整体失效。

即使理论推导本身没问题，只要在符号约定、深度解释或重投影坐标上有一点点不一致，都足以让 SSAO 的有效 sample 数量几乎归零。

## 修改内容

- `GetViewPositionFromUV(...)` 改为通过 `inverse_projection_matrix` 重建 view position
- `ProjectViewPositionToUV(...)` 改为通过 `projection_matrix` 做重投影
- evaluate 阶段在取到 sample texel 后，使用实际 sample texel 的位置重建 view position，而不是继续沿用中间浮点 UV

## 预期结果

这样可以让 SSAO evaluate 路径严格跟随 camera buffer 里的实际投影约定，避免解析近似公式带来的 backend 敏感性问题。

## 验证

- 验证构建通过
- Binlog：`build_logs/rendererdemo_20260320_074943.binlog`

## 下一步检查

重新执行：

1. `SSAO -> Debug Output = Valid Sample Ratio`
2. `Tone Map -> Debug View = SSAO`

如果图像不再接近全黑，说明问题确实出在 view-space 重建或投影路径。

如果仍然接近全黑，下一步重点排查：

- `along_ray` / `along_normal` 的过滤阈值
- sample direction basis 构造
- normal 空间约定是否不一致
