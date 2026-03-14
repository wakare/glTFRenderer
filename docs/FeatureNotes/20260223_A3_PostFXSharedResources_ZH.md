# Feature Note - A3.1 PostFX 共享 Half/Quarter Ping-Pong 资源基础

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260223_A3_PostFXSharedResources.md`

- Date: 2026-02-23
- Work Item ID: A3.1
- Title: 为 postfx 增加可复用、window-relative 的 half / quarter ping-pong render target
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. 范围

- In scope:
  - 增加可复用的 PostFX 共享资源 helper，用于分配 half / quarter 分辨率的 ping-pong render target。
  - 用 `PostFX_*` 前缀统一 frosted 相关 postfx 资源命名。
  - 在 `RendererSystemFrostedGlass` 中接入初始化，并暴露 handle，供后续 multi-pass stage 使用。
- Out of scope:
  - 新的 blur pyramid pass 或 temporal history 消费。
  - 替换当前 frosted 单 pass composite 算法。
  - ToneMap 输入 / 输出行为变更。

## 2. 功能逻辑

- Behavior before:
  - Frosted 路径只拥有一个 full-resolution 输出目标。
  - 没有可复用的 half / quarter ping-pong 目标集合。
- Behavior after:
  - `PostFxSharedResources` 创建并拥有：
    - `PostFX_Shared_Half_Ping`
    - `PostFX_Shared_Half_Pong`
    - `PostFX_Shared_Quarter_Ping`
    - `PostFX_Shared_Quarter_Pong`
  - 以上目标都是 window-relative（`0.5x` 与 `0.25x`），因此天然具备正确的 resize 生命周期。
  - `RendererSystemFrostedGlass` 现在通过 getter 暴露这些 handle，作为后续 B1 multi-pass 实现的稳定接线点。
- Key runtime flow:
  - `RendererSystemFrostedGlass::Init` 会先初始化共享 postfx 资源。
  - Frosted 输出目标重命名为 `PostFX_Frosted_Output`，并使用同一套 postfx usage flag。
  - 当前 frosted pass 的执行路径保持不变。

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - 本次迭代没有新增 shader 逻辑。
  - 本次迭代没有 pass topology 变化。
- Core algorithm changes:
  - 这是基础设施步骤，用于为未来 downsample / blur / composite 分阶段提供资源图输入。
- Parameter / model changes:
  - 新 helper 类型：`PostFxSharedResources`，包含 resolution enum 和 ping-pong pair 抽象。

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/PostFxSharedResources.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`

## 5. 验证与证据

- Functional checks:
  - 接入新的 shared resource helper 后，`RendererDemo` verify build 成功。
- Visual checks:
  - 本次迭代未执行（基础设施变更）。
- Performance checks:
  - 本次迭代未执行。
- Evidence files / links:
  - MSBuild summary:
    - `build_logs/rendererdemo_20260223_195837.msbuild.log`
  - Standard outputs:
    - `build_logs/rendererdemo_20260223_195837.stdout.log`
    - `build_logs/rendererdemo_20260223_195837.stderr.log`
  - Binlog:
    - `build_logs/rendererdemo_20260223_195837.binlog`

## 6. 验收结果

- Status: PASS（A3.1 foundation scope）
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 26
  - Error count: 0
  - A3 只部分达成：共享资源基础已到位，多 pass 实际消费仍在 B1。

## 7. 风险与后续

- Known risks:
  - 这些共享目标目前只是被分配和暴露，还没有被真正消费，因此运行时视觉行为不变。
  - 在未来接入消费 pass 之前，resize 压力下的 descriptor / binding 行为仍需运行时验证。
- Next tasks:
  - B1.1：接入使用 `PostFxSharedResources` 的 downsample + blur stage。
  - A4.x：按类似共享资源约定接入 history 生命周期。
