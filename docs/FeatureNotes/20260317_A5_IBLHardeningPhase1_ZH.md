# 功能记录 - A5-H1 IBL 加固阶段 1

配套文档：

- 英文配套：
  - `docs/FeatureNotes/20260317_A5_IBLHardeningPhase1.md`

- 日期：2026-03-17
- 工作项 ID：A5-H1
- 标题：记录 IBL 加固 review 结论，并把 environment-lighting 资源从 `RendererSystemLighting` 中抽离
- 负责人：AI coding session
- 配套说明：
  - `docs/FeatureNotes/20260317_A5_IBLHardeningPhase1.md`
- 相关计划：
  - `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder_ZH.md`
  - `docs/FeatureNotes/20260316_A5_ModelViewerRegressionValidation_ZH.md`
- 路径约定：
  - 仓库相对路径以当前目录（`C:\glTFRenderer`）为根。
  - 代码文件通常以 `glTFRenderer/` 开头；根目录脚本使用 `scripts/`。

## 1. 范围

- 本次范围：
  - 把当前 IBL review 的结论整理成明确的后续实现顺序。
  - 完成第一项加固任务：将 environment-lighting 资源生命周期与绑定逻辑从 `RendererSystemLighting` 中抽出。
  - 保持现有 A5 运行时行为和验证入口不被破坏。
- 不在本次范围：
  - 本轮不把 roughness ladder 直接替换成真正的 mip chain 或 texture array。
  - 本轮不删除剩余的临时 debug 控件。
  - 本轮不把环境源从 LDR lat-long 迁移到 HDR 或 cubemap 存储。

## 2. Review 摘要

- 本轮 review 的主要结论：
  - `RendererSystemLighting` 同时承担了源纹理加载、irradiance 生成、prefilter 生成、BRDF LUT 生成和 pass 绑定。
  - 初始化阶段会重复解码环境源图像两次。
  - specular IBL 仍然依赖多张独立 prefilter 纹理，并且 shader 路径会把所有 level 都采样一遍。
  - runtime 里仍保留了 bring-up 阶段的行为，包括硬编码默认环境路径和 procedural/texture 切换。
  - 当前环境资源路径仍是 LDR-only，因为只接受 32-bit WIC 格式，并把派生资源上传成 `RGBA8_UNORM`。

## 3. 优先级顺序

- A5-H1：
  - 先把 environment-lighting 资源抽成可复用的资源拥有者。
- A5-H2：
  - 再把多 SRV 的 prefilter ladder 收敛成 mip 驱动的资源形态，降低 roughness 查询成本。
  - 如果当前 `RendererInterface` 或更下层 renderer/RHI 路径无法自然表达这件事，就直接扩展框架接口，而不是保留业务层绕行实现。
- A5-H3：
  - 然后清理或隔离临时 bring-up 控件，并把环境源配置改成显式接口。
- A5-H4：
  - 最后再扩展到 HDR 输入能力，并决定预处理是继续走 CPU、离线化还是迁到 GPU。

## 4. 功能逻辑

- 修改前：
  - `RendererSystemLighting` 直接持有：
    - environment source 加载，
    - irradiance 和 prefilter 纹理生成，
    - BRDF LUT 生成，
    - lighting pass 的环境纹理绑定。
  - 初始化时环境源图会先为 source texture 解码一次，再为 derived textures 解码一次。
- 修改后：
  - 新的 `EnvironmentLightingResources` 负责：
    - source texture 生命周期，
    - irradiance 与 prefilter 纹理创建，
    - BRDF LUT 创建，
    - lighting pass 的环境纹理绑定配置。
  - `RendererSystemLighting` 现在主要保留：
    - lighting global 参数，
    - direct-light 与 shadow 编排，
    - lighting pass 创建，
    - lighting debug UI。
  - 环境源图像在一次初始化流程里只再解码一次。

## 5. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/EnvironmentLightingResources.h`
- `glTFRenderer/RendererDemo/RendererSystem/EnvironmentLightingResources.cpp`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/RendererDemo.vcxproj`
- `glTFRenderer/RendererDemo/RendererDemo.vcxproj.filters`
- `docs/FeatureNotes/20260317_A5_IBLHardeningPhase1.md`
- `docs/FeatureNotes/20260317_A5_IBLHardeningPhase1_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 6. 验证与证据

- 功能检查：
  - `RendererDemo` 在资源抽离完成后构建成功。
  - A5 已批准 baseline 的 compare 在 DX 与 Vulkan 上均通过。
- 视觉检查：
  - 已覆盖批准 baseline compare 的三个 case：
    - `env_texture_ibl_reference`
    - `ibl_off_reference`
    - `procedural_ibl_reference`
  - 首次 compare 曾抓到一个瞬时回归：
    - `procedural_ibl_reference` 失败，原因是抽离后的 BRDF LUT 路径使用了与现有 baseline 不一致的 `GeometrySchlickGGXIBL` 近似。
    - 在最终验收前已恢复为原基线实现并重新通过 compare。
- 性能检查：
  - 本轮没有新增性能 gate。
  - compare 仍保持 visual-first，这与当前 A5.6 的 gate 策略一致。
- 证据文件/链接：
  - Build wrapper 摘要：
    - `build_logs/build_a5h1_20260317_r3.stdout.log`
  - MSBuild 日志：
    - `build_logs/rendererdemo_20260317_154249.msbuild.log`
    - `build_logs/rendererdemo_20260317_154249.stdout.log`
    - `build_logs/rendererdemo_20260317_154249.stderr.log`
    - `build_logs/rendererdemo_20260317_154249.binlog`
  - Compare wrapper 摘要：
    - `build_logs/compare_a5h1_baseline_20260317_r2.stdout.log`
  - Compare 输出：
    - `.tmp/regression_compare_a5_model_viewer_lighting/a5cb_20260317_154555/summary.json`
    - `.tmp/regression_compare_a5_model_viewer_lighting/a5cb_20260317_154555/summary.md`

## 7. 验收结果

- 状态：PASS
- 验收说明：
  - review 驱动的加固顺序已经在项目内落档。
  - 第一项加固任务 `EnvironmentLightingResources` 已经接入并通过构建。
  - 最终批准 baseline compare 已在 DX 与 Vulkan 双后端通过。

## 8. 剩余风险与后续

- A5-H1 之后仍存在的风险：
  - specular prefilter 查询仍然绑定并采样多张独立纹理。
  - 临时 debug 控件和匿名环境参数打包仍未完全清理。
  - 环境源和派生纹理仍局限于当前 LDR 路径。
- 后续任务：
  - A5-H2：把 specular prefilter 收敛到 mip 驱动的单资源路径并降低逐像素成本；如果维护路径需要，就同步扩展 renderer/RHI-facing 接口。
  - A5-H3：把临时 environment bring-up 控件从 runtime-facing 逻辑中隔离或删除。
  - A5-H4：评估 HDR 输入能力和更可扩展的环境预处理路径。
  - 原则更新：后续重构中，如果真实渲染需求已经证明 `RendererCore` / `RHICore` 接口过窄，应当顺势补齐框架能力，而不是把“避免改框架”当成成功标准。
