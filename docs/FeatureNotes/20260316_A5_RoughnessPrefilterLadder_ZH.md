# Feature Note - A5.5 Roughness Prefilter Ladder

配套文档：

- 英文配套：
  - `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder.md`

- 日期：2026-03-16
- 工作项 ID：A5.5
- 标题：将单张 specular-prefilter 近似替换为 roughness-aware prefilter 梯度
- 负责人：AI coding session
- 配套说明：
  - `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder.md`
- 相关计划：
  - `docs/FeatureNotes/20260315_IndirectLightingOptionsSurvey_ZH.md`
  - `docs/FeatureNotes/20260316_A5_BrdfIntegrationLut_ZH.md`
- 路径约定：
  - 仓库相对路径以当前目录（`C:\glTFRenderer`）为根。
  - 代码文件通常以 `glTFRenderer/` 开头；根目录脚本使用 `scripts/`。

## 1. 范围

- 本次范围：
  - 将此前单张预滤波环境图替换为一组 roughness 梯度 level。
  - 在 lighting compute pass 中绑定多张 prefilter 纹理。
  - 在 lighting global constant buffer 中加入显式 roughness 阈值。
  - 在 shader 中按表面 roughness 选择并插值 prefilter level。
- 不在本次范围：
  - 真正的 cubemap 或 mip-chain prefilter 存储。
  - GPU 侧 prefilter 生成。
  - 运行时逐级分辨率或 roughness 数值编辑 UI。

## 2. 功能逻辑

- 修改前：
  - specular indirect 使用一张模糊 prefilter 贴图，再按 roughness 从清晰环境图向它混合。
  - A5.4 已经补了 BRDF LUT，但环境预滤波仍然只代表一种模糊尺度。
- 修改后：
  - lighting 现在会在 CPU 侧生成四张不同 roughness 的 prefilter 贴图。
  - shader 会在以下来源之间进行选择：
    - 清晰源环境图，
    - 四张 prefilter level，
    - 相邻 level 之间的线性插值。
  - roughness 阈值通过 `environment_prefilter_roughness` 传入。
- 关键运行流程：
  - `EnsureDerivedEnvironmentTextures(...)` 现在会创建：
    - 一张 irradiance 纹理，
    - 四张 specular prefilter 纹理。
  - `BuildLightingPassSetupInfo(...)` 现在会绑定：
    - `environmentPrefilterTex0`
    - `environmentPrefilterTex1`
    - `environmentPrefilterTex2`
    - `environmentPrefilterTex3`
  - `SampleEnvironmentPrefilter(...)` 负责选择并插值正确的 level。

## 3. 算法与渲染逻辑

- 涉及 pass / shader：
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- 核心算法改动：
  - CPU prefilter 梯度：
    - roughness level：`0.25`、`0.50`、`0.75`、`1.00`，
    - 分辨率：`128x64`、`64x32`、`32x16`、`16x8`，
    - 生成方法：沿用现有 lat-long cone-sampling prefilter，并按 level 重复生成。
  - Constant buffer 更新：
    - 在 `LightingGlobalBuffer` 中增加 `environment_prefilter_roughness`。
  - Shader 集成：
    - 当 roughness 低于第一个 level 时，从清晰环境图插值到 level 0，
    - 否则在相邻 prefilter level 之间插值，
    - A5.4 的 BRDF LUT 采样逻辑保持不变。
- 参数/模型变化：
  - `LightingGlobalParams` 从 `80` 字节增加到 `96` 字节。
  - 内部 prefilter 存储从单个 texture handle 变为一组 texture 列表。

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder.md`
- `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 5. 验证与证据

- 功能检查：
  - `RendererDemo` 构建成功。
  - 多级 prefilter 资源路径已通过 C++ 与 HLSL 绑定编译验证。
- 视觉检查：
  - 本轮未执行。
- 性能检查：
  - 本轮未执行。
- 证据文件/链接：
  - Build wrapper 摘要：
    - `build_logs/build_verify_20260316_165328.stdout.log`
    - `build_logs/build_verify_20260316_165328.stderr.log`
  - MSBuild 日志：
    - `build_logs/rendererdemo_20260316_165330.msbuild.log`
    - `build_logs/rendererdemo_20260316_165330.stdout.log`
    - `build_logs/rendererdemo_20260316_165330.stderr.log`
    - `build_logs/rendererdemo_20260316_165330.binlog`

## 6. 验收结果

- 状态：PASS（构建验证范围）
- 验收说明：
  - Build 状态：succeeded
  - Warning 数量：0
  - Error 数量：0
  - specular indirect 不再只受限于单一代表性 prefilter 模糊级别。

## 7. 风险与后续

- 已知风险：
  - 这些 prefilter level 仍然是分离的 lat-long 纹理，不是真正的 cubemap mip 层级。
  - 由于每个 level 都在初始化时 CPU 生成，启动成本进一步增加。
  - 运行时视觉验证仍未完成。
- 后续任务：
  - A5.6：评估当前 CPU 生成梯度是否只作为 bootstrap，还是需要迁移到 GPU 预处理。
  - A5.7：增加 rough metal 和 grazing 高光的运行时截图验证。
  - A5.8：评估环境源是否需要从 lat-long 贴图迁移到更适合 cubemap 的存储形式。
