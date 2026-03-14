# Feature Note - B9 固定视点回归与帧级遥测计划

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260228_B9_FixedViewpointScreenshotRegressionPlan.md`

- Date: 2026-02-28
- Work Item ID: B9
- Title: 固定视点截图回归与帧级性能遥测系统
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
  - `docs/RendererDemo_FrostedGlass_FramePassReference.md`
  - `docs/FeatureNotes/20260227_B8_BlurSourceOptimizationPlan.md`
  - `docs/FeatureNotes/20260228_B9_RegressionWorkflow_Runbook.md`

## 1. 需求更新

- 新需求：
  - 用可复现的固定视点截图回归，替代手工、主观的视觉对比。
  - 在同一套流程里支持客观 diff 指标与工件输出。
  - 同步采集 frame / per-pass 性能数据，为优化提供依据。
  - 在加入命令行批量验证后，仍保持当前 demo 工作流可用。
- 范围：
  - Demo 侧确定性 capture pipeline（第一阶段）
  - 渲染框架侧 capture / readback 接口（第二阶段增强）
  - 渲染框架侧 frame telemetry 导出接口（第二阶段增强）
  - Demo 侧固定视点测试用例构造 / 加载
  - CLI capture / compare / report 工作流（视觉 + 性能）

## 2. 问题陈述

当前循环依赖人工摆相机和主观视觉判断：

- 很难在不同运行中保持 camera / panel / light 状态一致
- 很难客观量化质量是否更好或更差
- 缺少可用于 PR / commit 级验收的稳定工件包
- 性能优化缺少针对具体 case 的 frame / per-pass 证据链

目标循环：

- 一条命令抓取预定义视点与遥测快照
- 一条命令比较 baseline 与 current 输出（视觉 + 性能）
- 报告给出 pass / fail、指标、diff 图和 telemetry delta

## 3. 提议的系统架构

三层架构：

1. Framework 层（capture 与 telemetry 原语）
- 针对指定 render output 的统一截图请求接口
- 异步 readback 与确定性文件写出
- metadata 输出（分辨率、帧索引、commit、GPU 信息、时间戳）
- frame telemetry snapshot 导出接口（frame time + pass timing + 选定计数器）

2. Application 层（测试用例套件）
- 固定视点 case descriptor（scene / camera / panel / global toggle）
- 确定性 warmup frame 策略
- RendererDemo 中的 batch capture runner
- 性能 probe profile 选择（采哪些指标、哪些指标参与门控）

3. Regression 层（CLI compare / report）
- baseline 与 current 的图像指标计算
- baseline 与 current 的性能指标比较
- 基于阈值的 pass / fail（视觉和性能）
- 报告工件包：summary json + markdown + diff heatmap + telemetry delta table

## 4. 数据模型与目录布局

- 本文使用的 repo 相对路径，均以当前目录 `C:\glTFRenderer` 为根。

## 4.1 Suite descriptor（json）

示例字段：

- `suite_name`
- `global.disable_debug_ui`
- `global.freeze_directional_light`
- `global.disable_panel_input_state_machine`
- `global.disable_prepass_animation`
- `global.default_warmup_frames`
- `global.default_capture_frames`
- `cases[]`：
  - `id`
  - `camera`（`position`、`euler_angles` 或 `euler_radians`）
  - `warmup_frames`（可选覆盖）
  - `capture_frames`（可选覆盖）
  - `capture_screenshot`（可选覆盖）
  - `capture_perf`（可选覆盖）
  - `logic_pack`（当前：`none|frosted_glass`）
  - `logic_args`（对 `frosted_glass` 而言：`blur_source_mode`、`full_fog_mode`、`reset_temporal_history`、runtime toggles）

## 4.2 建议目录约定

- suite 定义：`glTFRenderer/RendererDemo/Resources/RegressionSuites/*.json`
- 运行输出根目录示例：`glTFRenderer/build_logs/regression/<suite>_<timestamp>/`
- 每次运行的文件：
  - `suite_result.json`
  - `case_image.png`（示例命名）
  - `case_pass.csv`（示例命名）
  - `case_perf.json`（示例命名）

## 5. CLI 工作流

## 5.1 Capture

当前原型命令：

```powershell
RendererDemo.exe DemoAppModelViewer -dx -regression -regression-suite=Resources/RegressionSuites/frosted_glass_b9_smoke.json
```

- 可选输出根目录覆盖：
  - `-regression-output=<path>`

行为：

- 加载 suite
- 应用每个 case 状态
- 跑 warmup frame
- 抓取输出图像
- 写 per-case pass csv
- 写 per-case perf json（capture window 内的 frame + frosted-group aggregate）
- 写 suite summary json
- suite 结束后自动关闭窗口

## 5.2 Compare

当前原型命令：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Compare-RendererRegression.ps1 `
  -Baseline <baseline_run_dir> `
  -Current <current_run_dir> `
  -ReportOut <report_dir> `
  -Profile .\scripts\RegressionCompareProfile.default.json
```

行为：

- 加载 baseline / current 的 `suite_result.json`
- 按 case id 进行匹配比较
- 视觉指标：`MAE` / `RMSE` / `PSNR`
- 性能指标：frame / frosted aggregate 的增幅阈值
- 生成：
  - summary JSON
  - summary Markdown
  - `diff/*_absdiff.png`
- 失败时返回非零退出码

当前 compare 逻辑还会：

- 对每个 case 计算指标（`MAE`、`RMSE`、`PSNR`，以及可选 `SSIM`）
- 应用全局阈值和可选 ROI 阈值
- 根据阈值 profile 比较性能指标（frame 和可选 pass 级 delta）
- 输出报告与 diff 图像

## 6. 优先级与依赖计划

状态说明：`Planned`、`In Progress`、`Blocked`、`Accepted`

| Sub-item | 范围 | 优先级 | 依赖 | 状态 | 输出 |
|---|---|---|---|---|---|
| B9.1 | Framework screenshot API + readback queue | P0 | - | In Progress | 当前仍用 window client capture 回退；framework readback 待完成 |
| B9.2 | Framework frame / per-pass telemetry export API | P0 | - | In Progress | 当前 runner 已输出 per-case pass csv + perf json |
| B9.3 | Demo fixed-case suite loader and state applier | P0 | B9.1 | Accepted | 已支持 json suite load + camera pose + logic-pack apply |
| B9.4 | Deterministic warmup and frame gate | P0 | B9.1, B9.3 | Accepted | 已支持 per-case warmup / capture gate + temporal reset |
| B9.5 | Batch capture command-line entry | P0 | B9.2, B9.3, B9.4 | Accepted | `-regression -regression-suite=...` 自动运行并自动退出 |
| B9.6 | Visual compare engine + thresholds | P0 | B9.5 | In Progress | 已有 script 版 MAE / RMSE / PSNR compare + pass / fail |
| B9.7 | Performance compare engine + thresholds | P0 | B9.2, B9.5 | In Progress | 已有 script 版 perf delta gate + pass / fail |
| B9.8 | Diff / report generation | P1 | B9.6, B9.7 | In Progress | 已生成 summary JSON / Markdown + absdiff 输出 |
| B9.9 | CI integration job（capture / compare） | P1 | B9.6, B9.7, B9.8 | Planned | 自动化 visual / perf regression gate |
| B9.10 | Baseline update workflow and policy | P1 | B9.6, B9.7 | Planned | 受控的 visual / perf baseline 刷新流程 |

## 7. 推荐执行顺序

Phase 1（最小可用的 visual + perf capture）：

1. B9.1
2. B9.2
3. B9.3
4. B9.4
5. B9.5

Phase 2（客观的 visual / perf 验证）：

1. B9.6
2. B9.7
3. B9.8

Phase 3（团队工作流与治理）：

1. B9.9
2. B9.10

## 8. 验收检查表

功能：

- 相同 suite 文件在同一台机器上能产出可复现的 capture（小且有界的误差）
- 每个 case 都能在不手动移动相机的前提下被应用
- 相同输入下，compare 命令会返回确定性的 pass / fail（视觉 + 性能）

工件：

- 每次运行都产出完整的图像、metadata、性能指标和报告工件包
- 每个失败的视觉 case 都有明确的指标值与 diff 输出路径
- 每个失败的性能 case 都有指标 delta 与阈值来源

工作流：

- 日常迭代中，一条 capture 命令 + 一条 compare 命令就足够
- baseline 更新是显式且可审计的
- visual 和 performance gate 可以按 suite / profile 配置

## 9. 风险与缓解

- 动态 scene / light / input 导致非确定性：
  - 缓解：锁输入模式、确定性 warmup、可选固定 light animation seed / time。
- TAA / history 敏感：
  - 缓解：支持 warmup frame 和按 case 配置的可选 history reset。
- 跨 GPU 的数值漂移：
  - 缓解：按硬件类别做 machine-specific baseline bucket 或放宽阈值。
- 运行时噪声导致性能波动：
  - 缓解：固定 warmup / sample 策略、重复采样取中位数、按 profile 配容差。

## 10. 下一步动作

- 基于当前原型继续推进：
  - 完成 B9.1 的 framework 级 GPU readback screenshot API，替换 window capture 回退
  - 把 B9.6 / B9.7 / B9.8 从 script 原型迁移到集成命令工作流，或保留 script 作为稳定工具
  - 为 capture + compare 入口增加 CI gate

## 11. 运行备注（2026-02-28）

- 通过 Demo UI 导出的 case json，capture / compare 循环已验证可用。
- 运行时启动必须把 exe 目录作为工作目录：
  - `glTFRenderer/x64/Debug`
- `-regression-suite` 与 `-regression-output` 建议使用绝对路径。
- 当前常见失败原因：
  - 错误的 CWD 导致 `glTFResources/...` 场景加载失败
  - 错误的 CWD 导致 `Resources/Shaders/...` shader 加载失败
- 详细命令模板与故障排查写在：
  - `docs/FeatureNotes/20260228_B9_RegressionWorkflow_Runbook.md`
