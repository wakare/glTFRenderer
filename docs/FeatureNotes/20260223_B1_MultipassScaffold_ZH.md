# Feature Note - B1.1 Frosted Glass 多 Pass 脚手架（Downsample + Blur + Composite）

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260223_B1_MultipassScaffold.md`

- Date: 2026-02-23
- Work Item ID: B1.1
- Title: 用多 pass postfx 脚手架替换 frosted 单 pass 处理
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. 范围

- In scope:
  - 引入多 pass 的 frosted graph wiring：
    - half-resolution downsample pass
    - half-resolution separable blur horizontal / vertical pass
    - full-resolution composite pass
  - 在运行时 pass 中消费 `PostFxSharedResources` 的 half ping-pong 资源。
  - 保留现有 panel 参数模型和 debug UI 控件。
- Out of scope:
  - 专门的 mask / parameter 预处理 pass。
  - quarter-resolution blur pyramid 的消费。
  - Temporal history 累积和 reprojection。

## 2. 功能逻辑

- Behavior before:
  - Frosted 效果由一个全屏 compute pass 完成，并在 pass 内部做逐像素邻域 blur 循环。
- Behavior after:
  - Frosted 效果改为分阶段 postfx 路径：
    1. `Downsample Half`：full-res lighting color -> half-res ping
    2. `Blur Half Horizontal`：half ping -> half pong
    3. `Blur Half Vertical`：half pong -> half ping
    4. `Frosted Composite`：full-res panel logic + half blurred color -> final frosted output
  - Composite 继续保留 panel edge / rim / refraction / fresnel 逻辑，但读取的是预模糊纹理。
  - `Blur Sigma` 仍然通过基于 sigma 的 blurred contribution 调制来生效。
- Key runtime flow:
  - `RendererSystemFrostedGlass::Tick` 每帧更新所有 pass 的 dispatch size，并注册所有节点。
  - Composite pass 显式依赖 blur vertical pass，从而保证顺序稳定。

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlassPostfx.hlsl`（新增）：
    - `DownsampleMain`
    - `BlurHorizontalMain`
    - `BlurVerticalMain`
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`：
    - 现在改为采样 `BlurredColorTex`，不再在 full-resolution 上内联执行空间 blur 循环。
- Core algorithm changes:
  - 把 composite 中昂贵的逐像素 2D blur 改成独立 pass 的 separable preblur。
  - Composite 现在是在预过滤信号上叠加 shape / depth / refraction / fresnel 权重。
- Parameter / model changes:
  - CPU 侧 panel schema 无变化。
  - `FrostedGlassGlobalBuffer.blur_radius` 现在直接控制 blur pass kernel radius。

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlassPostfx.hlsl`

## 5. 验证与证据

- Functional checks:
  - 接入 multipass node 和 shader 后，`RendererDemo` verify build 成功。
- Visual checks:
  - 本次迭代记录中未执行（仅 build 验证范围）。
- Performance checks:
  - 本次迭代未执行。
- Evidence files / links:
  - MSBuild summary:
    - `build_logs/rendererdemo_20260223_210529.msbuild.log`
  - Standard outputs:
    - `build_logs/rendererdemo_20260223_210529.stdout.log`
    - `build_logs/rendererdemo_20260223_210529.stderr.log`
  - Binlog:
    - `build_logs/rendererdemo_20260223_210529.binlog`

## 6. 验收结果

- Status: PASS（B1.1 scaffold scope）
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 0
  - Error count: 0
  - B1 整体仍在进行中；这次迭代只建立了多 pass 运行时基础。

## 7. 风险与后续

- Known risks:
  - Quarter-resolution 资源虽然已经准备好，但还没有在 frosted blur pyramid 中被消费。
  - 运行时视觉 / 性能行为仍需通过场景验证和 timing 采集确认。
- Next tasks:
  - B1.2：接入 quarter-level pyramid 和更细化的 reconstruction / composite 策略。
  - B1.3：拆出显式的 mask / parameter stage，并对照 v1 基线验证质量 / 性能。
