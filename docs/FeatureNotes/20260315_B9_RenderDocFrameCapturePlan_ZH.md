# Feature Note - B9 RenderDoc 帧截取接入计划

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260315_B9_RenderDocFrameCapturePlan.md`

- Date: 2026-03-15
- Work Item ID: B9
- Title: Regression 工作流中的 RenderDoc 帧截取接入
- Owner: AI coding session
- Related Plan:
  - `docs/FeatureNotes/20260228_B9_FixedViewpointScreenshotRegressionPlan.md`
  - `docs/FeatureNotes/20260228_B9_RegressionWorkflow_Runbook.md`
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan_ZH.md`

## 1. 需求更新

- 新需求：
  - 在现有 `RendererDemo` regression 工作流中增加可选的 RenderDoc `.rdc` 自动截帧能力。
  - 保持现有 screenshot / perf capture 行为不变；RenderDoc 是新增工件，不替代原有产物。
  - 优先接入现有 `-regression` 与 suite 驱动的 batch flow，而不是新造一条独立 capture 路径。

## 2. 问题陈述

- 当前 B9 流程已经能稳定产出 case 级工件：
  - screenshot `.png`
  - pass timing `.pass.csv`
  - per-case summary `.perf.json`
- 当前 screenshot capture 是 window client 回退路径，不能保留 GPU 命令和状态历史。
- 一旦出现视觉或性能回归，现在缺少与该 case/frame 对齐的 GPU 调试工件，无法直接在 RenderDoc 中重放。
- 临时手工抓 RenderDoc 太慢，也容易出错：
  - 抓错帧
  - screenshot 和 `.rdc` 对不上
  - DX12 / Vulkan 的 teardown 行为更难排查

## 3. 设计目标

- 主要目标：
  - 在同一次 regression 运行里，按需生成 `.rdc` case 工件。
  - 保持 frame 选择确定性，并与现有 warmup / capture gate 对齐。
  - 当 RenderDoc 未安装或未注入时，默认 fail-soft。
  - 保持合法运行时行为，不为了让 capture 成功而缩减支持路径。

- 首个版本的非目标：
  - 不做基于 RenderDoc 的图像 diff 引擎。
  - 不把 RenderDoc 安装变成 CI 的硬依赖。
  - 不在 v1 中实现 RenderDoc 内部 replay 自动化。

## 4. 集成架构

- Framework 层：
  - 在 `glTFRenderer/RendererCore` 中新增轻量可选的 `RenderDocCaptureService`。
  - 通过 `renderdoc_app.h` 动态加载 API，并把该 service 视为与其他 GPU-backed helper 一样对生命周期敏感的 runtime helper。
  - 由 `RenderGraph` 暴露一个小型 request-driven 接口：
    - 设置 capture file path template
    - 为即将到来的某一帧 arm capture
    - 查询最后一次请求是否成功，以及产出的文件路径

- Frame lifecycle 落点：
  - 在目标帧开始前 arm capture。
  - 在被 arm 的那一帧开始执行时启动 capture。
  - 在同一帧最终 submit / present 之后结束 capture。
  - 在 shutdown 或 runtime rebuild 时，先等待 frame completion 和 GPU idle，再释放该 service。

- Demo / regression 层：
  - 扩展 regression suite config，增加可选的 RenderDoc capture 开关。
  - 保留未来按 case 细分控制的空间，同时支持 suite-level 默认值。
  - 把 `.rdc` 输出路径和 capture 状态写入 `suite_result.json`。

- Script / workflow 层：
  - `Compare-RendererRegression.ps1` 继续只负责 screenshot / perf compare。
  - 在 summary 工件中暴露 `.rdc` 路径，便于对失败 case 直接用 RenderDoc 打开排查。

## 5. 计划中的契约变更

- CLI：
  - 增加 `-renderdoc-ui`，在 device 初始化前预加载 RenderDoc，供共享的 DemoBase 手动抓帧 UI 使用，但不会强制生成 regression 工件
  - 增加 `-renderdoc-capture`，用于全局启用 RenderDoc capture
  - 增加 `-renderdoc-required`，当 RenderDoc API 不可用时初始化直接失败
  - `-regression` / `-regression-suite` / `-regression-output` 仍然是主入口

- Suite schema：
  - 在 capture config 中新增：
    - `capture_renderdoc`
    - `renderdoc_capture_frame_offset`
    - `keep_renderdoc_on_success`
  - 在 suite 级默认值中新增：
    - `default_capture_renderdoc`
    - `default_renderdoc_capture_frame_offset`
    - `default_keep_renderdoc_on_success`

- Per-case output：
  - 每个 case 的 `.rdc` 工件，写入 regression 运行目录下的 `cases/` 子目录
  - `suite_result.json` 新增字段：
    - `renderdoc_capture_path`
    - `renderdoc_capture_success`
    - `renderdoc_capture_retained`
    - `renderdoc_capture_keep_on_success`
    - `renderdoc_capture_error`

## 6. 优先级与依赖计划

状态说明：`Planned`、`In Progress`、`Blocked`、`Accepted`

| Sub-item | 范围 | 优先级 | 依赖 | 状态 | 输出 |
|---|---|---|---|---|---|
| B9.R1 | Third-party RenderDoc API bootstrap | P0 | - | Accepted | `renderdoc_app.h` 集成与动态加载 |
| B9.R2 | Framework capture service and frame hooks | P0 | B9.R1 | Accepted | `RendererCore` 中的 request/arm/start/end/teardown 路径 |
| B9.R3 | Regression schema + CLI enable path | P0 | B9.R2 | Accepted | suite / 参数解析与 opt-in 行为 |
| B9.R4 | Per-case `.rdc` artifact export and summary wiring | P0 | B9.R2, B9.R3 | Accepted | `.rdc` 文件与 `suite_result.json` 字段 |
| B9.R5 | Runbook / script updates | P1 | B9.R4 | Accepted | capture 命令与工件说明 |
| B9.R6 | DX12 / Vulkan validation | P0 | B9.R4 | Accepted | 两个 backend 上的成功截帧与干净退出 |

## 7. 推荐执行顺序

Phase 1（最小可用集成）：

1. B9.R1
2. B9.R2
3. B9.R3
4. B9.R4

Phase 2（工作流加固）：

1. B9.R5
2. B9.R6

## 8. 验收检查表

功能：

- 同一个 regression suite 可以按需为选定 case 产出 `.rdc`。
- `.png`、`.perf.json` 和 `.rdc` 对应同一个目标 case/frame。
- 当 RenderDoc runtime 缺失时，除非设置 `-renderdoc-required`，否则不会破坏原有 screenshot / perf capture。
- `suite_result.json` 能记录 capture 成功/失败及工件路径。

运行时安全：

- DX12 路径在 RenderDoc capture 后能干净退出。
- Vulkan 路径在 RenderDoc capture 后能干净退出。
- runtime rebuild / shutdown 顺序仍然保证先等待 frame completion 和 GPU idle，再销毁 GPU helper。

工作流：

- 日常回归仍然只需要一条 capture 命令和一条 compare 命令。
- 失败 case 有足够元数据让开发者直接打开匹配的 `.rdc` 排查。

## 9. 风险与缓解

- screenshot / perf 与 `.rdc` 帧错位：
  - 缓解：在 framework 中按 absolute frame index 绑定 capture 请求，不依赖过晚的 case finalization。

- RenderDoc 在不同机器上的可用性不一致：
  - 缓解：默认 fail-soft；在严格环境下显式使用 `-renderdoc-required`。

- 工件体积较大：
  - 缓解：保持 opt-in 与 per-case 可选；现在已经可以通过 `keep_renderdoc_on_success=false` 自动清理成功 case 的 `.rdc`。

- backend-specific shutdown 回归：
  - 缓解：复用现有 frame-complete 与 GPU-idle teardown 顺序，并在 DX12 / Vulkan 上都做验证。

## 10. 当前状态

- `RendererCore`、`RendererDemo` 与 regression suite parsing 的最小集成已经完成。
- 工作流加固已经完成，包括 `scripts/Capture-RendererRegression.ps1`、`scripts/Compare-RendererRegression.ps1` 与 B9 runbook 更新。
- 统一验证入口已经补齐到 `scripts/Validate-RendererRegression.ps1`，因此 DX12 与 Vulkan 可以走同一条 build / capture / compare 流程完成验证。
- `DemoBase` 现在在 `Runtime / Diagnostics > RenderDoc` 下暴露了共享的手动控制，包括单帧 capture、自动打开 replay UI，以及重新打开最近一次 capture。
- 这条共享手动 UI 仍然保持 `-renderdoc-ui` 显式 opt-in，这样普通的非 RenderDoc 运行不会额外扰动原有 perf 行为。
- 已在 RenderDoc 1.43 上完成以下验证：
  - DX12 的 required capture 路径
  - Vulkan 的自动 implicit-layer 激活路径
  - compare summary 对 RenderDoc 元数据的保留
- screenshot capture 的稳定性加固已经完成：
  - regression capture 期间会主动关闭 RenderDoc overlay
  - screenshot 导出会优先走 `PrintWindow`，再回退到 GDI `BitBlt`，从而避免普通桌面遮挡对重复性测试的污染
- compare 工作流的稳定性加固已经完成：
  - 只要 case 成功产出了 RenderDoc `.rdc`，perf 阈值判断就会自动跳过
  - compare summary 会保留 RenderDoc 元数据，以及仍被保留的工件路径，方便后续调试
- regression suite schema 加固已经完成：
  - `renderdoc_capture_frame_offset` 可以把对齐后的 RenderDoc / screenshot / pass-csv 最终落点向后延迟
  - `keep_renderdoc_on_success=false` 可以在保留 summary 元数据和 compare `rdc-pruned` 标记的同时清理成功 `.rdc`
- 脚本路径与进程收尾加固已经完成：
  - capture 与统一验证都会使用更紧凑的内部运行目录（`rc_*` / `rv_*`），避免 `.pass.csv`、`.perf.json`、`.rdc` 在 Windows 深路径下写失败
  - 统一验证在检测到子脚本已经输出 `STATUS=` 后会回收尾部进程，避免明明成功却卡在 wrapper 退出阶段

## 11. 运行备注（2026-03-15）

- 该计划明确以 B9 扩展项处理，不再平行创建新的 capture pipeline。
- 现有 screenshot capture 仍然是独立回退路径，不能与 RenderDoc `.rdc` 混为一谈。
- 首个版本优先保证帧对齐和 teardown 安全，再考虑更多便利性功能。
- 当前工作站上的本地验证结果：
  - DX12 自动 preload RenderDoc 并完成 capture 已经端到端通过。
  - 当系统中存在已注册的 RenderDoc 1.43+ 安装时，Vulkan 自动 implicit-layer 激活已经端到端通过。
  - 对于较老或安装不完整的 RenderDoc，仍可能需要从 RenderDoc UI 或其他已注入环境启动。
  - RenderDoc capture 会明显扰动 perf 数字，因此 compare 现在把带 `.rdc` 的 case 视为视觉调试 capture，而不是 perf gate。
