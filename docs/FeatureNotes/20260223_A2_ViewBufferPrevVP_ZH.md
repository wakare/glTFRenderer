# Feature Note - A2.1 ViewBuffer 上一帧 View-Projection 矩阵接线

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260223_A2_ViewBufferPrevVP.md`

- Date: 2026-02-23
- Work Item ID: A2.1
- Title: 为 view buffer 增加 `prev_view_projection_matrix` 及上传路径
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. 范围

- In scope:
  - 为 `ViewBuffer` 增加 `prev_view_projection_matrix`。
  - 每帧从 camera module 上传前一帧矩阵。
  - 在 viewport resize 时重置 previous-frame 状态。
  - 让 shadow view buffer 与新的布局保持一致。
- Out of scope:
  - Velocity buffer 生成。
  - Temporal reprojection 逻辑和 history blending。
  - 任何真正消费 previous-frame matrix 的视觉算法。

## 2. 功能逻辑

- Behavior before:
  - `ViewBuffer` 只提供当前 `view_projection_matrix` 和 inverse matrix。
- Behavior after:
  - `ViewBuffer` 同时携带当前帧和上一帧的 view-projection matrix。
  - 首帧和 resize 边界会用当前矩阵作为 previous fallback，避免无效 history。
- Key runtime flow:
  - Camera module 先计算当前 VP。
  - 上传时使用缓存的 previous VP；若未初始化则回退到 current。
  - 上传后推进缓存。

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `RendererModuleCamera` 的 CPU 侧上传路径。
  - `SceneViewCommon.hlsl` 的 constant buffer 布局。
  - 写 `ViewBuffer` 的 directional shadow setup 路径。
- Core algorithm changes:
  - 这一步只做数据接线，没有新渲染算法。
- Parameter / model changes:
  - 在 C++ 和 HLSL 的 view buffer 定义中都新增 `prev_view_projection_matrix`。

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.h`
- `glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/SceneViewCommon.hlsl`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`

## 5. 验证与证据

- Functional checks:
  - `RendererDemo` verify build 在 ABI / layout 改动后通过。
- Visual checks:
  - 本次迭代未执行（纯 plumbing 变更）。
- Performance checks:
  - 本次迭代未执行。
- Evidence files / links:
  - Build wrapper summary:
    - `build_logs/build_verify_20260223_184547.stdout.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260223_184548.msbuild.log`
    - `build_logs/rendererdemo_20260223_184548.stdout.log`
    - `build_logs/rendererdemo_20260223_184548.stderr.log`
    - `build_logs/rendererdemo_20260223_184548.binlog`

## 6. 验收结果

- Status: PASS（build validation scope）
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 30（既有 warning 基线）
  - Error count: 0

## 7. 风险与后续

- Known risks:
  - 矩阵虽然已上传，但当前还没有 shader 真正使用，因此不会有直接视觉收益。
- Next tasks:
  - A2.2：引入 velocity 或 reprojection 的消费者路径。
  - A2.3：增加 debug 可视化，验证 previous / current matrix 在运动中的行为。
