# 功能记录 - A5.6 Model Viewer 回归验证

配套文档：

- 英文版：
  - `docs/FeatureNotes/20260316_A5_ModelViewerRegressionValidation.md`

- 日期：2026-03-16
- 工作项 ID：A5.6
- 标题：为 A5 环境光照基线补齐 `DemoAppModelViewer` 回归验证路径
- 记录人：AI coding session
- 配套说明：
  - `docs/FeatureNotes/20260316_A5_ModelViewerRegressionValidation.md`
- 相关计划：
  - `docs/FeatureNotes/20260228_B9_FixedViewpointScreenshotRegressionPlan.md`
  - `docs/FeatureNotes/20260228_B9_RegressionWorkflow_Runbook.md`
  - `docs/FeatureNotes/20260316_A5_RoughnessPrefilterLadder.md`
- 路径约定：
  - 仓库相对路径以当前目录（`C:\glTFRenderer`）为根。
  - 代码路径通常从 `glTFRenderer/` 开始；根目录脚本使用 `scripts/`。

## 1. 范围

- 本次包含：
  - 为 `RendererSystemLighting::LightingGlobalParams` 暴露可编程读写接口。
  - 扩展 `DemoAppModelViewer` snapshot，使其可保存并回放环境光照状态。
  - 为普通 `DemoAppModelViewer` 增加 screenshot-first 的回归执行路径。
  - 为 `Regression::ApplyLogicPack(...)` 增加 `model_viewer_lighting` 支持。
  - 新增并执行一个专用于 A5 环境光照验证的 smoke suite。
- 本次不包含：
  - 让普通 `DemoAppModelViewer` 完全具备和 frosted 路径一致的 RenderDoc 自动化能力。
  - baseline/current 的图片差异自动比对。

## 2. 功能逻辑

- 变更前：
  - 普通 `DemoAppModelViewer` 的非渲染 snapshot 不包含环境光照状态。
  - 通用回归工作流实际上主要依赖 frosted-glass app 路径。
  - A5 环境光照的验证主要停留在构建成功和人工临时检查。
- 变更后：
  - `RendererSystemLighting` 提供了带归一化处理的 `GetGlobalParams()` 与 `SetGlobalParams(...)`。
  - `DemoAppModelViewer` snapshot 现在包含：
    - 相机和视口状态，
    - 平行光状态，
    - 环境光照全局参数。
  - 普通 `DemoAppModelViewer` 现在支持：
    - `-regression`
    - `-regression-suite=<path>`
    - `-regression-output=<path>`
  - 一次 regression run 现在可以：
    - 加载 suite JSON，
    - 应用固定相机和光照 logic-pack 状态，
    - 进行固定 warmup 帧数，
    - 导出 screenshot、pass CSV、perf JSON 和 `suite_result.json`，
    - 在 suite 结束后自动关闭应用。
  - 通过 `has_lighting_state` 保持向后兼容，所以只包含相机和平行光的旧 snapshot JSON 仍可正确导入。

## 3. 算法与运行时设计

- 本次涉及的模块：
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.h`
  - `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`
  - `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewerFrostedGlass.cpp`
  - `glTFRenderer/RendererDemo/Regression/RegressionLogicPack.h`
  - `glTFRenderer/RendererDemo/Regression/RegressionLogicPack.cpp`
  - `glTFRenderer/RendererDemo/Resources/RegressionSuites/model_viewer_lighting_a5_ibl_smoke.json`
- 核心运行时改动：
  - 光照状态捕获与回放：
    - `ModelViewerStateSnapshot` 新增 `has_lighting_state` 和 `lighting_global_params`。
    - `ApplyModelViewerStateSnapshot(...)` 在状态存在时回放环境光照参数。
  - Regression 执行路径：
    - `ConfigureRegressionRunFromArguments(...)` 负责解析 CLI 参数。
    - `TickRegressionAutomation()` 负责串起 warmup、capture、导出和退出。
    - 导出的产物沿用现有 screenshot-regression 形态：
      - `cases/*.png`
      - `cases/*.pass.csv`
      - `cases/*.perf.json`
      - `suite_result.json`
  - 截图实现：
    - 普通 model-viewer 路径使用 `PrintWindow(...)`，失败时回退到 `BitBlt`，并通过 `glTFImageIOUtil` 保存 PNG。
  - Logic pack 支持：
    - `model_viewer_lighting` 可配置：
      - `environment_enabled`
      - `use_environment_texture`
      - `ibl_diffuse_intensity`
      - `ibl_specular_intensity`
      - `ibl_horizon_exponent`
      - `environment_texture_intensity`
      - 可选 procedural 颜色
      - `directional_light_speed_radians`
  - App 路径隔离：
    - `DemoAppModelViewerFrostedGlass` 在初始化时直接重建自己的 runtime，避免通过基类重复配置 frosted 专用 regression 流程。

## 4. 改动文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.h`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewerFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Regression/RegressionLogicPack.h`
- `glTFRenderer/RendererDemo/Regression/RegressionLogicPack.cpp`
- `glTFRenderer/RendererDemo/Resources/RegressionSuites/model_viewer_lighting_a5_ibl_smoke.json`
- `docs/FeatureNotes/20260316_A5_ModelViewerRegressionValidation.md`
- `docs/FeatureNotes/20260316_A5_ModelViewerRegressionValidation_ZH.md`
- `docs/README.md`
- `docs/README_ZH.md`

## 5. 验证与证据

- 功能检查：
  - `RendererDemo` 构建成功。
  - A5.6 相关文件没有引入新的 warning；剩余 warning 是渲染模块里原有的数值转换 warning。
  - DX 和 Vulkan 两条运行时 regression capture 都成功完成全部 smoke suite case。
- 视觉检查：
  - screenshot 导出路径在 DX 和 Vulkan 上各成功生成 3 张 smoke suite PNG。
  - 本轮没有执行人工视觉审查，也没有做 baseline/current 差异比对。
- 性能检查：
  - 每个 case 的 perf JSON 导出成功。
  - 本轮没有执行跨运行的性能对比。
- 文档检查：
  - 文档引用校验仍报告 `75` 个历史遗留问题。
  - 新增的 A5.6 中英文 note 和 README 索引项没有出现在错误列表中。
- 证据文件：
  - Build wrapper 摘要：
    - `build_logs/build_verify_20260316_a56.stdout.log`
    - `build_logs/build_verify_20260316_a56.stderr.log`
  - MSBuild 日志：
    - `build_logs/rendererdemo_20260316_173254.msbuild.log`
    - `build_logs/rendererdemo_20260316_173254.stdout.log`
    - `build_logs/rendererdemo_20260316_173254.stderr.log`
    - `build_logs/rendererdemo_20260316_173254.binlog`
  - Regression capture wrapper 摘要：
    - `build_logs/capture_a56_dx.stdout.log`
    - `build_logs/capture_a56_dx.stderr.log`
    - `build_logs/capture_a56_vk_20260316_174242.stdout.log`
    - `build_logs/capture_a56_vk_20260316_174242.stderr.log`
  - 文档引用校验日志：
    - `build_logs/validate_docs_20260316_174452.stdout.log`
    - `build_logs/validate_docs_20260316_174452.stderr.log`
  - Regression 输出：
    - `.tmp/regression_a56/rc_dx_20260316_173359/model_viewer_lighting_a5_ibl_smoke_20260316_173401/suite_result.json`
    - `.tmp/regression_a56/rc_dx_20260316_173359/model_viewer_lighting_a5_ibl_smoke_20260316_173401/cases/001_ibl_off_reference.png`
    - `.tmp/regression_a56/rc_dx_20260316_173359/model_viewer_lighting_a5_ibl_smoke_20260316_173401/cases/002_procedural_ibl_reference.png`
    - `.tmp/regression_a56/rc_dx_20260316_173359/model_viewer_lighting_a5_ibl_smoke_20260316_173401/cases/003_env_texture_ibl_reference.png`
    - `.tmp/regression_a56_vk/rc_vk_20260316_174242/model_viewer_lighting_a5_ibl_smoke_20260316_174245/suite_result.json`
    - `.tmp/regression_a56_vk/rc_vk_20260316_174242/model_viewer_lighting_a5_ibl_smoke_20260316_174245/cases/001_ibl_off_reference.png`
    - `.tmp/regression_a56_vk/rc_vk_20260316_174242/model_viewer_lighting_a5_ibl_smoke_20260316_174245/cases/002_procedural_ibl_reference.png`
    - `.tmp/regression_a56_vk/rc_vk_20260316_174242/model_viewer_lighting_a5_ibl_smoke_20260316_174245/cases/003_env_texture_ibl_reference.png`

### 5.1 构建

- 命令：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Build-RendererDemo-Verify.ps1
```

- 结果：
  - `STATUS=BuildSucceeded`
  - `WARNINGS=38`
  - `ERRORS=0`

### 5.2 DX 运行时截取

- 命令：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite glTFRenderer\RendererDemo\Resources\RegressionSuites\model_viewer_lighting_a5_ibl_smoke.json `
  -Backend dx `
  -DemoApp DemoAppModelViewer `
  -OutputBase .tmp\regression_a56
```

- 结果：
  - `STATUS=RunSucceeded`
  - `EXITCODE=0`
  - `DEMO_APP=DemoAppModelViewer`
  - `CASES=3`
  - `RESULTS=3`
  - `FAILED=0`
  - `SUITE_SUCCESS=True`
  - `render_device=DX12`
  - `present_mode=VSYNC`
  - `renderdoc_capture_available=False`

### 5.3 Vulkan 运行时截取

- 命令：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite glTFRenderer\RendererDemo\Resources\RegressionSuites\model_viewer_lighting_a5_ibl_smoke.json `
  -Backend vk `
  -DemoApp DemoAppModelViewer `
  -OutputBase .tmp\regression_a56_vk
```

- 结果：
  - `STATUS=RunSucceeded`
  - `EXITCODE=0`
  - `DEMO_APP=DemoAppModelViewer`
  - `CASES=3`
  - `RESULTS=3`
  - `FAILED=0`
  - `SUITE_SUCCESS=True`
  - `render_device=Vulkan`
  - `present_mode=VSYNC`
  - `renderdoc_capture_available=False`

### 5.4 文档引用校验

- 命令：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Validate-DocReferences.ps1
```

- 结果：
  - validator exit code：`1`
  - 扫描 markdown 文件数：`87`
  - 检查引用数：`1057`
  - 报告问题数：`75`
  - 问题类型：仍然是旧 `build_logs`、`.tmp` 和本机 RenderDoc 路径缺失
  - 本次新增的 A5.6 note 与 README 索引更新未出现在错误列表中

## 6. 验收结果

- 状态：PASS（实现、构建、DX 运行时截取与 Vulkan 运行时截取范围）
- 验收说明：
  - 构建状态：成功
  - Warning 数量：`38`（历史遗留）
  - Error 数量：`0`
  - DX regression suite 状态：`3/3` case 通过
  - Vulkan regression suite 状态：`3/3` case 通过
  - A5 环境光照验证不再依赖 frosted-glass app 路径才能落地。

## 7. 风险与后续

- 已知风险：
  - 普通 `DemoAppModelViewer` 回归路径当前仍是 screenshot-first，还没有对齐 frosted 路径的 RenderDoc 自动化。
  - smoke suite 目前只覆盖一个固定的 Sponza 视角，材质和角度覆盖还比较窄。
- 后续任务：
  - 把新的 model-viewer 产物接入 baseline/current compare 用法。
  - 如果 A5 需要成为长期 gate，再补充 rough metal、掠射高光和更多视角的 case。

## 8. 下一阶段计划

1. 接通 compare 路径
   - 让当前 `model_viewer_lighting_a5_ibl_smoke` 的输出可以直接进入现有 baseline/current compare 工作流，不需要人工整理文件。
   - 基于当前 smoke suite 产物分别沉淀 DX 和 Vulkan 的批准 baseline。
   - 验收目标：
     - 一条 compare 命令就能在两个后端的当前 3 个 case 上报告零个非预期差异。
2. 扩展覆盖面
   - 增加更有针对性的验证视角，至少覆盖：
     - rough metal 响应，
     - 掠射角 glossy dielectric，
     - 强依赖环境贴图的反射场景。
   - 验收目标：
     - suite 能明显打到 BRDF LUT 和 roughness prefilter ladder 的高光差异。
3. 延后项
   - RenderDoc 自动化对齐保留为后续项，不作为下一阶段阻塞项。
