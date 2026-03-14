# Feature Note - A4 时间历史基础设施

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260224_A4_TemporalHistoryInfrastructure.md`

- Date: 2026-02-24
- Work Item ID: A4
- Title: 为 frosted-glass 增加 temporal history 生命周期、ping-pong 路由和 invalidation 路径
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. 范围

- In scope:
  - 为 frosted composite 增加 full-resolution temporal history render target。
  - 增加逐帧 history ping-pong（`A->B`、`B->A`）以及按帧选择 active node 的逻辑。
  - 在 resize 和 camera jump 时增加 temporal invalidation / reset 路径。
  - 在 frosted composite shader 中增加基于 velocity 的 reprojection 和 rejection 控制。
  - 在 frosted debug UI 中增加 temporal 调试控件。
- Out of scope:
  - Neighborhood clamping 或更高级的 TAA-style history clamp。
  - 在相机 / panel 压力路径下的最终视觉 checklist。
  - 最终性能验收采集。

## 2. 功能逻辑

- Behavior before:
  - Frosted composite 没有 temporal history 输入，也没有 history 生命周期。
  - 对 resize / camera jump 没有专门的 history reset 策略。
- Behavior after:
  - Frosted 现在拥有两张 history RT：
    - `PostFX_Frosted_History_A`
    - `PostFX_Frosted_History_B`
  - 每帧会在两个 composite node 之间切换：
    - 读 `A` 写 `B`
    - 读 `B` 写 `A`
  - Camera module 现在会向 temporal 消费者暴露可读取的 invalidation flag。
  - 在以下情况下会重置 history validity：
    - resize
    - 检测到 camera jump（view-projection delta 较大）
    - 显式 debug reset 按钮
  - Composite shader 现在会同时写出：
    - 最终 frosted 输出
    - 下一帧 history 输出

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `RendererSystemFrostedGlass` 的 pass graph wiring 和运行时状态管理。
  - `RendererModuleCamera` 的 temporal invalidation 信号。
  - `FrostedGlass.hlsl` 中 `CompositeMain` 的 temporal reprojection blend。
- Core algorithm changes:
  - 用 velocity 做 history reprojection：
    - `prev_uv = current_uv - velocity`
  - 用 velocity 和 edge 项拒绝无效 history。
  - 只在有效 panel mask 覆盖区域内混合 history。
  - 在全局参数和上传路径中维护逐帧的 history-valid flag。
- Parameter / model changes:
  - 新增 temporal 全局控件：
    - `temporal_history_blend`
    - `temporal_reject_velocity`
    - `temporal_edge_reject`
    - `temporal_history_valid`

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.h`
- `glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.cpp`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 5. 验证与证据

- Functional checks:
  - A4 接入后 verify build 成功。
- Visual checks:
  - 本次迭代未执行（互动验证待补）。
- Performance checks:
  - 本次迭代未执行（timing 采集待补）。
- Evidence files / links:
  - Build wrapper summary:
    - 仅记录本地工件名，这些文件不入库。
    - `a4_verify_wrapper_20260224_125940.stdout.log`
    - `a4_verify_wrapper_20260224_125940.stderr.log`
  - MSBuild logs:
    - 仅记录本地工件名，这些文件不入库。
    - `rendererdemo_20260224_125940.msbuild.log`
    - `rendererdemo_20260224_125940.stdout.log`
    - `rendererdemo_20260224_125940.stderr.log`
    - `rendererdemo_20260224_125940.binlog`

## 6. 验收结果

- Status: PASS（A4 core implementation scope，build validation）
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 76
  - Error count: 0
  - A4 仍在进行中，直到拿到视觉和性能验收证据。

## 7. 风险与后续

- Known risks:
  - 当前的 camera-jump 检测使用 matrix-delta threshold，可能需要按不同相机运动风格调参。
  - 还没有做 neighborhood color clamp，因此在快速高对比运动下仍可能看到 temporal artifact。
- Next tasks:
  - A4 验收：通过互动 checklist 验证运动稳定性和 resize-reset 行为。
  - M3 跟进：在开启 temporal 的情况下跑 B2 的状态切换场景，并做工件回看。
