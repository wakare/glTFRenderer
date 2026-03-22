# RendererDemo Texture Debug View 使用说明

配套文档：

- 共享依赖图：`docs/RendererDemo_TextureDebugView_Diagram.md`
- 英文版本：`docs/RendererDemo_TextureDebugView_Usage_EN.md`

## 1. 目标与范围

`RendererSystemTextureDebugView` 的目标是把“临时把某个 texture 挂到 tonemap pass 上输出”的做法收口成一个独立基础模块。

当前标准职责是：

- 维护稳定的调试 source 注册表
- 根据 source 解析上游 render target 和 dependency node
- 用独立 compute pass 做最终调试可视化输出
- 让 Debug UI、snapshot 和 regression 共用同一套状态模型

适用范围：

- `RendererDemo` 维护路径
- `DemoAppModelViewer`
- `DemoAppModelViewerFrostedGlass`
- 中间渲染结果观察、截图回归、问题定位

不在范围：

- `glTFApp` 旧路径
- RenderDoc capture 工作流本身
- 直接导出 CPU-side 纹理 dump

## 2. 运行时模型

当前运行时约定如下：

1. `DemoAppModelViewer::FinalizeModelViewerRuntimeObjects(...)` 负责注册稳定 `source id`。
2. 每个 source 通过 `SourceResolver` 返回两个量：
   - `render_target`
   - `dependency_node`
3. `RendererSystemTextureDebugView` 在 `Init(...)` / `Tick(...)` 中解析当前 source，并选择 color 或 scalar 可视化 pass。
4. 模块只在 source 的 `render_target` 或 `dependency_node` 变化时同步 render-graph node，不做每帧无条件 rebuild。
5. `RendererSystemToneMap` 只保留 tone mapping 职责；`TextureDebugView` 仅复用其最终输出与 tone map 参数。

这意味着后续新增调试源时，不需要再改 tonemap 主路径，只需要：

- 暴露上游输出
- 注册新 source
- 视情况补回归 case

## 3. 当前稳定 Source ID

下表描述当前已经接入的稳定 source。它们都可以被 Debug UI、snapshot JSON 和 regression suite 复用。

| `source_id` | Producer | Visualization | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `final.tonemapped` | `RendererSystemToneMap` | `Color` | `scale=1.0`, `bias=0.0`, `apply_tonemap=false` | 最终 LDR 输出，用于和正常 present 对照。 |
| `scene.color` | `RendererSystemSceneRenderer` | `Color` | `1.0`, `0.0`, `false` | Base pass 颜色。 |
| `scene.normal` | `RendererSystemSceneRenderer` | `Color` | `1.0`, `0.0`, `false` | Base pass 法线。 |
| `scene.velocity` | `RendererSystemSceneRenderer` | `Velocity` | `scale=32.0` | 使用速度专用缩放 UI。 |
| `scene.depth` | `RendererSystemSceneRenderer` | `Scalar` | `scale=-1.0`, `bias=1.0` | 默认把深度翻成更适合观察的灰度。 |
| `ssao.raw` | `RendererSystemSSAO` | `Scalar` | `1.0`, `0.0` | SSAO 原始输出。 |
| `ssao.final` | `RendererSystemSSAO` | `Scalar` | `1.0`, `0.0` | SSAO 模糊后的最终输出。 |
| `lighting.output` | `RendererSystemLighting` | `Color` | `1.0`, `0.0`, `true` | HDR lighting 输出，默认启用 tone map 观察。 |
| `frosted.output` | `RendererSystemFrostedGlass` | `Color` | `1.0`, `0.0`, `true` | 仅 frosted demo 可用。 |

稳定性要求：

- 不要轻易改动已有 `source_id` 字符串
- snapshot / regression 依赖这些 id 做状态恢复
- 如果语义已经变化到不兼容，优先新增新 id，而不是复用旧 id

## 4. Debug UI 与 Snapshot

`Texture Debug View` 作为独立 system 出现在运行时 Debug UI 中。当前 UI 语义如下：

- `Source`：切换当前 source，并恢复该 source 的默认参数
- `Scale` / `Bias`：适用于 `Color` 和 `Scalar`
- `Velocity Scale`：只适用于 `Velocity`
- `Apply ToneMap`：只对 `Color` source 有意义

`DemoAppModelViewer` 的 snapshot 已经把下面两部分都纳入 JSON：

- `tonemap`
- `texture_debug`

`texture_debug` 段格式如下：

```json
{
  "texture_debug": {
    "source_id": "scene.depth",
    "scale": -1.0,
    "bias": 1.0,
    "apply_tonemap": false
  }
}
```

这让 UI 观察、手工 snapshot 和 regression capture 共享同一套恢复路径。

## 5. Regression 使用方式

`RegressionLogicPack` 现在会先解析通用的 `debug_view_*` 参数，再执行各自的 logic pack 逻辑。因此即使：

```json
{
  "logic_pack": "none"
}
```

仍然可以单独切换调试输出。

当前支持的通用参数：

- `debug_view_source`：字符串，对应稳定 `source_id`
- `debug_view_scale`：数值
- `debug_view_bias`：数值
- `debug_view_tonemap`：布尔值

最小示例：

```json
{
  "logic_pack": "none",
  "logic_args": {
    "debug_view_source": "ssao.final"
  }
}
```

HDR 观察示例：

```json
{
  "logic_pack": "none",
  "logic_args": {
    "debug_view_source": "lighting.output",
    "debug_view_tonemap": true
  }
}
```

仓库内长期保留的 smoke suite：

- `glTFRenderer/RendererDemo/Resources/RegressionSuites/model_viewer_texture_debug_smoke.json`
- `glTFRenderer/RendererDemo/Resources/RegressionSuites/frosted_glass_texture_debug_smoke.json`

推荐验证入口：

- 构建：`powershell -ExecutionPolicy Bypass -File .\scripts\Build-RendererDemo-Verify.ps1`
- 回归 capture：`powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 -Suite <suite-path> -Backend vk -DemoApp <demo-name>`

## 6. 新增一个调试 Source 的推荐方式

新增 source 时，按下面顺序做：

1. 让上游 system 暴露稳定输出。
   - 至少要能拿到 `render_target`
   - 最好同时暴露对应 `dependency_node`
2. 在 `DemoAppModelViewer::FinalizeModelViewerRuntimeObjects(...)` 注册 `DebugSourceDesc`。
3. 明确选择 `VisualizationMode::Color`、`Scalar` 或 `Velocity`。
4. 设置合适的默认值：
   - HDR 颜色通常 `default_apply_tonemap = true`
   - 深度常常需要 `scale` / `bias`
   - 速度通常需要更高的默认缩放
5. 如果后续要长期回归，补至少一个 suite case。

推荐代码入口：

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemTextureDebugView.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemTextureDebugView.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`
- `glTFRenderer/RendererDemo/Regression/RegressionLogicPack.cpp`

## 7. 实现约束与维护规则

这部分是后续继续扩展时必须保留的约束：

- 不要把新的临时输出重新塞回 `RendererSystemToneMap`
- 不要在 `Init(...)` 和首帧 `Tick(...)` 重复注册同一 render-graph node
- 不要在 `Tick(...)` 里每帧无条件重建 pass setup
- 调试 source 的 resolver 必须返回真实的当前 `render_target` 和正确的 `dependency_node`
- `Apply ToneMap` 只应影响颜色类 source

这些约束直接关系到：

- Vulkan descriptor / node 生命周期稳定性
- regression warmup 长跑是否会持续分配资源
- snapshot / JSON 恢复是否可靠

## 8. 已验证内容

截至 `2026-03-22`，这套路径已经完成以下验证：

- `RendererDemo` quiet build 通过
- `model_viewer_texture_debug_smoke`：4/4 case 通过
- `frosted_glass_texture_debug_smoke`：1/1 case 通过

这意味着它已经可以作为后续中间渲染结果验证的统一入口来使用。
