# Renderer Framework Architecture Summary

## 1. Scope

This document is the English companion of `docs/RendererFramework_Architecture_Summary.md`.

It summarizes the current maintained rendering path:

- `RendererDemo`
- `RendererCore`
- `RHICore`

It focuses on:

- layer split
- frame flow
- resource lifecycle
- RenderGraph execution model
- resize / swapchain behavior
- current app-layer feature toolkit

Shared class diagram:

- `docs/RendererFramework_KeyClass_Diagram.md`

## 2. Layer Split

### 2.1 App runtime host

`DemoBase` is now the runtime host for the maintained demo path. It is responsible for:

- creating `RenderWindow`, `ResourceOperator`, and `RenderGraph`
- initializing modules and systems
- driving `TickFrame(...)`
- hosting debug UI
- handling runtime RHI switch and runtime rebuild

Key files:

- `glTFRenderer/RendererDemo/DemoApps/DemoBase.h`
- `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`

### 2.2 Render orchestration layer

`RendererInterface::RenderGraph` owns:

- render graph node lifecycle
- dependency inference and validation
- execution-plan build/apply
- pass execution
- debug diagnostics
- final present

Key files:

- `glTFRenderer/RendererCore/Public/RendererInterface.h`
- `glTFRenderer/RendererCore/Private/RendererInterface.cpp`

### 2.3 Resource and device abstraction

`ResourceOperator` / `ResourceManager` provide:

- resource creation and upload
- frame extent query
- window-relative resource lifecycle
- swapchain and surface sync entry points

### 2.4 RHI layer

`RHICore` provides backend-specific device, swapchain, and synchronization behavior for DX12 and Vulkan.

## 3. Current App-Layer Toolkit

The main improvement in recent refactors is not a backend rewrite. It is the new shared authoring pattern in `RendererDemo`.

The shared toolkit lives mainly in:

- `glTFRenderer/RendererDemo/RendererSystem/RenderPassSetupBuilder.h`

Current shared building blocks:

- `RenderFeature::FrameDimensions`
- `RenderFeature::ComputeExecutionPlan`
- `RenderFeature::GraphicsExecutionPlan`
- `RenderFeature::FrameSizedSetupState`
- `RenderFeature::PassBuilder`
- `RenderFeature::CreateRenderGraphNodeIfNeeded`
- `RenderFeature::SyncRenderGraphNode`
- `RenderFeature::CreateOrSyncRenderGraphNode`
- `RenderFeature::RegisterRenderGraphNodeIfValid`
- `RenderFeature::RegisterRenderGraphNodesIfValid`

This toolkit is already used by:

- `RendererSystemSceneRenderer`
- `RendererSystemLighting`
- `RendererSystemToneMap`
- `RendererSystemFrostedGlass`

## 4. Feature Contract

Feature systems are expected to follow the `RendererSystemBase` lifecycle:

- `Init(...)`
- `Tick(...)`
- `ResetRuntimeResources(...)`
- `OnResize(...)`
- `DrawDebugUI()`

Current recommended behavior:

- `Init(...)` creates resources and nodes
- `Tick(...)` builds execution plans, syncs setup when needed, updates dispatch/state, and registers nodes
- `OnResize(...)` invalidates runtime/setup state instead of directly rebuilding graph nodes

## 5. Typed Feature Dependencies

Feature dependencies are now moving away from string-based output lookup and implicit internal-state access.

Examples:

- `RendererSystemSceneRenderer::BasePassOutputs`
- `RendererSystemLighting::LightingOutputs`

This makes feature wiring easier to refactor and easier to audit.

## 6. Frame Flow

The simplified frame path is:

1. `DemoBase` tick callback drives app-layer modules and systems
2. `RenderGraph::OnFrameTick(...)` orchestrates frame work
3. `PrepareFrameForRendering(...)`
4. `ExecuteRenderGraphFrame(...)`
5. `BlitFinalOutputToSwapchain(...)`
6. `FinalizeFrameSubmission(...)`

## 7. Resource Lifecycle

There are two major ownership domains:

- global runtime resources managed by `ResourceManager`
- render-pass descriptor resources managed by `RenderGraph`

Global resource cleanup is centralized in:

- `ResourceOperator::CleanupAllResources(...)`

Descriptor resources are rebuilt or pruned by `RenderGraph` as node bindings and underlying resources change.

## 8. Resize And Setup Sync

One important current design rule is to avoid forcing graph rebuilds directly from `OnResize(...)`.

Instead:

1. `OnResize(...)` invalidates local runtime/setup state
2. `Tick(...)` detects size-dependent setup changes
3. the feature rebuilds setup through `SyncRenderGraphNode(...)` or `CreateOrSyncRenderGraphNode(...)`

This is already applied in:

- `RendererSystemSceneRenderer`
- `RendererSystemFrostedGlass`

## 9. Practical View Of Feature Roles

- `SceneRenderer`: base pass and GBuffer producer
- `Lighting`: lighting/shadow feature and lighting-output producer
- `FrostedGlass`: complex multi-pass post-lighting feature with runtime path selection
- `ToneMap`: final tone-map/output stage

See the shared dependency diagram:

- `docs/RendererFramework_KeyClass_Diagram.md`

## 10. Current Conclusion

The most valuable recent progress is above the backend:

- runtime orchestration is clearer
- feature contracts are more explicit
- pass authoring is less repetitive
- resize/setup synchronization is more coherent

This app-layer toolkit is now the recommended base for future rendering-feature work in the maintained `RendererDemo` path.
