# Surface Resize Coordinator 设计说明

配套文档：

- 共享关键类说明图：`docs/RendererFramework_KeyClass_Diagram.md`
- 英文版本：`docs/SurfaceResizeCoordinator_Design.md`

## 1. 目标

在不改变 `ResourceManager` 对外 API 的前提下，把 swapchain / window resize 逻辑拆成：

- `ResourceManagerSurfaceResizeCoordinator`
- `ResourceManagerSurfaceResourceRebuilder`

## 2. 组件划分

### 2.1 `ResourceManagerSurfaceResizeCoordinator`

职责：

- 帧级 surface 同步编排
- 生命周期与状态流转
- acquire / present 失败通知处理
- resize 请求失效入口

关键入口：

- `ResourceManagerSurfaceResizeCoordinator::Sync`
- `ResourceManagerSurfaceResizeCoordinator::NotifySwapchainAcquireFailure`
- `ResourceManagerSurfaceResizeCoordinator::NotifySwapchainPresentFailure`
- `ResourceManagerSurfaceResizeCoordinator::InvalidateSwapchainResizeRequest`

实现位置：

- `glTFRenderer/RendererCore/Private/ResourceManagerSurfaceSync.cpp`
- 声明位于 `glTFRenderer/RendererCore/Private/ResourceManagerSurfaceSync.h`

### 2.2 `ResourceManagerSurfaceResourceRebuilder`

职责：

- 原地 swapchain resize 与回退 recreate 流程
- resize 重试与 cooldown 策略
- window-relative render target 重建

关键入口：

- `ResourceManagerSurfaceResourceRebuilder::ResizeSwapchainIfNeeded`
- `ResourceManagerSurfaceResourceRebuilder::ResizeWindowDependentRenderTargets`

## 3. 调用流程

`ResourceManager::SyncWindowSurface` 现在委托给：

1. `ResourceManagerSurfaceResizeCoordinator::Sync`
2. coordinator 在需要时调用 rebuilder 的 resize 逻辑
3. coordinator 返回 `WindowSurfaceSyncResult`

这样就保留了 `RendererInterface::ResourceOperator` 与 `RenderGraph` 侧原有的调用合同。

## 4. 失败恢复

DX12 路径：

- 先尝试 `ResizeBuffers`
- 如果失败：
  - 保留当前 swapchain
  - 恢复当前 swapchain RT descriptor
  - 进入 `RESIZE_DEFERRED`，并按 cooldown 重试

非 DX12 路径：

- 原地 resize 失败后回退到 swapchain recreate

两条路径仍共享同一套生命周期状态流转和 deferred resource release 策略。

## 5. 这次重构的收益

- 把“状态决策”和“资源变更”拆开
- 降低 `ResourceManager` 单个函数的体积和分支密度
- 让未来 cross-RHI 策略调优更容易落地
- 对现有调用方和测试保持行为稳定
