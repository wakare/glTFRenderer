# RendererDemo App-Layer Toolkit Design

Companion references:

- Shared key-class diagram: `docs/RendererFramework_KeyClass_Diagram.md`
- Chinese companion: `docs/RendererDemo_AppLayer_Toolkit_Design.md`

## 1. Goal

This document describes the app-layer authoring toolkit currently used on the maintained `RendererDemo` path to organize render features.

Here, "app layer" means:

- `glTFRenderer/RendererDemo/DemoApps/*`
- `glTFRenderer/RendererDemo/RendererSystem/*`
- `glTFRenderer/RendererDemo/RendererModule/*`

The document only covers patterns that have already landed and are now exercised by `SceneRenderer`, `Lighting`, `ToneMap`, and `FrostedGlass`. The legacy `glTFApp` runtime path is intentionally out of scope.

## 2. Why This Toolkit Exists

Before the recent refactor sequence, the main problems were not in `RHICore` but in top-level orchestration drift:

- `DemoBase` carried demo hosting, system bootstrap, RHI rebuild flow, debug UI, and snapshots in one place.
- Feature dependencies were a mix of typed links, string-based lookups, and direct access to another system's internals.
- Pass setup, dispatch updates, resize handling, and node registration were repeated independently in each feature.

The goal was not to replace `RenderGraph`, but to establish a consistent feature-authoring toolkit on top of the maintained `RendererDemo` path.

## 3. Layer Split

### 3.1 `DemoBase` as runtime host

`DemoBase` is now the runtime host for the demo layer. It is responsible for:

- creating `RenderWindow`, `ResourceOperator`, and `RenderGraph`
- registering `TickFrame(...)` and debug UI callbacks
- initializing modules and systems
- starting render-graph execution
- handling runtime RHI switching, demo switching, snapshots, and resize notifications

Key entry points:

- `glTFRenderer/RendererDemo/DemoApps/DemoBase.h`
- `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`
- `DemoBase::CreateRenderRuntimeContext`
- `DemoBase::InitializeRuntimeModulesAndSystems`
- `DemoBase::StartRenderGraphExecution`
- `DemoBase::TickFrame`

The demo app itself no longer starts `CompileRenderPassAndExecute()` directly.

### 3.2 `RendererSystemBase` as the feature lifecycle contract

Each feature system follows the `RendererSystemBase` contract:

- `Init(...)`
- `Tick(...)`
- `ResetRuntimeResources(...)`
- `OnResize(...)`
- `DrawDebugUI()`

Current expectations are:

- `Init(...)` creates initial resources and nodes.
- `Tick(...)` synchronizes setup, updates dispatch or runtime state, and registers active nodes.
- `ResetRuntimeResources(...)` clears resource-level state.
- `OnResize(...)` marks runtime state dirty instead of forcing immediate graph rebuilds.

## 4. Toolkit Inventory

The toolkit is primarily centered in `glTFRenderer/RendererDemo/RendererSystem/RenderPassSetupBuilder.h`.

### 4.1 Data-contract helpers

Typed outputs are replacing string-based output lookup:

- `RendererSystemSceneRenderer::BasePassOutputs`
- `RendererSystemLighting::LightingOutputs`

This makes feature contracts searchable, refactorable, and compile-time checked.

### 4.2 Frame-dimension helpers

`RenderFeature::FrameDimensions` standardizes:

- current frame width and height
- downsampled dimensions
- compute dispatch group sizing

It serves as the common input for execution plans and frame-sized setup sync.

### 4.3 Execution-plan helpers

Shared plans:

- `RenderFeature::ComputeExecutionPlan`
- `RenderFeature::GraphicsExecutionPlan`

Feature-local plans:

- `RendererSystemSceneRenderer::BasePassExecutionPlan`
- `RendererSystemLighting::LightingExecutionPlan`
- `RendererSystemToneMap::ToneMapExecutionPlan`
- `RendererSystemFrostedGlass::TickExecutionPlan`

The design rule is simple: objectify per-frame execution inputs first, then let setup builders, sync helpers, and dispatch updates consume that object.

When a maintained-path feature or refactor exposes a missing `RendererCore` or `RHICore` capability, the default response should be to widen that framework layer instead of preserving a feature-local workaround just to avoid infrastructure work.

### 4.4 Pass authoring helpers

`RenderFeature::PassBuilder` currently covers:

- graphics and compute pass setup construction
- shader, module, and dependency assembly
- render-target, sampled-target, and buffer binding assembly
- viewport, dispatch, and execute-command assembly

This keeps the existing `RenderGraph::RenderPassSetupInfo` execution model while avoiding large hand-written setup structs inside each feature.

### 4.5 Node lifecycle helpers

Shared helpers include:

- `CreateRenderGraphNodeIfNeeded(...)`
- `SyncRenderGraphNode(...)`
- `CreateOrSyncRenderGraphNode(...)`
- `RegisterRenderGraphNodeIfValid(...)`
- `RegisterRenderGraphNodesIfValid(...)`

These cover the common app-layer boilerplate around create/rebuild/register flows.

### 4.6 Static setup sync helpers

Shared state:

- `RenderFeature::FrameSizedSetupState`

Current `FrostedGlass` usage shows the intended pattern:

- `Tick()` checks whether frame-sized setup still matches.
- Static pass setup is only rebuilt when dimensions change.
- `OnResize(...)` only invalidates state instead of holding `RenderGraph` directly.

## 5. Recommended Lifecycle Pattern

### 5.1 `Init(...)`

1. Create runtime resources.
2. Build feature-local execution plans or initial bindings.
3. Create render-graph nodes.
4. Initialize runtime/cache state.

### 5.2 `Tick(...)`

1. Build execution plans.
2. Synchronize static setup if needed.
3. Update per-frame buffers, dispatch, and runtime summaries.
4. Register active nodes for the frame.
5. Advance history or frame-local state.

### 5.3 `OnResize(...)`

Recommended responsibilities:

- history invalidation
- cache reset
- static-setup dirty marking

Avoid doing these directly inside `OnResize(...)`:

- rebuilding render-graph nodes
- depending on `RenderGraph`
- recreating the entire feature unless the feature design explicitly requires it

## 6. Implemented Examples

### 6.1 `SceneRenderer`

- graphics feature
- uses `BasePassExecutionPlan`
- uses `SyncBasePassSetup(...)` for viewport-aware setup rebuilds

### 6.2 `Lighting`

- compute-dominant path with directional-shadow graphics subpath
- `LightingExecutionPlan` unifies camera access and compute dispatch
- `SyncLightingTopology(...)` handles topology changes

### 6.3 `ToneMap`

- minimal compute feature
- `ToneMapExecutionPlan` is the standard compute-feature template

### 6.4 `FrostedGlass`

- complex multi-path, multi-bundle, history-heavy feature
- exercises resource bundles, execution plans, static setup sync, runtime plans, and batched registration
- acts as the main stress test of the current app-layer toolkit

## 7. Design Boundaries

This toolkit intentionally does not solve:

- multi-queue or async-compute scheduling policy
- deep `RenderGraph` compiler/executor decomposition
- replacing the global handle table with runtime-local resource registries
- editor/game dual-runtime coexistence

Those concerns remain above or below the current app-layer boundary. The current priority is consistent feature authoring on the maintained `RendererDemo` path.

## 8. Conclusion

The maintained `RendererDemo` path now has a reusable feature-development pattern:

- the demo host owns runtime orchestration
- features depend on typed outputs
- passes are authored through builders
- setup, dispatch, and registration are unified through execution plans and lifecycle helpers

This is no longer a local cleanup. It is now the default writing style for new features on the maintained path.

The same principle applies to framework evolution: if real product work proves that the current renderer or RHI surface is too narrow, extend it and let the feature return to a cleaner shape. Avoid treating “do not touch `RendererCore` / `RHICore`” as a goal by itself.
