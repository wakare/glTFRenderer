# Feature Note - A5.4 BRDF 积分 LUT

配套文档：

- 英文配套：
  - `docs/FeatureNotes/20260316_A5_BrdfIntegrationLut.md`

- 日期：2026-03-16
- 工作项 ID：A5.4
- 标题：为 split-sum specular IBL 增加 CPU 生成的 BRDF 积分 LUT
- 负责人：AI coding session
- 配套说明：
  - `docs/FeatureNotes/20260316_A5_BrdfIntegrationLut.md`
- 相关计划：
  - `docs/FeatureNotes/20260315_IndirectLightingOptionsSurvey_ZH.md`
  - `docs/FeatureNotes/20260316_A5_PreintegratedEnvironmentApprox_ZH.md`
- 路径约定：
  - 仓库相对路径以当前目录（`C:\glTFRenderer`）为根。
  - 代码文件通常以 `glTFRenderer/` 开头；根目录脚本使用 `scripts/`。

## 1. 范围

- 本次范围：
  - 在 lighting system 初始化阶段于 CPU 侧生成 2D BRDF 积分 LUT。
  - 将 LUT 绑定进 lighting compute pass。
  - 用 split-sum LUT 项替换当前 specular indirect 的经验系数。
  - 通过现有 lighting globals 向 shader 传递预滤波环境图的代表 roughness。
- 不在本次范围：
  - 多 mip 或 cubemap 的 prefilter 资源生成。
  - GPU 侧 BRDF LUT 烘焙。
  - 运行时环境源切换 UI。

## 2. 功能逻辑

- 修改前：
  - specular indirect 会在清晰环境辐亮度和模糊环境辐亮度之间做混合。
  - Fresnel、grazing 和 roughness 响应仍然依赖手工经验系数。
- 修改后：
  - lighting 额外生成一个 `environmentBrdfLutTex` 资源。
  - specular indirect 按 `(NdotV, roughness)` 采样 LUT。
  - shader 输出改为使用：
    - prefiltered environment radiance，
    - LUT 给出的 `F0 * A + B`，
    - 现有 specular intensity 控制。
  - 当前单张 prefilter 贴图仍然保留，但其代表 roughness 现在通过 `environment_texture_params.y` 显式传入 shader。
- 关键运行流程：
  - `RendererSystemLighting::EnsureEnvironmentBrdfLut(...)` 负责一次性创建 LUT 纹理。
  - `BuildLightingPassSetupInfo(...)` 负责绑定 `environmentBrdfLutTex`。
  - `GetEnvironmentSpecularLighting(...)` 负责采样 LUT 并应用 split-sum 项。

## 3. 算法与渲染逻辑

- 涉及 pass / shader：
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- 核心算法改动：
  - BRDF LUT 生成：
    - 输出：`128 x 128` 的 2D 纹理，存储格式为 `RGBA16_FLOAT`，
    - 方法：在 CPU 侧对 `(NdotV, roughness)` 做 GGX importance sampling 积分，
    - 存储通道：
      - `R`：scale 项 `A`，
      - `G`：bias 项 `B`。
  - Shader 集成：
    - LUT 采样坐标为 `float2(NdotV, roughness)`，
    - specular BRDF 项改为 `F0 * A + B`，
    - 环境辐亮度仍按编码后的代表 roughness 在清晰源图和固定 prefilter 图之间混合。
- 参数/模型变化：
  - 这一步没有增加新的调试滑条。
  - `environment_texture_params.y` 现在承载 prefiltered environment texture 的代表 roughness。
  - 新增的内部纹理句柄：
    - BRDF LUT。

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- `docs/FeatureNotes/20260316_A5_BrdfIntegrationLut.md`
- `docs/FeatureNotes/20260316_A5_BrdfIntegrationLut_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 5. 验证与证据

- 功能检查：
  - `RendererDemo` 构建成功。
  - 新增 LUT 创建、绑定和 shader 使用路径均已通过 C++ 与 HLSL 编译。
- 视觉检查：
  - 本轮未执行。
- 性能检查：
  - 本轮未执行。
- 证据文件/链接：
  - Build wrapper 摘要：
    - `build_logs/build_verify_20260316_164423.stdout.log`
    - `build_logs/build_verify_20260316_164423.stderr.log`
  - MSBuild 日志：
    - `build_logs/rendererdemo_20260316_164425.msbuild.log`
    - `build_logs/rendererdemo_20260316_164425.stdout.log`
    - `build_logs/rendererdemo_20260316_164425.stderr.log`
    - `build_logs/rendererdemo_20260316_164425.binlog`

## 6. 验收结果

- 状态：PASS（构建验证范围）
- 验收说明：
  - Build 状态：succeeded
  - Warning 数量：0
  - Error 数量：0
  - specular indirect 已改为使用 BRDF LUT，而不是上一阶段那组经验 visibility / grazing 系数。

## 7. 风险与后续

- 已知风险：
  - 现在的 specular prefilter 仍然只有单张代表性模糊图，不是完整 roughness mip ladder。
  - BRDF LUT 仍在 CPU 侧生成，会增加初始化成本。
  - 还没有执行运行时视觉验证。
- 后续任务：
  - A5.5：把单 prefilter 近似替换为 roughness-aware mip 或多级 prefilter 路径。
  - A5.6：评估 CPU 环境预处理是否只作为 bootstrap，还是迁移到 GPU。
  - A5.7：增加 metal 和 grazing 视角响应的运行时截图验证。
