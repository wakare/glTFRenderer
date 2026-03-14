# Feature Note - B1.3 Frosted Mask/Parameter 阶段拆分

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260224_B1_FrostedMaskPass.md`

- Date: 2026-02-24
- Work Item ID: B1.3
- Title: 增加显式的 mask/parameter 阶段，并让 composite 通过中间 panel 数据消费
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. 范围

- In scope:
  - 在 frosted composite 之前增加 full-resolution 的 mask / parameter compute pass。
  - 增加 full-resolution 的 panel mask / optics 专用中间 render target。
  - 更新 frosted composite shader，使其消费预先计算好的 mask / optics 数据，而不再在 composite 中重复评估 panel shape 逻辑。
  - 保持 half / quarter blur pyramid 路径不变，并复用现有 panel buffer schema。
- Out of scope:
  - Temporal accumulation / stabilization。
  - 不规则 `ShapeMask` 的真实实现路径。
  - 与冻结基线的最终视觉 / 性能验收。

## 2. 功能逻辑

- Behavior before:
  - Frosted 路径先执行 downsample / blur，再用一个 composite pass 同时完成 panel SDF / mask / optics 评估。
  - Composite pass 会直接采样 scene depth / normal，并逐像素计算 panel 几何逻辑。
- Behavior after:
  - Frosted 路径现在被拆成：
    1. `Downsample Half`
    2. `Blur Half Horizontal`
    3. `Blur Half Vertical`
    4. `Downsample Quarter`
    5. `Blur Quarter Horizontal`
    6. `Blur Quarter Vertical`
    7. `Frosted Mask/Parameter`
    8. `Frosted Composite`
  - `Frosted Mask/Parameter` 会写出：
    - `MaskParamOutput`（mask / rim / fresnel / panel index）
    - `PanelOpticsOutput`（refraction uv / effective blur / scene edge）
  - `Frosted Composite` 现在改为消费：
    - `MaskParamTex`
    - `PanelOpticsTex`
    - 以及现有的 half / quarter blurred texture
- Key runtime flow:
  - `Lighting -> Blur Pyramid -> Mask/Parameter -> Composite -> Frosted Output`

## 3. 算法与渲染逻辑

- Passes / shaders touched:
  - `RendererSystemFrostedGlass` 的 pass graph wiring 和资源创建。
  - `FrostedGlass.hlsl` 被拆分为 `MaskParameterMain` 和 `CompositeMain`。
- Core algorithm changes:
  - 把逐像素 panel mask / optics 推导从 composite 中抽到独立 compute pass。
  - Composite 改为消费打包后的中间数据，主要关注 blur reconstruction 和 highlight composition。
  - 这一版对重叠 panel 仍采用 strongest local mask payload 作为 composite 输入。
- Parameter / model changes:
  - CPU 侧 panel schema 无变化。
  - 新增两个 full-resolution postfx 中间目标：
    - `PostFX_Frosted_MaskParameter`
    - `PostFX_Frosted_PanelOptics`

## 4. 变更文件

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`

## 5. 验证与证据

- Functional checks:
  - 接入 mask / parameter 拆分后，`RendererDemo` verify build 成功。
- Visual checks:
  - 本次迭代未执行（互动验证待补）。
- Performance checks:
  - 本次迭代未执行（timing 采集待补）。
- Evidence files / links:
  - Build wrapper summary:
    - 仅记录本地工件名，这些文件不入库。
    - `b13_verify_20260224_115727.stdout.log`
    - `b13_verify_20260224_115727.stderr.log`
  - MSBuild logs:
    - 仅记录本地工件名，这些文件不入库。
    - `rendererdemo_20260224_115728.msbuild.log`
    - `rendererdemo_20260224_115728.stdout.log`
    - `rendererdemo_20260224_115728.stderr.log`
    - `rendererdemo_20260224_115728.binlog`

## 6. 验收结果

- Status: PASS（B1.3 implementation scope，build validation）
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 0
  - Error count: 0
  - B1 仍在进行中，直到补齐视觉 / 性能验收证据。

## 7. 风险与后续

- Known risks:
  - 当前的 overlap 行为仍然使用 strongest local mask payload，未来在 `B3` 中会替换为更明确的 layering policy。
  - 运动中的画质（边缘稳定性 / halo）仍需运行时验证。
- Next tasks:
  - B1 验收：抓 v1 / v2 对比截图和 per-pass timing。
  - B2：接入 panel 状态机和运行时参数过渡。
  - B3：用显式 layering policy 替换当前 overlap fallback。
