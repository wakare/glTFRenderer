# RendererDemo 应用层封装工具设计

## 1. 目标

这份文档描述当前 `RendererDemo` 维护路径里，应用层为了组织渲染系统而形成的封装工具集合。这里的“应用层”指：

- `glTFRenderer/RendererDemo/DemoApps/*`
- `glTFRenderer/RendererDemo/RendererSystem/*`
- `glTFRenderer/RendererDemo/RendererModule/*`

本文只覆盖当前已经落地、并且已经在 `SceneRenderer` / `Lighting` / `ToneMap` / `FrostedGlass` 中实际使用的设计，不讨论旧的 `glTFApp` 路径。

## 2. 设计动机

在最近几轮重构之前，`RendererDemo` 的问题不在底层 `RHICore`，而在上层编排逐步失去统一模式：

- demo 宿主、系统初始化、RHI 重建、Debug UI、Snapshot 都压在 `DemoBase`
- feature 之间既有显式依赖，也有字符串取输出、直接摸内部资源的隐式依赖
- pass setup、dispatch 更新、resize 同步、render graph node 注册在各系统里各写一套

重构目标不是推翻现有 `RenderGraph`，而是在 `RendererDemo` 应用层上建立一套统一的 feature authoring 工具箱，让复杂 feature 也能沿着同一种生命周期往下写。

## 3. 当前分层

### 3.1 `DemoBase` 作为运行时宿主

`DemoBase` 现在承担应用运行时宿主角色，主要负责：

- 创建 `RenderWindow` / `ResourceOperator` / `RenderGraph`
- 注册 `TickFrame(...)` 与 Debug UI 回调
- 统一初始化 modules / systems
- 启动 `RenderGraph` 执行
- 处理 runtime RHI switch、demo switch、snapshot、resize 通知

关键入口：

- `glTFRenderer/RendererDemo/DemoApps/DemoBase.h`
- `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`
- `DemoBase::CreateRenderRuntimeContext`
- `DemoBase::InitializeRuntimeModulesAndSystems`
- `DemoBase::StartRenderGraphExecution`
- `DemoBase::TickFrame`

这意味着 demo app 本身不再负责自己启动 `CompileRenderPassAndExecute()`，而是把“创建哪些模块/系统”交给宿主，把“如何跑起来”交给宿主层。

### 3.2 `RendererSystemBase` 作为 feature 生命周期合同

每个 feature system 都遵循 `RendererSystemBase` 的合同：

- `Init(...)`
- `Tick(...)`
- `ResetRuntimeResources(...)`
- `OnResize(...)`
- `DrawDebugUI()`

当前更明确的约定是：

- `Init(...)` 负责第一次创建资源和 node
- `Tick(...)` 负责每帧同步 setup、更新 dispatch/状态、注册 node
- `ResetRuntimeResources(...)` 负责资源级清空
- `OnResize(...)` 负责运行态失效标记，而不是强依赖即时 rebuild

## 4. 应用层工具箱

当前工具箱主要集中在 `glTFRenderer/RendererDemo/RendererSystem/RenderPassSetupBuilder.h`。

### 4.1 数据合同工具

#### typed outputs

系统间输出已经逐步改成显式结构，而不是字符串查找：

- `RendererSystemSceneRenderer::BasePassOutputs`
- `RendererSystemLighting::LightingOutputs`

作用：

- 把 feature 依赖变成可搜索、可重构、可编译检查的接口
- 降低“知道另一个系统内部成员名字”的耦合

### 4.2 frame 维度工具

`RenderFeature::FrameDimensions`

作用：

- 统一当前帧宽高获取
- 统一 downsample 尺寸推导
- 统一 compute group count 推导

它是后续 execution plan 和 static setup sync 的基础输入。

### 4.3 execution plan 工具

共享 plan：

- `RenderFeature::ComputeExecutionPlan`
- `RenderFeature::GraphicsExecutionPlan`

feature-local plan：

- `RendererSystemSceneRenderer::BasePassExecutionPlan`
- `RendererSystemLighting::LightingExecutionPlan`
- `RendererSystemToneMap::ToneMapExecutionPlan`
- `RendererSystemFrostedGlass::TickExecutionPlan`

设计原则：

- 先把“本帧执行需要的输入数据”对象化
- 再让 `Build*SetupInfo(...)`、`Sync*Setup(...)`、dispatch 更新等逻辑消费 plan

这样做的好处是，`Tick()` 和 `Init()` 会逐步退化成 orchestration，而不是参数拼装场。

### 4.4 pass authoring 工具

`RenderFeature::PassBuilder`

当前已经承接：

- graphics / compute pass setup 构造
- shader / module / dependency 装配
- render target / sampled target / buffer binding 装配
- viewport / dispatch / execute command 装配

配套 helper 包括：

- `MakeRenderTargetAttachment(...)`
- `MakeBufferBinding(...)`
- `MakeSampledRenderTargetBinding(...)`
- `MakeComputePostFxPassBuilder(...)`

设计目标很明确：保留现有 `RenderGraph::RenderPassSetupInfo` 执行模型，但不要让 feature 继续直接手写一大段 setup 结构体。

### 4.5 node lifecycle 工具

共享 node 生命周期 helper：

- `CreateRenderGraphNodeIfNeeded(...)`
- `SyncRenderGraphNode(...)`
- `CreateOrSyncRenderGraphNode(...)`
- `RegisterRenderGraphNodeIfValid(...)`
- `RegisterRenderGraphNodesIfValid(...)`

这些 helper 解决的是 feature 层最常见的样板代码：

- “第一次创建，以后重建”
- “有些 node 本帧不一定启用，但注册时不想手写空句柄判断”
- “多节点路径批量注册”

### 4.6 静态 setup 同步工具

共享状态：

- `RenderFeature::FrameSizedSetupState`

当前用法：

- `FrostedGlass` 在 `Tick()` 中先判断尺寸是否变化
- 尺寸变了才走 `SyncStaticPassSetups(...)`
- `OnResize(...)` 只做失效标记，不直接持有 `RenderGraph` 做 rebuild

这套模式适合：

- setup 明显依赖窗口/渲染尺寸
- node 持久存在，但 setup 不是每帧都需要重建
- feature 自己要把 resize 和 tick 解耦

## 5. 当前推荐生命周期模式

当前推荐的 feature 编排顺序如下。

### 5.1 `Init(...)`

1. 创建 runtime resources
2. 构建 feature-local execution plan 或 init bindings
3. 创建 render graph nodes
4. 写入初始 runtime state / cache state

### 5.2 `Tick(...)`

1. 构建 execution plan
2. 按需同步静态 setup
3. 更新本帧 buffer / dispatch / runtime summary
4. 注册本帧要执行的 nodes
5. 推进 history / frame-local state

### 5.3 `OnResize(...)`

只做这些事：

- history invalidation
- cache reset
- static setup dirty 标记

不推荐在这里做的事：

- 直接 rebuild render graph node
- 直接依赖 `RenderGraph`
- 直接重新创建整套 feature 资源，除非 feature 的设计本身要求完全销毁重建

## 6. 已落地的使用样例

### 6.1 `SceneRenderer`

特点：

- graphics feature
- 使用 `BasePassExecutionPlan`
- 使用 `SyncBasePassSetup(...)` 按 viewport 变化重建 setup

关键文件：

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSceneRenderer.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSceneRenderer.cpp`

### 6.2 `Lighting`

特点：

- compute 主通路 + directional shadow graphics 子通路
- `LightingExecutionPlan` 统一相机模块和 compute dispatch
- `SyncLightingTopology(...)` 处理光源拓扑变化

关键文件：

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`

### 6.3 `ToneMap`

特点：

- 最轻量的 compute feature
- `ToneMapExecutionPlan` 作为标准 compute feature 样板

关键文件：

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemToneMap.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemToneMap.cpp`

### 6.4 `FrostedGlass`

特点：

- 多 path、多 bundle、多 history 的复杂 feature
- 同时覆盖了 resource bundle、execution plan、static setup sync、runtime plan、批量注册等模式
- 是当前应用层工具箱最完整的压力测试对象

关键文件：

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`

## 7. 设计边界

当前这套应用层工具箱有意不处理这些问题：

- 多队列 / async compute 调度策略
- RenderGraph compiler / executor 的深层拆分
- runtime-local 资源注册表替代全局 handle table
- editor/game 双 runtime 并存

这些问题还在更上层或更底层，当前阶段先保证 `RendererDemo` feature authoring 和生命周期足够一致。

## 8. 当前结论

当前 `RendererDemo` 应用层已经形成了一个可复用的渲染特征开发模式：

- demo 宿主负责 runtime orchestration
- feature 通过 typed outputs 建依赖
- pass 通过 builder authoring
- setup / dispatch / register 通过 execution plan 和 lifecycle helper 统一

这套模式已经不只是局部重构，而是当前维护路径里默认推荐的写法。后续新增 feature 或继续拆 `DemoBase`，都应该优先建立在这套工具箱之上，而不是再回到 feature 各写一套生命周期代码。
