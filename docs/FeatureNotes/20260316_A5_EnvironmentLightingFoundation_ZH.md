# Feature Note - A5.1 环境光照基础阶段

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260316_A5_EnvironmentLightingFoundation.md`

- 日期：2026-03-16
- 工作项 ID：A5.1
- 标题：为场景光照 pass 增加环境光照基础能力
- Owner：AI coding session
- 配套文档：
  - `docs/FeatureNotes/20260316_A5_EnvironmentLightingFoundation.md`
- 相关计划：
  - `docs/FeatureNotes/20260315_IndirectLightingOptionsSurvey_ZH.md`
- 路径约定：
  - Repo 相对路径以当前目录为根（`C:\glTFRenderer`）。
  - 代码文件通常应从 `glTFRenderer/` 开始；仓库根下的工具脚本使用 `scripts/`。

## 1. 范围

- In scope:
  - 把 `SceneLighting.hlsl` 里的硬编码 ambient 占位项替换成结构化环境间接光项。
  - 在 `RendererSystemLighting` 中增加最小环境光照参数缓冲。
  - 将间接光拆分为 diffuse 与 specular 两类环境贡献。
  - 让 SSAO 只作用在 diffuse indirect 上。
  - 增加环境光照基础阶段的调试 UI。
- Out of scope:
  - HDR sky / cubemap 资源导入。
  - irradiance convolution、prefiltered specular IBL、BRDF LUT 生成。
  - reflection probes、probe volumes 或 screen-space GI。
  - 运行时 skybox 渲染。

## 2. 功能逻辑

- Behavior before:
  - `SceneLighting.hlsl` 先算 direct light，再加上一项乘了 SSAO 的硬编码 ambient 颜色。
  - 这项 ambient 占位并没有区分 diffuse indirect 和 specular indirect。
- Behavior after:
  - `SceneLighting.hlsl` 改为计算：
    - direct lighting，
    - diffuse environment lighting，
    - specular environment lighting。
  - SSAO 只调制 diffuse indirect。
  - 环境光照由 `RendererSystemLighting` 持有的运行时参数控制。
- Key runtime flow:
  - `RendererSystemLighting` 上传环境光照参数。
  - `SceneLighting.hlsl` 通过 `LightingGlobalBuffer` 读取这些参数。
  - Shader 用半球 sky / ground 近似来评估 diffuse 和 specular 的环境间接光。

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
  - `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- Core algorithm changes:
  - 新增专用 lighting constant buffer，用于承载环境光照控制参数。
  - 用下面三部分替换原来的 placeholder ambient：
    - 基于法线方向的 diffuse environment response，
    - 基于反射方向的 specular environment response。
  - 当前环境模型刻意保持简单，先使用 sky zenith / horizon / ground 的半球近似。
  - 这一阶段的目标是先把 IBL 所需的结构与语义走通，后续再接完整资源。
- Parameter / model changes:
  - 新增 lighting global params：
    - `sky_zenith_color`
    - `sky_horizon_color`
    - `ground_color`
    - `environment_control`
  - `environment_control` 内包含：
    - diffuse intensity，
    - specular intensity，
    - horizon falloff exponent，
    - enable flag。

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- `docs/FeatureNotes/20260316_A5_EnvironmentLightingFoundation.md`
- `docs/FeatureNotes/20260316_A5_EnvironmentLightingFoundation_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 5. 验证与证据

- Functional checks:
  - `RendererDemo` 构建成功。
  - 新增的 `LightingGlobalBuffer` 链路已通过 `RendererSystemLighting` 和 `SceneLighting.hlsl` 编译。
- Visual checks:
  - 本轮未执行。
- Performance checks:
  - 本轮未执行。
- Evidence files / links:
  - Build wrapper 摘要：
    - `build_logs/build_verify_20260316_160600.stdout.log`
    - `build_logs/build_verify_20260316_160600.stderr.log`
  - MSBuild 日志：
    - `build_logs/rendererdemo_20260316_160601.msbuild.log`
    - `build_logs/rendererdemo_20260316_160601.stdout.log`
    - `build_logs/rendererdemo_20260316_160601.stderr.log`
    - `build_logs/rendererdemo_20260316_160601.binlog`
  - 文档校验日志：
    - `build_logs/validate_docs_20260316_160844.stdout.log`
    - `build_logs/validate_docs_20260316_160844.stderr.log`

## 6. 验收结果

- Status: PASS（build validation scope）
- Acceptance notes:
  - Build 状态：succeeded
  - Warning 数：608
  - Error 数：0
  - `scripts/Validate-DocReferences.ps1` 报出的失败项来自本特性范围之外的历史归档日志与环境路径引用缺失。
  - 这份记录对应的是 IBL 的基础阶段，而不是完整的资源驱动 IBL 终态。

## 7. 风险与后续

- Known risks:
  - 半球环境模型虽然修正了光照语义，但仍不是 asset-driven IBL。
  - 在接入 prefiltered environment 与 BRDF LUT 之前，specular indirect 仍是近似项。
- Next tasks:
  - A5.2：增加资源驱动的环境输入源。
  - A5.3：增加 irradiance diffuse 与 prefiltered specular IBL 资源。
  - A5.4：在 IBL 资源落地后，再决定 SSGI 还是 probe GI 作为下一档动态间接光阶段。
