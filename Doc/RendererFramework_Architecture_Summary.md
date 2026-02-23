# 渲染框架结构与关键机制总结（结合核心代码）

> 本文基于当前主干代码，重点覆盖：
> 1) 框架结构与调用链
> 2) 资源生命周期管理
> 3) RDG 依赖推导与执行计划
> 4) Resize / Swapchain 重建相关逻辑
>
> 代码入口主要位于：`RendererCore`、`RHICore`、`RendererDemo`。

## 1. 框架总体结构

### 1.1 分层

- **应用层（Demo）**：组织系统、注册 Tick / DebugUI、启动事件循环。  
  参考：`glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`
- **渲染编排层（RendererInterface::RenderGraph）**：
  - 维护 render graph node 集合
  - 推导/校验依赖
  - 执行 pass
  - 维护 descriptor 资源缓存和延迟释放
  参考：`glTFRenderer/RendererCore/Public/RendererInterface.h:126`, `glTFRenderer/RendererCore/Private/RendererInterface.cpp:1501`
- **资源与设备抽象层（ResourceOperator / ResourceManager）**：
  - 对外暴露创建/上传/resize/swapchain 生命周期接口
  - 管理 command list、swapchain RT、window-relative RT 重建
  参考：`glTFRenderer/RendererCore/Public/RendererInterface.h:63`, `glTFRenderer/RendererCore/Private/ResourceManager.cpp`
- **RHI 实现层（DX12 / Vulkan）**：
  - swapchain acquire/present/resize
  - API 特定同步细节
  参考：`glTFRenderer/RHICore/Private/RHIDX12Impl/DX12SwapChain.cpp:206`, `glTFRenderer/RHICore/Private/RHIVKImpl/VKSwapChain.cpp:182`

### 1.2 主循环调用链（关键）

1. Demo 初始化渲染上下文并构造 `ResourceOperator` / `RenderGraph`。  
   参考：`glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`
2. 注册业务 Tick 与 DebugUI 回调到 `RenderGraph`。  
   参考：`glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`
3. ModelViewer 完成系统搭建后调用 `CompileRenderPassAndExecute()`，把渲染逻辑挂到窗口 Tick。  
   参考：`glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp:77`, `glTFRenderer/RendererCore/Private/RendererInterface.cpp:1371`
4. `Run()` 进入窗口事件循环，驱动每帧执行。  
   参考：`glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp:423`, `glTFRenderer/RendererCore/Private/RendererInterface.cpp:742`

## 2. 每帧执行结构（RenderGraph）

当前已经重构为“阶段化”风格，`OnFrameTick` 只做编排：

- `PrepareFrameForRendering(...)`
- `ExecuteRenderGraphFrame(...)`
- `BlitFinalOutputToSwapchain(...)`
- `FinalizeFrameSubmission(...)`

参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:1381`

### 2.1 帧准备上下文（FramePreparationContext）

将 `window_width/window_height/profiler_slot_index/command_list` 统一收敛到一个结构，降低函数间参数耦合。  
参考：`glTFRenderer/RendererCore/Public/RendererInterface.h:239`

### 2.2 帧准备阶段拆分

- 解析最终输出 RT：`ResolveFinalColorOutput()`  
  参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:1395`
- 执行业务 Tick + ImGui NewFrame：`ExecuteTickAndDebugUI()`  
  参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:1413`
- Surface 同步 + 帧推进：`SyncWindowSurfaceAndAdvanceFrame()`  
  参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:1433`
- Acquire + 命令上下文：`AcquireCurrentFrameCommandContext()`  
  参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:1458`

## 3. 资源生命周期管理

资源生命周期分成两类：

- **全局资源生命周期**（ResourceManager 主导）
- **RenderGraph 描述符生命周期**（RenderGraph 主导）

### 3.1 全局资源：创建、延期释放、全局清理

#### 创建

- `ResourceOperator` 构造时根据 API 初始化 `RHIConfigSingleton`，并创建 `ResourceManager`。  
  参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:938`
- `ResourceManager::InitResourceManager()` 初始化 factory/device/queue/swapchain/memory manager/command lists/RT manager。  
  参考：`glTFRenderer/RendererCore/Private/ResourceManager.cpp`

#### 延迟释放

`ResourceManager` 维护 `m_deferred_release_entries`：

- 入队：`EnqueueResourceForDeferredRelease(...)`  
  参考：`glTFRenderer/RendererCore/Private/ResourceManager.cpp`
- 每次 `SyncWindowSurface(...)` 前推进帧号并释放到期资源：`AdvanceDeferredReleaseFrame()`  
  参考：`glTFRenderer/RendererCore/Private/ResourceManager.cpp`, `glTFRenderer/RendererCore/Private/ResourceManager.cpp`
- 强制释放：`FlushDeferredResourceReleases(true)`  
  参考：`glTFRenderer/RendererCore/Private/ResourceManager.cpp`

#### 全局清理（用于切换 RHI）

`ResourceOperator::CleanupAllResources(...)`：

1. `WaitFrameRenderFinished()` + `WaitGPUIdle()`
2. `RHIResourceFactory::CleanupResources(...)`
3. `memory_manager.ReleaseAllResource()`
4. 清理 handle table（可选是否清理窗口 handle）

参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:1195`

这就是当前“全局清理资源接口”的核心落点。

### 3.2 RenderGraph 描述符资源：按需重建 + 延迟回收

`RenderGraph` 对每个 node 维护 `RenderPassDescriptorResource` 缓存：

- Buffer descriptor 缓存
- Texture descriptor / descriptor table 缓存
- 上一帧绑定快照（用于判断是否需要重建）

参考：`glTFRenderer/RendererCore/Public/RendererInterface.h:228`

关键策略：

1. 每帧 `PruneDescriptorResources(...)` 清除 draw_info 中已不存在的绑定并延迟释放。  
   参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:2037`
2. 如果 binding 描述发生变化，则重建 descriptor（并把旧 descriptor 入延迟释放队列）。  
   参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp`
3. 针对 RT resize 后底层 `m_source` 指针变化，额外检测 descriptor 绑定源是否已经变更，必要时强制重建，避免“句柄不变但底层资源已变”的悬挂绑定。  
   参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp`
4. 对不再活跃节点，按保留帧数回收 descriptor 资源。  
   参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:2136`

此外，RenderGraph 自己也有一条 descriptor 资源延迟释放队列：

- 入队：`RenderGraph::EnqueueResourceForDeferredRelease`  
  参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:1945`
- 释放：`RenderGraph::FlushDeferredResourceReleases`  
  参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:2162`

## 4. RDG 依赖推导与执行计划

### 4.1 资源访问抽取

对每个 node 从 draw_info 抽取 `reads/writes`：

- Buffer: `CBV/SRV`=读，`UAV`=读写
- Texture: `SRV`=读，`UAV`=读写
- RenderTargetTexture: `SRV`=读，`UAV`=读写
- RenderTargetAttachment: 默认写；若 `load_op==LOAD`，额外记为读

参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:101`

### 4.2 自动推导边

`BuildResourceInferredEdges(...)`：对每个资源建立 `writer -> consumer(writer∪reader)` 边。  
参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:197`

### 4.3 合并显式依赖并校验

`ValidateDependencyPlan(...)` 将显式 `dependency_render_graph_nodes` 合并进边图，同时校验依赖合法性（存在、非自依赖、已注册）。  
参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:508`

非法显式依赖会：

- 标记 `graph_valid=false`
- 清空执行序
- 当前帧跳过 graph 执行

参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:583`

### 4.4 执行计划缓存与重建

`BuildExecutionPlan(...)` 输出：

- 组合边图
- 自动合并计数
- 执行签名 signature
- cache key 是否变化
- 是否需要重建执行序

参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:537`

`ApplyExecutionPlanResult(...)`：

- 需要重建时执行拓扑排序
- 检测环并记录 cycle nodes
- 更新缓存执行序

参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:561`

### 4.5 诊断输出

`UpdateDependencyDiagnostics(...)` 将 graph 有效性、自动合并边数量、非法边、cycle nodes、签名/缓存规模写入 UI 可读结构。  
参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:641`

另外，RenderGraph 的“校验日志节流 + 跳过策略判断”已经抽到独立策略模块：

- `glTFRenderer/RendererCore/Private/RenderGraphExecutionPolicy.h`
- `glTFRenderer/RendererCore/Private/RenderGraphExecutionPolicy.cpp`

`RenderGraph` 通过 `ValidationPolicy` 暴露配置（日志间隔、是否 warning 即跳过），默认行为保持与历史一致。  
参考：`glTFRenderer/RendererCore/Public/RendererInterface.h`

Demo 侧在 Advanced 面板展示这些诊断：  
参考：`glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp:246`

### 4.6 执行时序

每帧执行顺序：

1. `BuildExecutionPlan`
2. `ApplyExecutionPlanResult`
3. `UpdateDependencyDiagnostics`
4. `ExecutePlanAndCollectStats`
5. `CollectUnusedRenderPassDescriptorResources`

参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:1501`

## 5. Resize / Swapchain 重建逻辑

## 5.1 每帧入口如何触发 resize

`OnFrameTick -> PrepareFrameForRendering -> SyncWindowSurfaceAndAdvanceFrame -> ResourceOperator::SyncWindowSurface`。  
参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:1381`, `glTFRenderer/RendererCore/Private/RendererInterface.cpp:1433`, `glTFRenderer/RendererCore/Private/RendererInterface.cpp:1120`

`SyncWindowSurface(...)` 返回 `WindowSurfaceSyncResult`，状态枚举定义见：  
`glTFRenderer/RendererCore/Public/Renderer.h:318`

## 5.2 ResourceManager 的 resize 状态机

`ResourceManager::SyncWindowSurface(...)` 逻辑：

1. 推进 deferred release 帧并释放到期资源
2. 处理 minimized / runtime invalid
3. 调 `ResizeSwapchainIfNeeded(window_w, window_h)`
4. 调 `ResizeWindowDependentRenderTargetsImpl(render_w, render_h, swapchain_resized)`
5. 生成 `READY/RESIZED/DEFERRED_RETRY/INVALID/MINIMIZED`

参考：`glTFRenderer/RendererCore/Private/ResourceManager.cpp`

状态字段定义：`SwapchainLifecycleState`  
参考：`glTFRenderer/RendererCore/Public/Renderer.h:307`

### 5.2.1 协调器拆分（最新）

为了降低 `ResourceManager` 的复杂度，resize 逻辑已拆分为两个内部组件：

- `ResourceManagerSurfaceResizeCoordinator`：负责状态推进、入口编排与失败通知收敛
- `ResourceManagerSurfaceResourceRebuilder`：负责 swapchain/窗口相关 RT 的实际重建执行

实现位置：

- `glTFRenderer/RendererCore/Private/ResourceManagerSurfaceSync.h`
- `glTFRenderer/RendererCore/Private/ResourceManagerSurfaceSync.cpp`

`ResourceManager` 的公开接口保持不变，仅在内部委托到上述组件执行。

## 5.3 Swapchain resize 关键策略

`ResizeSwapchainIfNeeded(...)` 引入了三类稳定性策略：

- **窗口稳定帧门限**（避免用户拖拽时每帧 resize）
- **失败冷却重试**（可指数退避）
- **按周期节流日志**

参考：`glTFRenderer/RendererCore/Private/ResourceManager.cpp`, `glTFRenderer/RendererCore/Private/ResourceManager.cpp`, `glTFRenderer/RendererCore/Private/ResourceManager.cpp`

DX12 特化路径：

- resize 前显式等待/清理：`WaitFrameRenderFinished + WaitGPUIdle + FlushDeferredResourceReleases(true)`
- 释放 swapchain RT wrapper 后额外 `HostWaitPresentFinished`，降低 flip-model ResizeBuffers 失败概率
- in-place resize 失败时不立刻重建 swapchain，而是恢复当前 swapchain RT 并转入 `RESIZE_DEFERRED` 重试

参考：`glTFRenderer/RendererCore/Private/ResourceManager.cpp`, `glTFRenderer/RendererCore/Private/ResourceManager.cpp`, `glTFRenderer/RendererCore/Private/ResourceManager.cpp`

非 DX12（如 Vulkan）可走 fallback recreate。  
参考：`glTFRenderer/RendererCore/Private/ResourceManager.cpp`

## 5.4 窗口相关 RT 自动 resize

`ResizeWindowDependentRenderTargetsImpl(...)` 会扫描 `m_render_target_descs` 中 `size_mode==WINDOW_RELATIVE` 的 RT：

- 根据 scale/min 计算新尺寸
- 创建新 allocation 并更新 HandleTable
- 旧 allocation 延迟释放

参考：`glTFRenderer/RendererCore/Private/ResourceManager.cpp`

为了减少业务侧负担，`CreateRenderTarget(...)` 还有一个“自动窗口相对化”逻辑：如果创建时是 FIXED 且尺寸恰好等于当前 swapchain，则自动转为 `WINDOW_RELATIVE(1.0,1.0)`。  
参考：`glTFRenderer/RendererCore/Private/ResourceManager.cpp`

这使得大部分主屏 RT（BasePass/Lighting/Frosted 等）即便最初按宽高创建，也能跟随窗口自动重建。

## 5.5 Present 失败恢复

`RenderGraph::Present(...)` 若 present 失败，会调用 `NotifySwapchainPresentFailure()`，交给下一帧 `SyncWindowSurface` 进入重建/恢复路径。  
参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp`, `glTFRenderer/RendererCore/Private/ResourceManager.cpp`

## 5.6 Vulkan 与 DX12 swapchain 实现差异（关键点）

- DX12：`ResizeBuffers` 失败返回 false，由上层 ResourceManager 控制重试节奏。  
  参考：`glTFRenderer/RHICore/Private/RHIDX12Impl/DX12SwapChain.cpp:206`
- Vulkan：创建时会按 surface capabilities clamp extent（避免非法 extent），acquire/present 出现 `OUT_OF_DATE` 返回 false 触发上层恢复。  
  参考：`glTFRenderer/RHICore/Private/RHIVKImpl/VKSwapChain.cpp:55`, `glTFRenderer/RHICore/Private/RHIVKImpl/VKSwapChain.cpp:197`, `glTFRenderer/RHICore/Private/RHIVKImpl/VKSwapChain.cpp:250`

## 6. 与系统层（RendererSystem）的关系

系统侧每帧只需：

- 更新自己的数据（buffer upload / dispatch 维度）
- `RegisterRenderGraphNode(...)` 注册本帧要执行的节点

参考：

- Scene：`glTFRenderer/RendererDemo/RendererSystem/RendererSystemSceneRenderer.cpp:93`
- Lighting：`glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp:224`
- Frosted：`glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp:143`

由于 `FinalizeFrameSubmission()` 末尾会 `clear m_render_graph_node_handles`，所以“按帧注册”是必须行为。  
参考：`glTFRenderer/RendererCore/Private/RendererInterface.cpp:1576`

## 7. 当前设计下的实践建议

1. **切 RHI 或大规模重载时**：优先调用 `CleanupAllResources(true/false)` 走统一清理流程。  
2. **新增屏幕尺寸相关 RT**：优先保证 `size_mode=WINDOW_RELATIVE`（或使用当前已有的自动转换行为）。  
3. **新增 pass 绑定资源时**：尽量保持 binding 名称稳定，能减少 descriptor 重建频率。  
4. **排查画面不更新/卡住**：先看 `SwapchainLifecycleState` 与 `DependencyDiagnostics`（UI 已接入）。

---

## 附：一条完整的帧执行链（简化）

```text
Window Tick
  -> RenderGraph::OnFrameTick
    -> PrepareFrameForRendering
      -> ResolveFinalColorOutput
      -> SyncWindowSurfaceAndAdvanceFrame
        -> ResourceManager::SyncWindowSurface
          -> ResizeSwapchainIfNeeded
          -> ResizeWindowDependentRenderTargetsImpl
      -> AcquireCurrentFrameCommandContext
    -> ExecuteRenderGraphFrame
      -> BuildExecutionPlan
      -> ApplyExecutionPlanResult
      -> ExecutePlanAndCollectStats
        -> ExecuteRenderGraphNode (for each pass)
    -> BlitFinalOutputToSwapchain
    -> FinalizeFrameSubmission
      -> RenderDebugUI
      -> Present
```

