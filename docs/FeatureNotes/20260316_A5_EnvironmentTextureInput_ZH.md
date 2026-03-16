# Feature Note - A5.2 环境贴图输入

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260316_A5_EnvironmentTextureInput.md`

- 日期：2026-03-16
- 工作项 ID：A5.2
- 标题：为 lighting pass 增加文件驱动的环境贴图输入
- Owner：AI coding session
- 配套文档：
  - `docs/FeatureNotes/20260316_A5_EnvironmentTextureInput.md`
- 相关计划：
  - `docs/FeatureNotes/20260315_IndirectLightingOptionsSurvey_ZH.md`
  - `docs/FeatureNotes/20260316_A5_EnvironmentLightingFoundation_ZH.md`
- 路径约定：
  - Repo 相对路径以当前目录为根（`C:\glTFRenderer`）。
  - 代码文件通常应从 `glTFRenderer/` 开始；仓库根下的工具脚本使用 `scripts/`。

## 1. 范围

- In scope:
  - 在 `RendererSystemLighting` 中增加文件驱动的环境贴图句柄。
  - 通过 `AddTexture(...)` 把普通 `TextureHandle` 绑定进 lighting compute pass。
  - 在 `RendererModuleLighting.hlsl` 中加入 equirectangular 环境贴图采样。
  - 保留 procedural sky/ground 环境项作为 fallback。
  - 增加环境贴图强度与启用状态的调试控件。
- Out of scope:
  - cubemap 资源类型支持。
  - mip-chain prefilter 生成。
  - BRDF LUT 集成。
  - 运行时贴图热重载或任意路径输入 UI。

## 2. 功能逻辑

- Behavior before:
  - 环境间接光只由 procedural sky/ground 颜色和标量参数驱动。
  - 场景间接光里还没有真实的 texture 资源输入。
- Behavior after:
  - `RendererSystemLighting` 默认会从文件加载环境贴图：
    - `glTFRenderer/glTFResources/Models/Plane/dawn_4k.png`
  - 如果文件不可用，则回退到 1x1 纹理句柄，shader 继续走 procedural gradient 路径。
  - lighting shader 现在可以在下面两条路径间切换：
    - procedural gradient environment，
    - file-backed environment texture sampling。
- Key runtime flow:
  - lighting init 创建 `LightingGlobalBuffer`，并保证环境贴图句柄存在。
  - `BuildLightingPassSetupInfo(...)` 把 `environmentTex` 作为普通 texture resource 绑定进 pass。
  - shader 将世界空间方向转换成 lat-long UV，并采样环境贴图。

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- Core algorithm changes:
  - 新增 `EnsureEnvironmentTexture(...)`，用于创建：
    - 文件驱动环境贴图，或
    - fallback 1x1 纹理。
  - 扩展 `LightingGlobalParams`，加入 `environment_texture_params`。
  - 使用下面的经纬度映射公式采样 equirectangular 环境图：
    - `u = atan2(z, x) / (2*pi) + 0.5`
    - `v = acos(y) / pi`
  - 同时保留 procedural 环境模型，作为稳定 fallback。
- Parameter / model changes:
  - 新增纹理参数块：
    - `environment_texture_params.x`：环境贴图强度
    - `environment_texture_params.z`：是否使用环境贴图

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- `docs/FeatureNotes/20260316_A5_EnvironmentTextureInput.md`
- `docs/FeatureNotes/20260316_A5_EnvironmentTextureInput_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 5. 验证与证据

- Functional checks:
  - `RendererDemo` 构建成功。
  - lighting compute pass 上新的普通纹理绑定路径已通过编译。
- Visual checks:
  - 本轮未执行。
- Performance checks:
  - 本轮未执行。
- Evidence files / links:
  - Build wrapper 摘要：
    - `build_logs/build_verify_20260316_162217.stdout.log`
    - `build_logs/build_verify_20260316_162217.stderr.log`
  - MSBuild 日志：
    - `build_logs/rendererdemo_20260316_162218.msbuild.log`
    - `build_logs/rendererdemo_20260316_162218.stdout.log`
    - `build_logs/rendererdemo_20260316_162218.stderr.log`
    - `build_logs/rendererdemo_20260316_162218.binlog`

## 6. 验收结果

- Status: PASS（build validation scope）
- Acceptance notes:
  - Build 状态：succeeded
  - Warning 数：0
  - Error 数：0
  - 这一阶段把环境光照从“仅参数驱动”推进到了“资源驱动采样”，但还不是完整 IBL 终态。

## 7. 风险与后续

- Known risks:
  - 当前环境贴图仍未经过 irradiance convolution 或 specular prefilter。
  - roughness 响应仍是近似项，因为这一步还没有接 BRDF LUT 和 mip-filtered specular environment。
  - 默认环境源目前仍是代码内固定路径。
- Next tasks:
  - A5.3：增加 irradiance diffuse 与 roughness-aware specular prefilter 资源。
  - A5.4：增加 BRDF integration LUT 支持。
  - A5.5：在 texture 生命周期管理更清晰后，再补环境源选择或运行时配置能力。
