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
- 已存在 compare 脚本：
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

## 4. 通过 CLI 抓 Baseline / Current

推荐模式（安静日志）：

```powershell
$repoRoot = (Get-Location).Path
$exeWorkDir = Join-Path $repoRoot "glTFRenderer\x64\Debug"
$exe = ".\RendererDemo.exe"
$suite = Join-Path $exeWorkDir "build_logs\regression_case_exports\0_20260228_185013.json"

$baselineOut = Join-Path $repoRoot ".tmp\regression_opt_baseline"
$currentOut = Join-Path $repoRoot ".tmp\regression_opt_current"
$baselineStdout = Join-Path $repoRoot ".tmp\capture_baseline.stdout.log"
$baselineStderr = Join-Path $repoRoot ".tmp\capture_baseline.stderr.log"
$currentStdout = Join-Path $repoRoot ".tmp\capture_current.stdout.log"
$currentStderr = Join-Path $repoRoot ".tmp\capture_current.stderr.log"

Push-Location $exeWorkDir
& $exe DemoAppModelViewer -dx -regression "-regression-suite=$suite" "-regression-output=$baselineOut" `
  > $baselineStdout `
  2> $baselineStderr

& $exe DemoAppModelViewer -dx -regression "-regression-suite=$suite" "-regression-output=$currentOut" `
  > $currentStdout `
  2> $currentStderr
Pop-Location
```

每次运行预期生成：

- 运行目录下的 `suite_result.json`
- 运行目录下的 `001_case.png`
- 运行目录下的 `001_case.pass.csv`
- 运行目录下的 `001_case.perf.json`

## 5. Compare Baseline 与 Current

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

## 6. 构建验证（安静模式）

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

## 7. 已遇到的问题与修复

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

## 8. 最小日常循环

1. 从 UI 导出或刷新 case json。
2. 抓 baseline，或复用已有 baseline 运行。
3. 修改代码 / shader。
4. 用 `Build-RendererDemo-Verify.ps1` 做构建验证。
5. 在相同 suite json 和相同 CWD 规则下抓 current 运行。
6. 运行 compare 脚本并查看 `summary.json` + `diff/*.png`。
7. 基于视觉和性能阈值决定保留还是回退。
