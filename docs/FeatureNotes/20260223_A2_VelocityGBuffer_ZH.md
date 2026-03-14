# Feature Note - A2.2 Velocity GBuffer 输出路径（仅生产侧）

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260223_A2_VelocityGBuffer.md`

- Date: 2026-02-23
- Work Item ID: A2.2
- Title: 为 base pass 增加 velocity render target 与 shader motion-vector 输出
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. 范围

- In scope:
  - 为 base pass 增加新的 velocity render target。
  - 扩展 model rendering shader，写出逐像素 motion vector。
  - 通过 `RendererSystemOutput<RendererSystemSceneRenderer>` 暴露 velocity RT handle。
- Out of scope:
  - Temporal history buffer 消费。
  - Motion-vector debug visualization UI。
  - Temporal reprojection 或 denoise 逻辑。

## 2. 功能逻辑

- Behavior before:
  - Base pass 只写 color / normal / depth。
  - 没有专用 velocity 通道。
- Behavior after:
  - Base pass 会写 `color + normal + velocity + depth`。
  - Velocity 编码为 UV-space delta：
    - `velocity_uv = current_uv - previous_uv`
  - Previous position 使用 A2.1 中加入到 `ViewBuffer` 的 `prev_view_projection_matrix`。
- Key runtime flow:
  - VS 用当前 world position 计算 previous-frame clip position。
  - FS 重建 `previous_uv`，并把 velocity 写到 `SV_TARGET2`。

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `RendererSystemSceneRenderer` 的 base pass MRT 布局。
  - `ModelRenderingShader.hlsl` 的 VS / FS 输出。
- Core algorithm changes:
  - 在 base pass 中增加 motion vector 的生产侧输出。
  - VS 同时导出当前和上一帧的 clip-space position。
  - FS 从 NDC delta 计算 velocity，再转换为 UV-space delta（不在 FS 中读取 `ViewBuffer`）。
- Parameter / model changes:
  - 新 RT：`BasePass_Velocity`（`RGBA16_FLOAT`）。
  - 新 FS 输出：`velocity : SV_TARGET2`。

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSceneRenderer.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSceneRenderer.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/ModelRenderingShader.hlsl`

## 5. 验证与证据

- Functional checks:
  - 在新增 MRT 布局和 shader 输出后，`RendererDemo` verify build 通过。
- Visual checks:
  - 本次迭代未执行（仅 producer 路径）。
- Performance checks:
  - 本次迭代未执行。
- Evidence files / links:
  - MSBuild summary:
    - `build_logs/rendererdemo_20260223_185103.msbuild.log`
  - Standard outputs:
    - `build_logs/rendererdemo_20260223_185103.stdout.log`
    - `build_logs/rendererdemo_20260223_185103.stderr.log`
  - Binlog:
    - `build_logs/rendererdemo_20260223_185103.binlog`

## 6. 验收结果

- Status: PASS（build validation scope）
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 7
  - Error count: 0

## 7. 风险与后续

- Known risks:
  - 当前 velocity 仍然对 current / previous position 使用当前帧 instance transform；animated transform history 尚未真正建模。
  - 还没有运行时 debug view 去可视化验证方向和幅值。
- Next tasks:
  - A2.3：增加 velocity debug visualization 和有效性检查。
  - A4.x：在 temporal stabilization 流水线中消费 velocity / history。
