# RendererDemo 应用层封装工具使用说明

配套文档：

- 共享关键类说明图：`docs/RendererFramework_KeyClass_Diagram.md`
- 英文版本：`docs/RendererDemo_AppLayer_Toolkit_Usage_EN.md`
- 设计文档：`docs/RendererDemo_AppLayer_Toolkit_Design.md`

## 1. 适用范围

这份文档给出当前 `RendererDemo` 维护路径下，新增 demo、模块或渲染 feature 时的推荐使用方式。

目标不是覆盖所有历史写法，而是明确当前应优先采用的模式：

- `DemoBase` 负责运行时宿主
- `RendererSystemBase` 负责 feature 生命周期
- `RenderPassSetupBuilder.h` 负责应用层 authoring helper

## 2. 新增一个 demo 的推荐方式

### 2.1 继承 `DemoBase`

新 demo 应该继承 `DemoBase`，并只实现自己的业务初始化与业务 Tick：

- `InitInternal(...)`
- `TickFrameInternal(...)`
- `DrawDebugUIInternal()`（可选）

不要在 demo 自己的逻辑里再直接启动 render graph 主循环。当前标准入口已经是：

- `DemoBase::CreateRenderRuntimeContext`
- `DemoBase::InitializeRuntimeModulesAndSystems`
- `DemoBase::StartRenderGraphExecution`

### 2.2 在宿主中注册模块和系统

推荐顺序：

1. 创建 modules
2. 创建 systems
3. 放进 `m_modules` / `m_systems`
4. 让 `DemoBase` 统一驱动 `module->Tick(...)` 与 `system->Tick(...)`

这样做的原因是：

- RHI switch / rebuild / resize 的回调路径已经和宿主对齐
- Snapshot、Debug UI、Demo switch 都不需要 feature 自己重复接一次

## 3. 新增一个 system 的推荐骨架

### 3.1 最小合同

一个新 system 至少实现：

- `Init(...)`
- `Tick(...)`
- `ResetRuntimeResources(...)`
- `OnResize(...)`

### 3.2 推荐写法

推荐把 system 分成四层：

1. typed outputs
2. execution plan
3. setup builder
4. runtime orchestration

示意：

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

然后把下面这些函数拆开：

- `BuildMyFeatureExecutionPlan(...)`
- `BuildMyFeaturePassSetupInfo(...)`
- `SyncMyFeatureSetup(...)`
- `Tick(...)`

## 4. 如何选择 typed outputs

当一个 feature 需要读取另一个 feature 的结果时：

- 优先使用显式输出结构
- 不要再用字符串约定去拿输出
- 不要直接读另一个 system 的私有成员

推荐：

```cpp
const auto scene_outputs = m_scene->GetOutputs();
const auto lighting_outputs = m_lighting->GetOutputs();
```

不推荐：

```cpp
// 不要依赖字符串名或别的 system 的内部字段
```

## 5. 如何写 compute feature

`ToneMap` 是当前最干净的 compute feature 参考。

推荐步骤：

1. 用 `RenderFeature::ComputeExecutionPlan` 生成 dispatch plan
2. 用 `RenderFeature::PassBuilder::Compute(...)` 构造 setup
3. `Init(...)` 用 `CreateRenderGraphNodeIfNeeded(...)`
4. `Tick(...)` 用 `execution_plan.compute_plan.ApplyDispatch(...)`
5. `Tick(...)` 末尾注册 node

关键参考：

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemToneMap.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemToneMap.cpp`

## 6. 如何写 graphics feature

`SceneRenderer` 是当前最干净的 graphics feature 参考。

推荐步骤：

1. 用 `RenderFeature::GraphicsExecutionPlan` 表达 viewport
2. 在 `Build*SetupInfo(...)` 里调用 `.SetViewport(execution_plan.graphics_plan)`
3. `Tick(...)` 中判断 viewport 或其他静态 setup 是否变化
4. 变化时调用 `SyncRenderGraphNode(...)`

关键参考：

- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSceneRenderer.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSceneRenderer.cpp`

## 7. 如何写多 pass feature

`FrostedGlass` 是当前多 pass feature 的主要样板。

推荐顺序：

1. 先把资源按 bundle 收口
2. 再把 pass setup 通过 helper 或 builder 收口
3. 再把 `Init()` / `Tick()` 拆成 orchestration + helper
4. 最后再把静态 setup sync 接进 tick

不要一上来就在一个大函数里同时处理：

- 资源创建
- setup 构造
- dispatch 更新
- runtime path 选择
- node 注册

那样后面几乎无法继续重构。

## 8. 何时用 `CreateRenderGraphNodeIfNeeded`

适用场景：

- node 只在初始化时创建一次
- setup 后续不会变化

例如：

- 轻量 compute pass
- 没有 viewport / size / binding 拓扑变化的固定 pass

## 9. 何时用 `SyncRenderGraphNode`

适用场景：

- node 可能已经存在
- setup 未来会因为 viewport、拓扑、资源路径变化而重建

例如：

- base pass viewport 变化
- lighting 拓扑变化
- screen-size 相关 graphics/compute setup 变化

## 10. 何时用 `CreateOrSyncRenderGraphNode`

适用场景：

- 同一套 helper 既要支持初始化创建，也要支持后续同步
- 不想在 feature 内部保留一份重复的 create/sync 分支

当前 `FrostedGlass` 的静态 pass setup 就是这个模式。

## 11. 何时引入 `FrameSizedSetupState`

当满足以下条件时，优先引入：

- setup 依赖 render width / height
- 不是每帧都需要 rebuild
- `OnResize(...)` 无法也不应该直接拿 `RenderGraph`

推荐模式：

1. `OnResize(...)` 只 `Reset()`
2. `Tick(...)` 里检查 `Matches(...)`
3. 只有尺寸变化才 `SyncStaticPassSetups(...)`

这样 resize 处理就和 render graph rebuild 保持了解耦。

## 12. `ResetRuntimeResources(...)` 的推荐职责

应该做：

- 清空 node handle
- 清空 render target / buffer handle
- reset history / cache / setup state

不应该做：

- 依赖旧资源继续上传
- 假设 resize 之后 node 仍然有效
- 保留已经无效的 frame-sized cache

## 13. 常见坑

### 13.1 在 `OnResize(...)` 里直接 rebuild node

问题：

- `OnResize(...)` 只有 `ResourceOperator`
- 它不是 render graph rebuild 的稳定位置

做法：

- 只做 dirty 标记
- 在 `Tick(...)` 里同步 setup

### 13.2 继续用字符串当 feature 输出合同

问题：

- 改名和重构都不安全
- 依赖关系难追踪

做法：

- 改成 typed outputs

### 13.3 在 `Tick(...)` 里混写 plan 构建、setup 构建和注册

问题：

- 代码迅速膨胀
- 无法单独验证 resize / runtime policy / dispatch update

做法：

- 拆成 `Build*ExecutionPlan(...)`
- `Build*SetupInfo(...)`
- `Sync*Setup(...)`
- `Register*Nodes(...)`

## 14. 当前推荐模板

### 14.1 轻量 compute feature

- 参考 `RendererSystemToneMap`

### 14.2 graphics base pass feature

- 参考 `RendererSystemSceneRenderer`

### 14.3 compute + topology sync feature

- 参考 `RendererSystemLighting`

### 14.4 多 pass + history + static sync feature

- 参考 `RendererSystemFrostedGlass`

## 15. 文档配套关系

如果要理解更上层结构，继续看：

- `docs/RendererFramework_Architecture_Summary.md`
- `docs/RendererDemo_AppLayer_Toolkit_Design.md`

如果要理解底层 RenderGraph 算法，继续看：

- `docs/RDG_Algorithm_Notes.md`
