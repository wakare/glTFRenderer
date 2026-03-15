# B9 回归工作流运行手册（固定视点截图 + Compare）

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260228_B9_RegressionWorkflow_Runbook.md`

- Date: 2026-02-28
- Scope: `RendererDemo` 的 frosted-glass 视觉 / 性能迭代
- Purpose: 提供一套确定、可重复的 baseline capture、current capture、diff / perf compare 循环
- Path base: 下文的 repo 相对路径均假定当前目录为 `C:\glTFRenderer`

## 1. 前置条件

- 已存在构建产物：
  - `glTFRenderer/x64/Debug/RendererDemo.exe`
- 已存在 regression case json（由 Demo UI 导出）：
  - 默认位置：`glTFRenderer/x64/Debug/build_logs/regression_case_exports/*.json`
- 已存在 capture / compare 脚本：
  - `scripts/Validate-RendererRegression.ps1`
  - `scripts/Capture-RendererRegression.ps1`
  - `scripts/Compare-RendererRegression.ps1`
  - `scripts/RegressionCompareProfile.default.json`

## 2. 关键运行路径规则

必须始终以 **exe 目录作为 working directory** 启动 `RendererDemo.exe`：

- repo 相对路径：`glTFRenderer/x64/Debug`

原因：

- 运行时 shader include 通过相对路径 `Resources/Shaders/...` 加载
- 场景 glTF 路径同样是相对路径（`glTFResources/...`）
- 当前 setup 下，只有 exe 目录能同时满足这两类查找

## 3. 从 UI 导出 Case JSON

在 Demo 面板中：

- 设定目标 camera / panel 状态
- 点击 `Export Current Regression Case JSON`
- 记录 UI 显示的输出路径（`Last Exported Case JSON`）

## 4. 一键验证

当目标是验证整条 regression 流程，并且不希望为不同 backend 额外写手工步骤时，优先使用统一验证入口：

```powershell
$repoRoot = (Get-Location).Path
$suite = Join-Path $repoRoot "glTFRenderer\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json"

powershell -ExecutionPolicy Bypass -File .\scripts\Validate-RendererRegression.ps1 `
  -Suite $suite `
  -Backends dx,vk `
  -OutputBase ".tmp\regression_validate" `
  -RenderDocCapture `
  -RenderDocRequired
```

预期顶层输出：

- 当所有 backend 的 build、capture、compare 都通过时，输出 `STATUS=ValidationPassed`
- 在验证输出目录下生成 `SUMMARY_JSON=...` 和 `SUMMARY_MD=...`
- 输出 `BACKEND_DX_*` / `BACKEND_VK_*` 汇总行，直接指向各 backend 的 baseline/current 运行目录和 compare summary

说明：

- 该验证脚本只构建一次，然后对 `-Backends` 里的每个 backend 运行同一套 capture / compare 流程。
- 如果你已经确认当前二进制可用，只想重跑 capture / compare，可增加 `-SkipBuild`。
- RenderDoc 相关开关会统一应用到所有 backend，不需要再补充额外的 RHI 特化验证步骤。

## 5. 通过 CLI 抓 Baseline / Current

推荐模式（安静日志）：

```powershell
$repoRoot = (Get-Location).Path
$suite = Join-Path $repoRoot "glTFRenderer\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json"

powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite $suite `
  -Backend dx `
  -OutputBase ".tmp\regression_opt_baseline"

powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite $suite `
  -Backend dx `
  -OutputBase ".tmp\regression_opt_current"
```

该 capture 脚本会输出 `STATUS=...`、`RUN_DIR=...`、`SUMMARY=...`、`STDOUT=...`、`STDERR=...` 等汇总行。后续 compare 直接使用 `RUN_DIR`，只有在状态不是 `RunSucceeded` 时才需要深入查看重定向日志。
同时它会使用 `rc_dx_<timestamp>` 这类更紧凑的内部运行目录，降低在输出根路径已经很深时触发 Windows 路径长度问题的概率。

手工回退命令：

```powershell
$repoRoot = (Get-Location).Path
$exeWorkDir = Join-Path $repoRoot "glTFRenderer\x64\Debug"
$exe = ".\RendererDemo.exe"
$suite = Join-Path $repoRoot "glTFRenderer\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json"
$baselineOut = Join-Path $repoRoot ".tmp\regression_opt_baseline_manual"

Push-Location $exeWorkDir
& $exe DemoAppModelViewerFrostedGlass -dx -disable-debug-ui --no-assert-dialog -regression "-regression-suite=$suite" "-regression-output=$baselineOut"
Pop-Location
```

可选的 RenderDoc 参数：

- 增加 `-renderdoc-ui`，可以在 device 初始化前预加载 RenderDoc runtime，但不会强制本次 regression 产出 `.rdc`。如果你只是想使用 DemoBase 里的公共 RenderDoc UI 手动抓帧，这是推荐的启动参数。
- 增加 `-renderdoc-capture`，可为启用了 `capture_renderdoc` 的 case 生成 `.rdc`，也可在本次运行中强制所有 case 都抓 RenderDoc。
- 当希望在 RenderDoc API 不可用时立即失败，可增加 `-renderdoc-required`。
- `Capture-RendererRegression.ps1` 会把这两个开关原样透传给 `RendererDemo.exe`。
- DX12 会先走正常的 DLL 搜索路径；如果没找到，再回退到已注册的 RenderDoc 安装目录，也就是 `renderdoc.json` 所在位置。
- Vulkan 不再走手工 `LoadLibrary` 注入。检测到已注册的 RenderDoc 1.43+ 安装后，程序会在 device 初始化前自动打开 RenderDoc 的 Vulkan implicit layer。若安装过旧或不完整，仍需从 RenderDoc UI 或等价注入环境启动。

每次运行预期生成：

- 运行目录下的 `suite_result.json`
- 运行目录下的 `001_case.png`
- 运行目录下的 `001_case.pass.csv`
- 运行目录下的 `001_case.perf.json`
- 可选：运行目录下的 `001_case_frameXXXX.rdc`

说明：

- RenderDoc 会去掉请求模板里的扩展名，并在最终 `.rdc` 文件名后追加 frame 后缀。
- suite `global` 现在支持：
  - `default_capture_renderdoc`
  - `default_renderdoc_capture_frame_offset`
  - `default_keep_renderdoc_on_success`
- 每个 case 的 `capture` 块现在支持：
  - `capture_renderdoc`
  - `renderdoc_capture_frame_offset`
  - `keep_renderdoc_on_success`
- `renderdoc_capture_frame_offset` 会把 RenderDoc / screenshot / pass-csv 的最终落点向后延迟指定帧数，保证这些工件仍然对齐到同一个更晚的 frame。
- `keep_renderdoc_on_success=false` 会在 summary 写出之后清理成功 case 的 `.rdc`。这种模式下 `suite_result.json` 仍会记录 capture success 和 frame index，但会把 `renderdoc_capture_path` 置空，并标记 `renderdoc_capture_retained=false`。
- `suite_result.json` 会记录每个 case 的实际 RenderDoc 路径、capture frame index、retention 状态和 capture error 字段。
- Regression screenshot 现在会优先使用 `PrintWindow(PW_RENDERFULLCONTENT | PW_CLIENTONLY)`，只有失败时才回退到 `BitBlt`，这样在普通 Windows 桌面环境下运行时更不容易把其他窗口截进去。

手动 UI 抓帧：

- 如果只是想用共享的 RenderDoc 控件手动抓帧，建议用 `-renderdoc-ui` 启动任意开启了 debug UI 的 demo。
- 打开 Demo 面板里的 `Runtime / Diagnostics > RenderDoc`。
- `Capture Current Frame` 会通过共享的 `DemoBase` 路径抓取当前帧的单帧 `.rdc`。
- `Auto Open Replay UI After Capture` 会在该帧完成后自动用 RenderDoc 打开刚产出的 `.rdc`。
- `Open Last Capture In RenderDoc` 会重新打开 runtime 记录的最近一次成功 capture。
- 当进程以 `-renderdoc-ui` 或其他 RenderDoc 启动参数进入后，runtime RHI recreate 现在也会在目标 backend 创建设备前重新执行 preload，因此 DX12/Vulkan 切换仍然沿用同一条 opt-in 路径。
- 共享 UI 和 regression 自动流程共用同一个 RenderDoc service。如果某个 regression case 已经 arm 了 capture，手动按钮会直接报告“已有 pending capture”，不会叠加第二个请求。

## 6. Compare Baseline 与 Current

```powershell
$repoRoot = (Get-Location).Path
$baselineRun = Join-Path $repoRoot ".tmp\regression_opt_baseline\0_suite_XXXXXXXX_XXXXXX"
$currentRun = Join-Path $repoRoot ".tmp\regression_opt_current\0_suite_XXXXXXXX_XXXXXX"
$reportOut = Join-Path $repoRoot ".tmp\regression_opt_compare"
$profile = Join-Path $repoRoot "scripts\RegressionCompareProfile.default.json"

powershell -ExecutionPolicy Bypass -File .\scripts\Compare-RendererRegression.ps1 `
  -Baseline $baselineRun `
  -Current  $currentRun `
  -ReportOut $reportOut `
  -Profile $profile `
  > .tmp\compare.stdout.log 2> .tmp\compare.stderr.log
```

预期输出报告：

- `.tmp/regression_opt_compare/` 下的 summary JSON
- `.tmp/regression_opt_compare/` 下的 summary Markdown
- `.tmp/regression_opt_compare/diff/` 下的 diff 图像

RenderDoc 说明：

- compare 仍然沿用正常的 screenshot diff 路径
- 只要 baseline 或 current 一侧成功产出了 RenderDoc capture，perf 检查就会自动跳过，因为 `.rdc` capture 会显著扰动 timing，继续套用 perf 阈值容易误报
- `summary.json` 会保留 baseline/current 的 RenderDoc 元数据；当 `.rdc` 仍被保留时，也会附带对应路径，方便对失败 case 直接用 RenderDoc 重放
- 如果某个成功 capture 按 `keep_renderdoc_on_success=false` 被主动清理，compare summary 会把该 case 标成 `rdc-pruned`，而不是保留一个失效路径

## 7. 构建验证（安静模式）

使用隔离的 verify build 脚本：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Build-RendererDemo-Verify.ps1 > .tmp\build.stdout.log 2> .tmp\build.stderr.log
```

之后只读取 wrapper stdout 中的汇总行：

- `STATUS=...`
- `WARNINGS=...`
- `ERRORS=...`
- `TXT=...`
- `STD=...`
- `ERR=...`
- `BIN=...`

## 8. 已遇到的问题与修复

### 7.1 场景加载崩溃（错误 CWD）

症状：

- 启动时在 glTF 加载路径（`Sponza.gltf`）抛异常
- 程序提前退出，返回非零码

根因：

- 进程从仓库根目录或其他目录启动，导致 `glTFResources/...` 无法解析

修复：

- 以 `glTFRenderer/x64/Debug` 作为 CWD 启动
- `-regression-suite` 传绝对路径

### 7.2 Shader 加载 / 编译失败（错误 CWD）

症状：

- 在 shader 源码加载路径附近抛异常（`glTFRenderer/RendererDemo/Resources/Shaders/ModelRenderingShader.hlsl`）

根因：

- shader 相对路径从错误的 working directory 解析

修复：

- 同上：CWD 必须是 exe 目录

### 7.3 PDB lock 构建失败（`C1041`）

症状：

- 间歇性构建失败：无法打开 `vc143.pdb`

根因：

- 并发 build host 进程争用

修复：

- 重跑安静 verify build（通常可恢复）
- 保持 `/m:1 /nr:false` 和隔离的 verify 输出策略

### 7.4 AIChat 输出洪泛风险

症状：

- 当命令打印大量日志时，terminal / chat 变卡甚至卡死

修复：

- 始终把 stdout / stderr 重定向到文件
- 只回报状态、warning / error 数量、关键诊断和日志路径

## 9. 最小日常循环

1. 从 UI 导出或刷新 case json。
2. 抓 baseline，或复用已有 baseline 运行。
3. 修改代码 / shader。
4. 用 `Build-RendererDemo-Verify.ps1` 做构建验证。
5. 在相同 suite json 和相同 CWD 规则下抓 current 运行。
6. 运行 compare 脚本并查看 `summary.json` + `diff/*.png`。
7. 基于视觉和性能阈值决定保留还是回退。
