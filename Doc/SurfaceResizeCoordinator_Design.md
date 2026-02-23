# Surface Resize Coordinator Design

## 1. Goal

Split swapchain/window-resize logic into:

- `ResourceManagerSurfaceResizeCoordinator`
- `ResourceManagerSurfaceResourceRebuilder`

while keeping `ResourceManager` public API unchanged.

## 2. Components

### 2.1 `ResourceManagerSurfaceResizeCoordinator`

Responsibilities:

- frame-level surface sync orchestration
- lifecycle/status transitions
- acquire/present failure notification handling
- resize request invalidation entry

Key entry points:

- `ResourceManagerSurfaceResizeCoordinator::Sync`
- `ResourceManagerSurfaceResizeCoordinator::NotifySwapchainAcquireFailure`
- `ResourceManagerSurfaceResizeCoordinator::NotifySwapchainPresentFailure`
- `ResourceManagerSurfaceResizeCoordinator::InvalidateSwapchainResizeRequest`

Implementation:

- `glTFRenderer/RendererCore/Private/ResourceManagerSurfaceSync.cpp`
- declarations in `glTFRenderer/RendererCore/Private/ResourceManagerSurfaceSync.h`

### 2.2 `ResourceManagerSurfaceResourceRebuilder`

Responsibilities:

- in-place swapchain resize and fallback recreate flow
- resize retry/cooldown behavior
- window-relative render target rebuild

Key entry points:

- `ResourceManagerSurfaceResourceRebuilder::ResizeSwapchainIfNeeded`
- `ResourceManagerSurfaceResourceRebuilder::ResizeWindowDependentRenderTargets`

## 3. Call Flow

`ResourceManager::SyncWindowSurface` now delegates to:

1. `ResourceManagerSurfaceResizeCoordinator::Sync`
2. coordinator calls rebuilder resize functions when needed
3. coordinator returns `WindowSurfaceSyncResult`

This preserves the old caller contract in `RendererInterface::ResourceOperator` and `RenderGraph`.

## 4. Failure Recovery

DX12 path:

- attempt `ResizeBuffers`
- if failed:
  - keep current swapchain alive
  - restore current swapchain RT descriptors
  - enter `RESIZE_DEFERRED` and retry with cooldown

Non-DX12 path:

- fallback to swapchain recreate on in-place resize failure

Both paths still rely on the same lifecycle transitions and deferred resource release policy.

## 5. Why This Refactor Helps

- separates "state decision" and "resource mutation"
- reduces `ResourceManager` function size and branch density
- makes future cross-RHI policy tuning easier without touching all ResourceManager logic
- keeps behavior stable for existing systems and tests
