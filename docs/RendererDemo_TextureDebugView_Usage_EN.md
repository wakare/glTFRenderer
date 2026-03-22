# RendererDemo Texture Debug View Usage

Companion references:

- Shared dependency diagram: `docs/RendererDemo_TextureDebugView_Diagram.md`
- Chinese companion: `docs/RendererDemo_TextureDebugView_Usage.md`

## 1. Goal and Scope

`RendererSystemTextureDebugView` replaces the old pattern of temporarily wiring arbitrary texture output through the tone-map pass with one independent base module.

Its current responsibilities are:

- maintain a stable debug-source registry
- resolve the upstream render target and dependency node for the selected source
- produce the final debug visualization through its own compute pass
- share one state model across debug UI, snapshots, and regression capture

In scope:

- the maintained `RendererDemo` path
- `DemoAppModelViewer`
- `DemoAppModelViewerFrostedGlass`
- intermediate render-result inspection, screenshot regression, and issue isolation

Out of scope:

- the legacy `glTFApp` path
- RenderDoc capture workflow itself
- direct CPU-side texture dumps

## 2. Runtime Model

The current runtime contract is:

1. `DemoAppModelViewer::FinalizeModelViewerRuntimeObjects(...)` registers stable `source id` values.
2. Every source resolves through `SourceResolver`, which returns:
   - `render_target`
   - `dependency_node`
3. `RendererSystemTextureDebugView` resolves the active source in `Init(...)` / `Tick(...)` and chooses either the color or scalar visualization path.
4. The system synchronizes the render-graph node only when the resolved `render_target` or `dependency_node` changes. It does not rebuild setup unconditionally every frame.
5. `RendererSystemToneMap` remains responsible only for tone mapping; `TextureDebugView` merely reuses its final output and tone-map parameters.

As a result, adding a new debug source no longer requires changing the tone-map path. The standard work is:

- expose the upstream output
- register a new source
- add a regression case when the source should stay covered long term

## 3. Current Stable Source IDs

These source ids are already wired for reuse by the debug UI, snapshot JSON, and regression suites.

| `source_id` | Producer | Visualization | Defaults | Notes |
| --- | --- | --- | --- | --- |
| `final.tonemapped` | `RendererSystemToneMap` | `Color` | `scale=1.0`, `bias=0.0`, `apply_tonemap=false` | Final LDR output for comparison with normal present. |
| `scene.color` | `RendererSystemSceneRenderer` | `Color` | `1.0`, `0.0`, `false` | Base-pass color. |
| `scene.normal` | `RendererSystemSceneRenderer` | `Color` | `1.0`, `0.0`, `false` | Base-pass normal. |
| `scene.velocity` | `RendererSystemSceneRenderer` | `Velocity` | `scale=32.0` | Uses the dedicated velocity-scale UI. |
| `scene.depth` | `RendererSystemSceneRenderer` | `Scalar` | `scale=-1.0`, `bias=1.0` | Defaults are chosen to make depth easier to inspect. |
| `ssao.raw` | `RendererSystemSSAO` | `Scalar` | `1.0`, `0.0` | Raw SSAO output. |
| `ssao.final` | `RendererSystemSSAO` | `Scalar` | `1.0`, `0.0` | Final blurred SSAO output. |
| `lighting.output` | `RendererSystemLighting` | `Color` | `1.0`, `0.0`, `true` | HDR lighting output; tone map is enabled by default. |
| `frosted.output` | `RendererSystemFrostedGlass` | `Color` | `1.0`, `0.0`, `true` | Available only in the frosted demo path. |

Stability rules:

- do not casually rename an existing `source_id`
- snapshot restore and regression cases depend on these identifiers
- when semantics change incompatibly, prefer adding a new id over silently reusing the old one

## 4. Debug UI and Snapshot

`Texture Debug View` appears as an independent runtime system in the debug UI. The current controls mean:

- `Source`: switches the source and restores that source's default parameters
- `Scale` / `Bias`: used for `Color` and `Scalar`
- `Velocity Scale`: used only for `Velocity`
- `Apply ToneMap`: meaningful only for `Color` sources

`DemoAppModelViewer` snapshots now serialize and restore:

- `tonemap`
- `texture_debug`

The `texture_debug` JSON block looks like this:

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

That keeps manual inspection, saved snapshots, and regression capture on one shared restore path.

## 5. Regression Usage

`RegressionLogicPack` now parses the generic `debug_view_*` arguments before any pack-specific logic. This means even:

```json
{
  "logic_pack": "none"
}
```

can still switch the debug output by itself.

Supported generic arguments:

- `debug_view_source`: string, mapped to a stable `source_id`
- `debug_view_scale`: number
- `debug_view_bias`: number
- `debug_view_tonemap`: boolean

Minimal example:

```json
{
  "logic_pack": "none",
  "logic_args": {
    "debug_view_source": "ssao.final"
  }
}
```

HDR example:

```json
{
  "logic_pack": "none",
  "logic_args": {
    "debug_view_source": "lighting.output",
    "debug_view_tonemap": true
  }
}
```

Long-lived smoke suites in the repo:

- `glTFRenderer/RendererDemo/Resources/RegressionSuites/model_viewer_texture_debug_smoke.json`
- `glTFRenderer/RendererDemo/Resources/RegressionSuites/frosted_glass_texture_debug_smoke.json`

Recommended entry points:

- build: `powershell -ExecutionPolicy Bypass -File .\scripts\Build-RendererDemo-Verify.ps1`
- regression capture: `powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 -Suite <suite-path> -Backend vk -DemoApp <demo-name>`

## 6. Recommended Way to Add a New Debug Source

When adding a source, keep this order:

1. Expose a stable upstream output.
   - At minimum, provide the `render_target`.
   - Prefer exposing the matching `dependency_node` as well.
2. Register a `DebugSourceDesc` in `DemoAppModelViewer::FinalizeModelViewerRuntimeObjects(...)`.
3. Choose the correct `VisualizationMode`: `Color`, `Scalar`, or `Velocity`.
4. Pick sensible defaults:
   - HDR color usually wants `default_apply_tonemap = true`
   - depth often needs non-default `scale` / `bias`
   - velocity usually needs a larger default scale
5. Add at least one suite case when the source should remain regression-covered.

Primary code entry points:

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemTextureDebugView.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemTextureDebugView.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`
- `glTFRenderer/RendererDemo/Regression/RegressionLogicPack.cpp`

## 7. Implementation Constraints and Maintenance Rules

These are the constraints that should stay intact as the module evolves:

- do not move new temporary outputs back into `RendererSystemToneMap`
- do not register the same render-graph node twice across `Init(...)` and the first `Tick(...)`
- do not rebuild pass setup unconditionally every frame in `Tick(...)`
- every source resolver must return the real current `render_target` and the correct `dependency_node`
- `Apply ToneMap` should affect only color sources

These rules directly affect:

- Vulkan descriptor and node lifetime stability
- whether long warmup regression runs keep reallocating resources
- whether snapshot and JSON restore remain reliable

## 8. Validation Status

As of `2026-03-22`, the path has been validated with:

- a successful quiet `RendererDemo` build
- `model_viewer_texture_debug_smoke`: 4/4 cases passed
- `frosted_glass_texture_debug_smoke`: 1/1 case passed

This is the baseline that allows `TextureDebugView` to serve as the standard entry point for intermediate-render-result validation going forward.
