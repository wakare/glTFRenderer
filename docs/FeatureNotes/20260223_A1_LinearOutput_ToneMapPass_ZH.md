# Feature Note - A1.1 线性光照输出与独立 Tone Map Pass

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260223_A1_LinearOutput_ToneMapPass.md`

- Date: 2026-02-23
- Work Item ID: A1.1
- Title: 让 lighting 保持线性输出，并新增独立的 tone mapping 终结 pass
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. 范围

- In scope:
  - 移除 scene lighting pass 输出路径中的直接 sRGB 转换。
  - 增加独立的 tone map compute pass 作为最终颜色输出。
  - 把 lighting / frosted 的中间输出升级到 `RGBA16_FLOAT`。
  - 将 tone map system 接入 `DemoAppModelViewer` 运行时流水线。
- Out of scope:
  - 所有系统层面的完整 HDR exposure 控制策略。
  - Temporal stabilization 和 history buffer。
  - 高级 tone-mapping operator 调优和 color grading 流水线。

## 2. 功能逻辑

- Behavior before:
  - `SceneLighting.hlsl` 直接输出 `LinearToSrgb(final_lighting)`。
  - Frosted pass 输出就是最终帧输出。
- Behavior after:
  - Lighting pass 输出线性颜色。
  - Frosted pass 用浮点格式处理线性输入 / 输出。
  - 新的 `RendererSystemToneMap` 在 frosted 之后执行，并成为最终输出。
- Key runtime flow:
  - `Scene -> Lighting(linear) -> Frosted(linear) -> ToneMap(sRGB output) -> Swapchain`

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `SceneLighting.hlsl`：移除内联 sRGB 转换。
  - `FrostedGlass` system 输出格式改为 `RGBA16_FLOAT`。
  - 新增 `ToneMap.hlsl`：包含 exposure + operator 选择（`Reinhard` / `ACES`）+ gamma 输出变换。
- Core algorithm changes:
  - 把 transfer-function 的职责移动到独立的 terminal pass。
- Parameter / model changes:
  - 新增 tone map 全局参数：`exposure`、`gamma`、`tone_map_mode`。

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemToneMap.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemToneMap.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/ToneMap.hlsl`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.h`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`
- `glTFRenderer/RendererDemo/RendererDemo.vcxproj`
- `glTFRenderer/RendererDemo/RendererDemo.vcxproj.filters`

## 5. 验证与证据

- Functional checks:
  - `RendererDemo` 目标构建成功。
  - 新 system 编译通过，并接入运行时 system 列表。
- Visual checks:
  - 本次迭代未执行（仅做 build 验证）。
- Performance checks:
  - 本次迭代未执行（仅做 build 验证）。
- Evidence files / links:
  - Build wrapper summary:
    - `build_logs/build_verify_20260223_183108.stdout.log`
  - MSBuild logs:
    - `build_logs/rendererdemo_20260223_183109.msbuild.log`
    - `build_logs/rendererdemo_20260223_183109.stdout.log`
    - `build_logs/rendererdemo_20260223_183109.stderr.log`
    - `build_logs/rendererdemo_20260223_183109.binlog`

## 6. 验收结果

- Status: PASS（build validation scope）
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 30（既有 warning 类别：`C4099`、`C4267`、`C4244`）
  - Error count: 0

## 7. 风险与后续

- Known risks:
  - 尚未在运行时验证视觉回归（需要截图 / 感知检查）。
  - Tone map 默认值可能仍需要按场景调优。
- Next tasks:
  - A1.2：做运行时视觉验证并更新基线截图。
  - A1.3：对照验收目标评估 tone-map operator 和 exposure 默认值。
