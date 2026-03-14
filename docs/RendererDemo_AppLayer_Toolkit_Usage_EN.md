# RendererDemo App-Layer Toolkit Usage

Companion references:

- Shared key-class diagram: `docs/RendererFramework_KeyClass_Diagram.md`
- Chinese companion: `docs/RendererDemo_AppLayer_Toolkit_Usage.md`
- Design companion: `docs/RendererDemo_AppLayer_Toolkit_Design.md`

## 1. Scope

This document describes the recommended way to add demos, modules, and render features on the maintained `RendererDemo` path.

The preferred model today is:

- `DemoBase` owns runtime hosting.
- `RendererSystemBase` defines feature lifecycle.
- `RenderPassSetupBuilder.h` provides app-layer authoring helpers.

## 2. Adding a New Demo

### 2.1 Inherit from `DemoBase`

New demos should derive from `DemoBase` and only implement demo-specific logic:

- `InitInternal(...)`
- `TickFrameInternal(...)`
- `DrawDebugUIInternal()` when needed

Do not start the render-graph main loop directly inside demo-specific logic. The standard host entry points are:

- `DemoBase::CreateRenderRuntimeContext`
- `DemoBase::InitializeRuntimeModulesAndSystems`
- `DemoBase::StartRenderGraphExecution`

### 2.2 Register modules and systems through the host

Recommended order:

1. Create modules.
2. Create systems.
3. Store them in `m_modules` / `m_systems`.
4. Let `DemoBase` drive `module->Tick(...)` and `system->Tick(...)`.

This keeps resize, RHI rebuild, snapshots, debug UI, and demo switching aligned with one host-managed path.

## 3. Adding a New System

### 3.1 Minimum contract

A new system should at least implement:

- `Init(...)`
- `Tick(...)`
- `ResetRuntimeResources(...)`
- `OnResize(...)`

### 3.2 Recommended structure

Split the system into four layers:

1. typed outputs
2. execution plan
3. setup builder
4. runtime orchestration

Representative layout:

```cpp
struct MyFeatureOutputs
{
    RendererInterface::RenderTargetHandle output{NULL_HANDLE};
};

struct MyFeatureExecutionPlan
{
    RenderFeature::ComputeExecutionPlan compute_plan{};
    RendererInterface::RenderTargetHandle input{NULL_HANDLE};
};
```

Then separate:

- `BuildMyFeatureExecutionPlan(...)`
- `BuildMyFeaturePassSetupInfo(...)`
- `SyncMyFeatureSetup(...)`
- `Tick(...)`

## 4. Choosing Typed Outputs

When one feature consumes another feature's results:

- prefer explicit output structs
- do not fall back to string-based lookup
- do not read another system's private members directly

Recommended:

```cpp
const auto scene_outputs = m_scene->GetOutputs();
const auto lighting_outputs = m_lighting->GetOutputs();
```

## 5. Writing a Compute Feature

`ToneMap` is the cleanest compute-feature reference today.

Recommended steps:

1. Build a dispatch plan with `RenderFeature::ComputeExecutionPlan`.
2. Build setup with `RenderFeature::PassBuilder::Compute(...)`.
3. Use `CreateRenderGraphNodeIfNeeded(...)` in `Init(...)`.
4. Use `execution_plan.compute_plan.ApplyDispatch(...)` in `Tick(...)`.
5. Register the node at the end of `Tick(...)`.

## 6. Writing a Graphics Feature

`SceneRenderer` is the cleanest graphics-feature reference today.

Recommended steps:

1. Represent viewport state with `RenderFeature::GraphicsExecutionPlan`.
2. Call `.SetViewport(execution_plan.graphics_plan)` inside `Build*SetupInfo(...)`.
3. Detect viewport or other static setup changes inside `Tick(...)`.
4. Rebuild setup with `SyncRenderGraphNode(...)` when needed.

## 7. Writing a Multi-pass Feature

`FrostedGlass` is the main reference for multi-pass feature authoring.

Recommended order:

1. bundle the resources first
2. factor pass setup through helpers or builders
3. split `Init()` / `Tick()` into orchestration plus helpers
4. connect static setup sync to the tick path

Do not start with one oversized function that mixes:

- resource creation
- setup construction
- dispatch updates
- runtime-path policy
- node registration

## 8. When to Use `CreateRenderGraphNodeIfNeeded`

Use it when:

- the node is created once during init
- setup is not expected to change later

Examples:

- lightweight compute passes
- fixed passes without viewport, size, or binding-topology changes

## 9. When to Use `SyncRenderGraphNode`

Use it when:

- the node may already exist
- setup may need rebuilds because viewport, topology, or resource path changed

Examples:

- base-pass viewport updates
- lighting topology updates
- screen-size-dependent graphics or compute setup

## 10. When to Use `CreateOrSyncRenderGraphNode`

Use it when:

- one helper must support both initial creation and later synchronization
- you want to avoid duplicating create/sync branches inside the feature

The current `FrostedGlass` static-pass setup path follows this pattern.

## 11. When to Introduce `FrameSizedSetupState`

Prefer it when:

- setup depends on render width or height
- rebuild is not required every frame
- `OnResize(...)` should not directly own `RenderGraph`

Recommended pattern:

1. `OnResize(...)` only calls `Reset()`.
2. `Tick(...)` checks `Matches(...)`.
3. Static setup is synchronized only when dimensions changed.

## 12. Expected Responsibility of `ResetRuntimeResources(...)`

It should:

- clear node handles
- clear render-target and buffer handles
- reset history, cache, and setup state

It should not:

- depend on old resources remaining valid
- assume nodes survive a resize or rebuild
- keep stale frame-sized cache alive

## 13. Common Pitfalls

### 13.1 Rebuilding nodes directly inside `OnResize(...)`

Problem:

- `OnResize(...)` only receives `ResourceOperator`
- it is not the stable place to rebuild render-graph state

Preferred fix:

- mark dirty only
- synchronize setup inside `Tick(...)`

### 13.2 Using strings as feature output contracts

Problem:

- rename and refactor safety is poor
- dependency tracking becomes opaque

Preferred fix:

- convert to typed outputs

### 13.3 Mixing plan build, setup build, and node registration in `Tick(...)`

Problem:

- code size grows quickly
- resize policy, dispatch update, and runtime policy become hard to reason about independently

Preferred fix:

- split into `Build*ExecutionPlan(...)`
- `Build*SetupInfo(...)`
- `Sync*Setup(...)`
- `Register*Nodes(...)`

## 14. Current Recommended Templates

- lightweight compute feature: `RendererSystemToneMap`
- graphics base-pass feature: `RendererSystemSceneRenderer`
- compute plus topology-sync feature: `RendererSystemLighting`
- multi-pass plus history plus static-sync feature: `RendererSystemFrostedGlass`

## 15. Companion Documents

To understand the broader structure:

- `docs/RendererFramework_Architecture_Summary.md`
- `docs/RendererFramework_Architecture_Summary_EN.md`
- `docs/RendererDemo_AppLayer_Toolkit_Design.md`

To understand the underlying RenderGraph algorithm:

- `docs/RDG_Algorithm_Notes.md`
- `docs/RDG_Algorithm_Notes_ZH.md`
