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
  - 在通用 build / capture / compare 工作流之上补一个 A5 专用的一键验证入口。
  - 为通过的 A5 validate 结果补一个本地 baseline 固化脚本。
- 本次不包含：
  - 让普通 `DemoAppModelViewer` 完全具备和 frosted 路径一致的 RenderDoc 自动化能力。
  - 长期批准 baseline 的沉淀与存放策略。

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
  - 验证入口：
    - `scripts/Validate-A5-ModelViewerLighting.ps1` 固化了 A5 suite、`DemoAppModelViewer` 和默认 compare profile，对外提供一条命令的验证入口。

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
- `scripts/Validate-A5-ModelViewerLighting.ps1`
- `scripts/Promote-A5-ModelViewerLighting-Baseline.ps1`

## 5. 验证与证据

- 功能检查：
  - `RendererDemo` 构建成功。
  - A5.6 相关文件没有引入新的 warning；剩余 warning 是渲染模块里原有的数值转换 warning。
  - DX 和 Vulkan 两条运行时 regression capture 都成功完成全部 smoke suite case。
  - A5 smoke suite 已经可以通过专用 wrapper 跑通统一的 baseline/current compare 流程。
- 视觉检查：
  - screenshot 导出路径在 DX 和 Vulkan 上各成功生成 3 张 smoke suite PNG。
  - 针对同一构建的成对 fresh capture，自动 baseline/current 视觉 compare 已在两个后端通过。
- 性能检查：
  - 每个 case 的 perf JSON 导出成功。
  - 直接走 perf-enabled validate 时，两个后端都通过过一次。
  - 但重复顺序运行仍然能看到足以越过默认阈值的 GPU timing 波动，所以 A5 专用 wrapper 先把 perf compare 维持为显式 opt-in。
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
  - A5 统一验证 wrapper 日志：
    - `build_logs/validate_a56_regression_20260317_124126.stdout.log`
    - `build_logs/validate_a56_regression_20260317_124126.stderr.log`
    - `build_logs/validate_a56_wrapper_20260317_124613.stdout.log`
    - `build_logs/validate_a56_wrapper_20260317_124613.stderr.log`
    - `build_logs/validate_a56_wrapper_20260317_124916.stdout.log`
    - `build_logs/validate_a56_wrapper_20260317_124916.stderr.log`
  - baseline promotion 日志：
    - `build_logs/promote_a56_baseline_20260317_145456.stdout.log`
    - `build_logs/promote_a56_baseline_20260317_145456.stderr.log`
  - Regression 输出：
    - `.tmp/regression_a56/rc_dx_20260316_173359/model_viewer_lighting_a5_ibl_smoke_20260316_173401/suite_result.json`
    - `.tmp/regression_a56/rc_dx_20260316_173359/model_viewer_lighting_a5_ibl_smoke_20260316_173401/cases/001_ibl_off_reference.png`
    - `.tmp/regression_a56/rc_dx_20260316_173359/model_viewer_lighting_a5_ibl_smoke_20260316_173401/cases/002_procedural_ibl_reference.png`
    - `.tmp/regression_a56/rc_dx_20260316_173359/model_viewer_lighting_a5_ibl_smoke_20260316_173401/cases/003_env_texture_ibl_reference.png`
    - `.tmp/regression_a56_vk/rc_vk_20260316_174242/model_viewer_lighting_a5_ibl_smoke_20260316_174245/suite_result.json`
    - `.tmp/regression_a56_vk/rc_vk_20260316_174242/model_viewer_lighting_a5_ibl_smoke_20260316_174245/cases/001_ibl_off_reference.png`
    - `.tmp/regression_a56_vk/rc_vk_20260316_174242/model_viewer_lighting_a5_ibl_smoke_20260316_174245/cases/002_procedural_ibl_reference.png`
    - `.tmp/regression_a56_vk/rc_vk_20260316_174242/model_viewer_lighting_a5_ibl_smoke_20260316_174245/cases/003_env_texture_ibl_reference.png`
  - 统一验证摘要：
    - `.tmp/regression_validate_a56/rv_20260317_124127/summary.json`
    - `.tmp/regression_validate_a56/rv_20260317_124127/summary.md`
    - `.tmp/regression_validate_a5_wrapper/rv_20260317_124614/summary.md`
    - `.tmp/regression_validate_a5_wrapper/rv_20260317_124916/summary.json`
    - `.tmp/regression_validate_a5_wrapper/rv_20260317_124916/summary.md`
  - 已固化的本地 baseline：
    - `build_logs/regression_baselines/a5_model_viewer_lighting/accepted_20260317/baseline_manifest.json`
    - `build_logs/regression_baselines/a5_model_viewer_lighting/latest.json`
    - `build_logs/regression_baselines/a5_model_viewer_lighting/accepted_20260317/validation.summary.json`

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

### 5.5 A5 统一验证

- 命令：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Validate-A5-ModelViewerLighting.ps1
```

- 结果：
  - `STATUS=ValidationPassed`
  - `BACKENDS=dx,vk`
  - `PASSED_BACKENDS=2`
  - `FAILED_BACKENDS=0`
  - `BACKEND_DX_STATUS=Passed`
  - `BACKEND_VK_STATUS=Passed`
  - 两个 backend 的 compare summary 都是 `ComparePassed`
  - compare summary 里的 `Perf` 列对所有 case 都显示 `N/A`，因为 wrapper 默认只做 visual compare
  - 如果需要 perf gate，可以显式加 `-EnablePerfCompare`

### 5.6 Perf-Enabled Spot Check

- 命令：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Validate-RendererRegression.ps1 `
  -Suite glTFRenderer\RendererDemo\Resources\RegressionSuites\model_viewer_lighting_a5_ibl_smoke.json `
  -DemoApp DemoAppModelViewer `
  -OutputBase .tmp\regression_validate_a56
```

- 结果：
  - `STATUS=ValidationPassed`
  - `BACKENDS=dx,vk`
  - `PASSED_BACKENDS=2`
  - `FAILED_BACKENDS=0`
  - `BACKEND_DX_PERF_SKIPPED=0`
  - `BACKEND_VK_PERF_SKIPPED=0`
  - 后续重复运行在部分 case 上出现了超出默认阈值的 perf 波动，因此专用 wrapper 暂时把 perf compare 保持为 opt-in

### 5.7 Baseline 固化

- 命令：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Promote-A5-ModelViewerLighting-Baseline.ps1 `
  -Validation .tmp\regression_validate_a5_wrapper\rv_20260317_124916\summary.json `
  -Label accepted_20260317
```

- 结果：
  - `STATUS=BaselinePromoted`
  - `RUN_KIND=baseline`
  - 本地 baseline 根目录：
    - `build_logs/regression_baselines/a5_model_viewer_lighting/accepted_20260317`
  - manifest：
    - `build_logs/regression_baselines/a5_model_viewer_lighting/accepted_20260317/baseline_manifest.json`
  - latest 指针：
    - `build_logs/regression_baselines/a5_model_viewer_lighting/latest.json`
  - 已固化的 backend 工件：
    - DX baseline run 已复制到 baseline 根目录
    - Vulkan baseline run 已复制到 baseline 根目录

## 6. 验收结果

- 状态：PASS（实现、构建、DX 运行时截取与 Vulkan 运行时截取范围）
- 验收说明：
  - 构建状态：成功
  - Warning 数量：`38`（历史遗留）
  - Error 数量：`0`
  - DX regression suite 状态：`3/3` case 通过
  - Vulkan regression suite 状态：`3/3` case 通过
  - 统一 baseline/current visual compare 验证：两个后端都通过
  - A5 环境光照验证不再依赖 frosted-glass app 路径才能落地。
  - A5 现在具备一条 build + capture + visual compare 的一键验证入口。
  - A5 现在已经有一份本地批准的 DX / Vulkan baseline manifest，可供后续 compare 复用。

## 7. 风险与后续

- 已知风险：
  - 普通 `DemoAppModelViewer` 回归路径当前仍是 screenshot-first，还没有对齐 frosted 路径的 RenderDoc 自动化。
  - smoke suite 目前只覆盖一个固定的 Sponza 视角，材质和角度覆盖还比较窄。
  - 默认 perf 阈值目前仍有噪声，所以 A5 专用 wrapper 没把 perf compare 设为强制 gate。
  - 面向已批准 baseline 的 compare 目前还需要手动走通用工作流，还没有专门的 “current vs approved A5 baseline” wrapper。
- 后续任务：
  - 增加一个专门针对 `latest.json` 的 fresh-capture compare wrapper。
  - 决定 A5 是要采用更宽松 / 分机器 bucket 的 perf 策略，还是继续把 perf 检查保留为手动 spot check。
  - 如果 A5 需要成为长期 gate，再补充 rough metal、掠射高光和更多视角的 case。

## 8. 下一阶段计划

1. 接通 compare 路径
   - 状态：已通过 `scripts/Validate-A5-ModelViewerLighting.ps1` 完成日常验证入口接通。
   - 剩余工作：
     - 增加一个针对 `build_logs/regression_baselines/a5_model_viewer_lighting/latest.json` 的 compare wrapper。
     - 决定 perf compare 是否从 opt-in 升级成稳定 gate。
   - 验收目标：
     - 面向已批准 baseline 的 compare 命令可以在两个后端的当前 3 个 case 上报告零个非预期差异。
2. 扩展覆盖面
   - 增加更有针对性的验证视角，至少覆盖：
     - rough metal 响应，
     - 掠射角 glossy dielectric，
     - 强依赖环境贴图的反射场景。
   - 验收目标：
     - suite 能明显打到 BRDF LUT 和 roughness prefilter ladder 的高光差异。
3. 延后项
   - RenderDoc 自动化对齐保留为后续项，不作为下一阶段阻塞项。
