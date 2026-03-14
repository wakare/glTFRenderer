# Feature Note - A2.3 Velocity Debug View 路径

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260223_A2_VelocityDebugView.md`

- Date: 2026-02-23
- Work Item ID: A2.3
- Title: 为 motion-vector 有效性检查增加 velocity 可视化模式
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. 范围

- In scope:
  - 在最终 tone-map 阶段增加一个 debug-view 模式，用于可视化 base-pass velocity。
  - 把 `BasePass_Velocity` 作为可选 debug 输入绑定到 tone-map pass。
  - 增加 debug mode 和 velocity scale 的运行时 UI 控件。
- Out of scope:
  - Temporal reprojection / history 消费。
  - Velocity 后处理（dilate / filter）。
  - 自动截图基线更新。

## 2. 功能逻辑

- Behavior before:
  - Tone-map pass 只消费 frosted final color，并输出显示颜色。
  - Velocity buffer 虽然已在 A2.2 中存在，但没有可视化路径。
- Behavior after:
  - Tone-map pass 支持 `Debug View = Velocity`。
  - 在 velocity 模式下，输出颜色编码为：
    - R / G：围绕中性灰表示有符号 UV motion 的方向和幅值
    - B：motion speed intensity
  - `Velocity Scale` 用于放大可视化效果，便于观察运动向量。

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `RendererSystemToneMap` 的 pass binding 和 UI 控件。
  - `ToneMap.hlsl` 的 debug 分支。
- Core algorithm changes:
  - 在 tone-map compute shader 中新增 debug 分支：
    - 采样 `InputVelocityTex.xy`
    - 按 `debug_velocity_scale` 做缩放
    - 映射到 debug color，并绕过 tone-map operator 分支
- Parameter / model changes:
  - `ToneMapGlobalParams` 新增：
    - `debug_view_mode`（`0=Final`、`1=Velocity`）
    - `debug_velocity_scale`

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemToneMap.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemToneMap.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/ToneMap.hlsl`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`

## 5. 验证与证据

- Functional checks:
  - 在新增 tone-map binding 和 constant-buffer layout 改动后，verify build 成功。
- Visual checks:
  - 本次迭代未执行（仅 build 验证范围）。
- Performance checks:
  - 本次迭代未执行。
- Evidence files / links:
  - MSBuild summary:
    - `build_logs/rendererdemo_20260223_194613.msbuild.log`
  - Standard outputs:
    - `build_logs/rendererdemo_20260223_194613.stdout.log`
    - `build_logs/rendererdemo_20260223_194613.stderr.log`
  - Binlog:
    - `build_logs/rendererdemo_20260223_194613.binlog`

## 6. 验收结果

- Status: PASS（build validation scope）
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 30
  - Error count: 0

## 7. 风险与后续

- Known risks:
  - 本记录未验证运行时的视觉正确性。
  - Velocity 可视化依赖 UV-space 约定；后续 temporal pass 必须使用相同约定。
- Next tasks:
  - A2 收尾：在相机运动下补 runtime visual validation 截图 / checklist。
  - A4.x：在 history 路径和 rejection 逻辑中真正消费 velocity。
