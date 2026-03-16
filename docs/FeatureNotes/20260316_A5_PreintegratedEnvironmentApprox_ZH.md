# Feature Note - A5.3 预积分环境光近似资源

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260316_A5_PreintegratedEnvironmentApprox.md`

- 日期：2026-03-16
- 工作项 ID：A5.3
- 标题：增加 CPU 生成的 irradiance 与 specular-prefilter 环境纹理
- Owner：AI coding session
- 配套文档：
  - `docs/FeatureNotes/20260316_A5_PreintegratedEnvironmentApprox.md`
- 相关计划：
  - `docs/FeatureNotes/20260315_IndirectLightingOptionsSurvey_ZH.md`
  - `docs/FeatureNotes/20260316_A5_EnvironmentTextureInput_ZH.md`
- 路径约定：
  - Repo 相对路径以当前目录为根（`C:\glTFRenderer`）。
  - 代码文件通常应从 `glTFRenderer/` 开始；仓库根下的工具脚本使用 `scripts/`。

## 1. 范围

- In scope:
  - 在 CPU 侧加载默认环境图。
  - 生成一张低频 irradiance lat-long 纹理。
  - 生成一张模糊的 specular-prefilter lat-long 纹理。
  - 把两张派生纹理绑定进 lighting compute pass。
  - 在 shader 中让 diffuse indirect 和 roughness-aware specular indirect 使用这些派生纹理。
- Out of scope:
  - GPU 侧 convolution 或 prefilter pass。
  - cubemap 表示。
  - 基于 mip 的 roughness 级联。
  - BRDF integration LUT。

## 2. 功能逻辑

- Behavior before:
  - lighting 已经可以直接采样文件驱动的环境贴图。
  - diffuse 和 specular indirect 仍主要依赖直接环境采样或 procedural fallback。
- Behavior after:
  - 当源环境图可用时，lighting 现在会创建三类环境资源：
    - source environment texture，
    - irradiance texture，
    - specular-prefilter texture。
  - diffuse indirect 使用 irradiance 纹理。
  - specular indirect 会根据 roughness 在 sharp source radiance 和 prefiltered radiance 之间插值。
  - 当源图不可用时，三条路径都会回退到 1x1 纹理，procedural environment 仍可继续使用。
- Key runtime flow:
  - `RendererSystemLighting::EnsureEnvironmentTexture(...)` 负责加载源图并创建源纹理。
  - `EnsureDerivedEnvironmentTextures(...)` 在 CPU 上生成派生纹理。
  - `BuildLightingPassSetupInfo(...)` 绑定：
    - `environmentTex`
    - `environmentIrradianceTex`
    - `environmentPrefilterTex`

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- Core algorithm changes:
  - 在 CPU 上把源环境图解码为 linear RGB。
  - irradiance 纹理生成：
    - 输出为低分辨率 lat-long 纹理，
    - 方法是围绕目标 normal 做 cosine-weighted hemisphere sampling。
  - specular-prefilter 纹理生成：
    - 输出为模糊 lat-long 纹理，
    - 方法是围绕目标 reflection direction 做固定 roughness 的 cone sampling。
  - shader 集成方式：
    - diffuse indirect 采样 irradiance 纹理，
    - specular indirect 用 `roughness^2` 在源 radiance 与 prefiltered radiance 之间插值。
- Parameter / model changes:
  - 没有新增用户侧可调的标量参数，继续沿用现有环境贴图控制项。
  - 新增的内部纹理句柄包括：
    - source environment，
    - irradiance，
    - prefilter。

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- `docs/FeatureNotes/20260316_A5_PreintegratedEnvironmentApprox.md`
- `docs/FeatureNotes/20260316_A5_PreintegratedEnvironmentApprox_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 5. 验证与证据

- Functional checks:
  - `RendererDemo` 构建成功。
  - 新的派生环境纹理路径已经在 C++ 与 HLSL 两侧通过编译。
- Visual checks:
  - 本轮未执行。
- Performance checks:
  - 本轮未执行。
- Evidence files / links:
  - Build wrapper 摘要：
    - `build_logs/build_verify_20260316_163336.stdout.log`
    - `build_logs/build_verify_20260316_163336.stderr.log`
  - MSBuild 日志：
    - `build_logs/rendererdemo_20260316_163336.msbuild.log`
    - `build_logs/rendererdemo_20260316_163336.stdout.log`
    - `build_logs/rendererdemo_20260316_163336.stderr.log`
    - `build_logs/rendererdemo_20260316_163336.binlog`

## 6. 验收结果

- Status: PASS（build validation scope）
- Acceptance notes:
  - Build 状态：succeeded
  - Warning 数：0
  - Error 数：0
  - 这一阶段增加了近似的预积分环境资源，但还没有进入最终 split-sum BRDF 终态。

## 7. 风险与后续

- Known risks:
  - CPU 生成派生纹理属于近似方案；如果源图尺寸或采样数继续增大，初始化成本会上升。
  - specular prefilter 当前仍是一张代表性模糊纹理，不是完整的 roughness/mip 金字塔。
  - 在没有 BRDF LUT 的情况下，金属与 grazing 响应仍然是简化的。
- Next tasks:
  - A5.4：增加 BRDF integration LUT 支持。
  - A5.5：决定 CPU 预积分只保留为 bootstrap，还是替换成 GPU 环境预处理路径。
  - A5.6：增加运行时截图/回归验证，用于调优派生纹理质量与成本。
