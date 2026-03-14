# Feature Note - B2 Panel 状态机与运行时控制

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260224_B2_PanelStateMachineRuntimeControl.md`

- Date: 2026-02-24
- Work Item ID: B2
- Title: 增加 panel 交互状态机、运行时状态曲线和输入驱动状态切换
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. 范围

- In scope:
  - 增加 panel 交互状态：`Idle`、`Hover`、`Grab`、`Move`、`Scale`。
  - 为每个 panel 增加 state-curve 模型，用于驱动 blur / rim / fresnel / alpha。
  - 增加运行时 transition smoothing 和逐帧状态混合。
  - 在 demo 侧为 panel `0` 增加输入映射，驱动状态切换。
  - 暴露状态、transition speed 和 per-state curve 的 debug UI 控件。
- Out of scope:
  - 多 panel 的 hit-test 和选择路由。
  - 完整的 panel transform manipulation 交互语义。
  - 最终视觉验收抓图（截图 + 运动 checklist）。

## 2. 功能逻辑

- Behavior before:
  - Frosted panel 只有静态参数。
  - 没有显式的交互状态模型。
  - 运行时不会根据交互状态切换去驱动 blur / rim / fresnel / alpha。
- Behavior after:
  - Panel descriptor 现在包含：
    - target interaction state
    - transition speed
    - panel alpha
    - per-state curve set
  - 运行时维护 blended state weight，并按平滑指数过渡逐帧更新。
  - GPU 上传路径现在会把 blended state curve 应用到：
    - blur sigma
    - blur strength
    - rim intensity
    - fresnel intensity
    - panel alpha
  - Demo 运行时输入映射（panel `0`）：
    - mouse move -> `Hover`
    - `LMB` -> `Move`
    - `RMB` -> `Grab`
    - `Ctrl + LMB` -> `Scale`
    - 否则 -> `Idle`
- Key runtime flow:
  - `Input -> target state -> blended state weights -> panel GPU parameters -> mask/composite shading`

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `RendererSystemFrostedGlass` 的运行时状态更新和 panel 上传转换。
  - `FrostedGlass.hlsl` 在 mask stage 中把 `shape_info.w` 用作 panel alpha。
- Core algorithm changes:
  - 引入状态权重混合：
    - `weight += (target - weight) * (1 - exp(-speed * dt))`
  - 对 blended per-state curve 做归一化，并在 GPU 参数打包时应用。
  - Mask stage 现在会用运行时 panel alpha 乘 panel SDF mask。
- Parameter / model changes:
  - 新增 panel interaction 模型（`PanelInteractionState`、`PanelStateCurve`）。
  - 新增 interaction state 和 curve tuning 的 debug 控件。
  - Demo 开关：启用 / 禁用 input-driven state machine。

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.h`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`
- `scripts/Build-RendererDemo-Verify.ps1`

## 5. 验证与证据

- Functional checks:
  - 接入 B2 后 verify build 成功。
  - Build 脚本现在会显式报告 PID / duration / status，避免与 Rider 后台 MSBuild 混淆。
- Visual checks:
  - 本次迭代未执行（互动验证待补）。
- Performance checks:
  - 本次迭代未执行（timing 采集待补）。
- Evidence files / links:
  - Build wrapper summary:
    - 仅记录本地工件名，这些文件不入库。
    - `b2_verify_wrapper2_20260224_123446.stdout.log`
    - `b2_verify_wrapper2_20260224_123446.stderr.log`
  - MSBuild logs:
    - 仅记录本地工件名，这些文件不入库。
    - `rendererdemo_20260224_123446.msbuild.log`
    - `rendererdemo_20260224_123446.stdout.log`
    - `rendererdemo_20260224_123446.stderr.log`
    - `rendererdemo_20260224_123446.binlog`

## 6. 验收结果

- Status: PASS（B2 core implementation scope，build validation）
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 0
  - Error count: 0
  - B2 仍在进行中，直到补齐运行时视觉稳定性 checklist。

## 7. 风险与后续

- Known risks:
  - 当前 demo 输入映射只驱动 panel `0`，还没有完整的多 panel hit testing。
  - Overlap / selection 行为尚未最终确定，后续可能需要和 `B3` layering 规则进一步对齐。
- Next tasks:
  - B2 验收：记录交互 / 运动场景下的平滑过渡证据。
  - B2 扩展：增加多 panel 的目标选择策略。
  - M2 / M3 跟进：与 `A4` temporal stabilization 联合验证 flicker 行为。
