# Feature Note - B6.2 自适应 Multilayer 精修

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260224_B6_AdaptiveMultilayerRefinement.md`

- Date: 2026-02-24
- Work Item ID: B6.2
- Title: 优化 layered composite 顺序，并为 `auto` 模式增加运行时预算回退
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
  - `docs/FeatureNotes/20260224_B6_Top2MultilayerCore.md`

## 1. 范围

- In scope:
  - 调整 multilayer composite 行为，减少 overlap 区域中“front layer 比 back layer 更清晰”的伪影。
  - 为 multilayer 路径增加运行时预算感知的 fallback。
  - 增加用于观察预算与运行态的 debug UI。
- Out of scope:
  - 对所有场景类型做最终 artifact 调优。
  - 完整的 GPU-time 驱动动态质量自适应。

## 2. 功能逻辑

- Behavior before:
  - Front 和 back 的 frosted color 都是基于原始 scene color 分别计算再混合。
  - 在部分 overlap 情况下，front layer 可能会视觉上比预期更锐。
  - `auto` 模式没有运行时预算回退。
- Behavior after:
  - Multilayer 路径现在先评估 back layer，再以前一个 back-composited 结果为底评估 front layer。
  - 这减少了 top layer 比底层更清晰的反常情况。
  - `auto` 模式新增预算回退：
    - 会跟踪 over-budget streak
    - 当持续超预算时，会在 cooldown 帧数内禁用 multilayer
  - 运行时状态会显示在 debug UI 中。

## 3. 算法与运行时变化

- Composite refinement:
  - `back_composited_color = blend(scene, back_layer)`
  - 随后再以 `back_composited_color` 作为 base input 重新评估 `front_layer`
- Adaptive runtime guard:
  - 新增参数 / 状态：
    - `multilayer_runtime_enabled`
    - `multilayer_frame_budget_ms`
    - over-budget streak counter + cooldown frame counter
  - 模式行为：
    - `single`：始终关闭
    - `multi_layer`：始终开启
    - `auto`：按 overlap 与 runtime budget 联合控制

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 5. 验证与证据

- Functional checks:
  - B6.2 接入后 verify build 成功。
- Visual checks:
  - 互动视觉确认待补（用户可见验证步骤尚未执行）。
- Performance checks:
  - Runtime fallback counter 已暴露；完整 timing 验收仍待补。
- Evidence files / links:
  - Build wrapper summary:
    - 仅记录本地工件名，这些文件不入库。
    - `b62_verify_wrapper_20260224_154326.stdout.log`
    - `b62_verify_wrapper_20260224_154326.stderr.log`
  - MSBuild logs:
    - 仅记录本地工件名，这些文件不入库。
    - `rendererdemo_20260224_154327.msbuild.log`
    - `rendererdemo_20260224_154327.stdout.log`
    - `rendererdemo_20260224_154327.stderr.log`
    - `rendererdemo_20260224_154327.binlog`

## 6. 验收结果

- Status: PASS（B6.2 core implementation scope，build validation）
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 146
  - Error count: 0
  - B6 整体仍在进行中，直到获得视觉 / 性能验收签字。

## 7. 风险与后续

- Known risks:
  - 当前 frame-time proxy（`interval`）仍是 CPU 侧估算，不一定能精确代表 GPU 压力。
  - 对高对比 overlap-heavy 场景，仍可能需要更多艺术调参。
- Next tasks:
  - B6 验收：在 overlap-heavy 运动场景中运行，调节默认值（`back weight`、`overlap threshold`、`budget ms`）。
  - 可选：增加基于 GPU timing 的质量切换，以提高 runtime fallback 的准确性。
