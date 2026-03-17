# Feature Note - A5-H2 IBL 加固阶段 2

配套文档：

- 英文版：
  - `docs/FeatureNotes/20260317_A5_IBLHardeningPhase2.md`

- 日期：2026-03-17
- Work Item ID：A5-H2
- 标题：为运行时纹理补充显式 mip 链上传能力，并把 specular prefilter 收敛成单张 mip 资源
- 负责人：AI coding session
- 配套文档：
  - `docs/FeatureNotes/20260317_A5_IBLHardeningPhase2.md`
- 相关计划：
  - `docs/FeatureNotes/20260317_A5_IBLHardeningPhase1.md`
  - `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder.md`
- 路径约定：
  - 仓库相对路径以当前目录 (`C:\glTFRenderer`) 为根。
  - 代码文件通常以 `glTFRenderer/` 开头；根目录工具脚本使用 `scripts/`。

## 1. 范围

- 本次范围：
  - 扩展 runtime texture 创建接口，使一张纹理可以从 CPU 侧显式提供的 mip payload 初始化。
  - 把 4 张独立的 specular prefilter 纹理替换成 1 张带 mip 的纹理。
  - 保持当前 A5 视觉 baseline 在 DX 与 Vulkan 上不回归。
- 不在本次范围：
  - 本轮不清理剩余的 environment bring-up 控件。
  - 本轮不把环境源路径从当前 LDR 流水线切到 HDR。
  - 本轮不把环境预处理从 CPU 挪到离线或 GPU。

## 2. 为什么这轮要扩框架代码

- 这轮的维护路径诉求已经不只是“在 shader 里少采几次样”。
- 一个干净的 A5-H2 实现需要 renderer 层能把 CPU 侧生成的 prefilter 阶梯直接上传成一张纹理，而不是继续在 feature 层维护 4 个独立句柄。
- 所以这轮有意识地扩了框架层：
  - `RendererInterface::TextureDesc` 现在显式支持 mip payload。
  - `ResourceOperator::CreateTexture(...)` 现在可以分 mip 上传这些数据。
  - `RHIUtils::UploadTextureData(...)` 现在会正确处理紧凑 CPU 图像行宽与 RHI copy footprint 行宽不一致的问题。

## 3. 功能逻辑

- 改动前：
  - `EnvironmentLightingResources` 会创建 4 张独立的 prefilter 纹理。
  - lighting shader 绑定的是 `environmentPrefilterTex0..3`。
  - specular IBL 每像素会把所有 prefilter level 都采一遍，再手动选层和插值。
- 改动后：
  - `EnvironmentLightingResources` 会生成 1 条 prefilter mip 链，当前写入的 authored level 为：
    - `128x64` 对应 roughness `0.25`
    - `64x32` 对应 roughness `0.50`
    - `32x16` 对应 roughness `0.75`
    - `16x8` 对应 roughness `1.00`
  - lighting pass 现在只绑定 1 张 `environmentPrefilterTex`。
  - shader 保留了原有的低 roughness 行为：
    - roughness 落在 `[0.0, 0.25]` 时，仍然从 sharp `environmentTex` 过渡到 mip `0`
  - shader 简化了其余区间：
    - roughness 落在 `(0.25, 1.0]` 时，先映射到 mip LOD，再对单张 prefilter 纹理调用 `SampleLevel(...)`
  - 最终结果是资源绑定面更小，specular IBL 的逐像素代价更低，同时不破坏当前批准的视觉 baseline。

## 4. 框架层影响

- `RendererInterface::TextureDesc` 新增了 `mip_levels`，显式 mip 上传不再是 feature 私有技巧，而是 renderer 的正式能力。
- `ResourceOperator::CreateTexture(...)` 现在支持 3 类运行时纹理创建：
  - 通过 `data` 做单 mip 上传
  - 通过 `mip_levels` 做显式 mip 链上传
  - 不带初始数据的空纹理分配
- `RHIUtils::UploadTextureData(...)` 现在只会把紧凑源图像的实际行宽复制到带 padding 的目标行里。
  - 这修正了之前默认“源行宽总是等于 RHI copy pitch”的假设。
  - 对较小 mip 尤其重要，因为 DX12 的 copy footprint 行宽可能比紧凑图像更宽。

## 5. 变更文件

- `glTFRenderer/RendererCore/Public/Renderer.h`
- `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- `glTFRenderer/RHICore/Private/RHIUtils.cpp`
- `glTFRenderer/RendererDemo/RendererSystem/EnvironmentLightingResources.h`
- `glTFRenderer/RendererDemo/RendererSystem/EnvironmentLightingResources.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
- `docs/FeatureNotes/20260317_A5_IBLHardeningPhase2.md`
- `docs/FeatureNotes/20260317_A5_IBLHardeningPhase2_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 6. 验证与证据

- 功能验证：
  - framework 与 shader 改动落地后，`RendererDemo` 构建成功。
  - 单纹理 prefilter 路径替换掉原来的多纹理 ladder 后，approved-baseline compare 在 DX 与 Vulkan 都通过。
- 视觉验证：
  - approved-baseline compare 覆盖了：
    - `env_texture_ibl_reference`
    - `ibl_off_reference`
    - `procedural_ibl_reference`
  - 最终验收时没有残留视觉 mismatch。
- 性能验证：
  - 这轮把 prefilter SRV 绑定数从 `4` 降到了 `1`。
  - specular IBL shader 不再每像素把所有 prefilter level 都采一遍。
  - 本轮没有新增 perf gate；A5 compare 仍保持 visual-first。
- 证据文件：
  - Build wrapper 摘要：
    - `build_logs/build_a5h2_20260317_160723.stdout.log`
  - MSBuild 日志：
    - `build_logs/rendererdemo_20260317_160724.msbuild.log`
    - `build_logs/rendererdemo_20260317_160724.stdout.log`
    - `build_logs/rendererdemo_20260317_160724.stderr.log`
    - `build_logs/rendererdemo_20260317_160724.binlog`
  - Compare wrapper 摘要：
    - `build_logs/compare_a5h2_baseline_20260317_162401.stdout.log`
  - Compare 输出：
    - `.tmp/regression_compare_a5_model_viewer_lighting/a5cb_20260317_162402/summary.md`

## 7. 验收结果

- 状态：PASS
- 验收说明：
  - A5-H2 现在已经是“框架支持的 mip 链路径”，而不是 feature 层的多 SRV 绕行实现。
  - runtime 绑定面更小，specular IBL 采样路径更轻。
  - 当前批准的 A5 baseline 在 DX 与 Vulkan 双后端都保持通过。

## 8. 剩余风险与后续

- A5-H2 之后仍存在的风险：
  - 临时的 environment bring-up 控件和匿名参数打包仍然存在。
  - 环境源和派生纹理仍局限于当前 LDR 路径。
  - 环境预处理仍然是初始化阶段的 CPU 同步生成。
- 后续任务：
  - A5-H3：把临时 environment bring-up 控件从 runtime-facing 逻辑中隔离或删除。
  - A5-H4：把环境源和派生资源路径继续扩到 HDR 输入与更可扩展的预处理方案。
