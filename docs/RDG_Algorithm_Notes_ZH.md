# RDG 算法说明

配套文档：

- 共享关键类说明图：`docs/RendererFramework_KeyClass_Diagram.md`
- 英文版本：`docs/RDG_Algorithm_Notes.md`

## 1. 范围

本文说明当前 RenderGraph（RDG）算法，主要实现位于：

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
  - `CollectResourceAccess`
  - `BuildExecutionPlan`
  - `ApplyExecutionPlanResult`
  - `ExecuteRenderGraphFrame`
- `glTFRenderer/RendererCore/Public/RendererInterface.h`
  - `DependencyDiagnostics`
  - `ValidationPolicy`

重点覆盖：

- 依赖推断
- 执行计划构建
- 缓存校验
- 拓扑排序
- 诊断信息发布

## 2. 数据模型

### 2.1 节点级输入

每个节点的依赖输入来自两部分：

- 显式依赖：node desc 里的 `dependency_render_graph_nodes`
- draw info 中声明的资源访问（buffer / texture / render target）

其中资源访问抽取从 `CollectResourceAccess` 开始。

### 2.2 资源键空间

RDG 会把资源统一归一成 `(kind, value)`：

- `ResourceKind::Buffer`
- `ResourceKind::Texture`
- `ResourceKind::RenderTarget`

相关类型：

- `ResourceKey`
- `ResourceKind`

### 2.3 边表示

依赖关系存为邻接表：

- `DependencyEdgeMap = map<Node, set<Node>>`
- 语义是 `from -> to`

## 3. 访问分类规则

`CollectResourceAccess()` 会把每个 binding 归类到 `reads` / `writes`：

- Buffer
  - `CBV`、`SRV` => 读
  - `UAV` => 读 + 写
- Texture
  - `SRV` => 读
  - `UAV` => 读 + 写
- RenderTargetTexture binding
  - `SRV` => 读
  - `UAV` => 读 + 写
- RenderTarget attachment
  - 永远写
  - 如果 `load_op == LOAD`，同时也读

## 4. 依赖推断

### 4.1 Readers / Writers 索引

`CollectResourceReadersAndWriters()` 会构建：

- `resource -> readers set`
- `resource -> writers set`

### 4.2 推断边构建

`BuildResourceInferredEdges()` 对每个资源的逻辑是：

1. `consumers = writers U readers`
2. 遍历每个 `writer`
3. 对每个 `consumer != writer` 添加 `writer -> consumer`

这样就能对同一资源建立：

- writer-before-use
- writer-before-writer

## 5. 显式依赖校验

`ValidateDependencyPlan()` 会把显式依赖合并进推断图，并校验每条显式边。

显式依赖有效当且仅当：

- handle 有效
- 不是自依赖
- index 没越界
- 依赖节点在本帧已注册

无效显式边会被收集并标记到诊断信息里。

## 6. 执行签名与缓存键

`ComputeExecutionSignature()` 会对下列信息做哈希：

- node handle 列表
- node 资源读集合
- node 资源写集合
- 显式依赖列表
- 合并后的 outgoing edges

这个签名和 node 数量共同作为执行缓存键。

## 7. 缓存顺序校验

`ValidateExecutionOrder()` 用当前图去验证缓存顺序是否仍然成立：

- node 数一致
- 所有 node 唯一且存在
- 对任意边 `u -> v`，都有 `order[u] < order[v]`

## 8. 拓扑排序与环检测

`TopologicalSortExecutionNodes()` 会：

- 计算 indegree
- 把 zero-indegree 节点放进有序集合
- 每次弹出最小 handle，并松弛其 outgoing edges

如果输出数量不等于 node 数，则剩余 indegree > 0 的节点就是 cycle nodes。

## 9. 计划构建流程

`BuildExecutionPlan()` 的主要阶段：

1. `CollectResourceAccessPlan()`
2. `ValidateDependencyPlan()`
3. `ComputeExecutionSignature()`
4. cache-key 对比
5. cached order 校验
6. 是否需要重建的判定

## 10. Apply 阶段语义

`ApplyExecutionPlanResult()` 的行为：

- 如果不需要 rebuild：保留当前 cache
- 如果存在无效显式依赖：标记图无效，清空 execution order，跳过执行
- 否则运行拓扑排序：
  - 排序成功：更新 cache order，图有效
  - 出现环：图无效，清空 cache，并返回 cycle nodes

## 11. 诊断模型

`UpdateDependencyDiagnostics()` 会发布：

- graph validity
- invalid explicit edges
- auto-merged dependency count
- execution signature
- cached node/order size
- cycle nodes

公开诊断类型：

- `RenderGraph::DependencyDiagnostics`

## 12. 运行时集成

RDG 算法在 `ExecuteRenderGraphFrame()` 中每帧运行一次：

1. 快照本帧已注册节点
2. 构建 plan context
3. 构建 plan
4. apply plan / 更新 cache
5. 更新 diagnostics
6. 按缓存顺序执行，并收集 timing stats

## 13. 复杂度（高层）

记：

- `N` = 本帧已注册节点数
- `E` = 合并后的依赖边数
- `A` = 所有节点的 binding 项总数

则大致复杂度为：

- access collection: `O(A log N)`
- inferred edge build: `O(sum over resources of W_r * C_r)`
- cached order validation: `O(N + E)`，有序容器会带来额外 log 因子
- topological sort: `O((N + E) log N)`

当前实现使用 `std::map` / `std::set`，优先保证确定性顺序，而不是极限吞吐。

## 14. 正确性不变量

当 `graph_valid == true`：

- cached execution order 必须恰好包含所有 registered nodes 一次
- 对任意依赖边 `u -> v`，`u` 必须出现在 `v` 之前

当存在无效显式依赖或 cycle：

- `graph_valid == false`
- execution order 被清空
- 为安全起见跳过 pass 执行

## 15. 扩展检查表

如果引入新的 binding 类型或资源种类：

1. 扩展 `CollectResourceAccess()` 分类逻辑
2. 确保 `ComputeExecutionSignature()` 能观察到其访问变化
3. 只有在引入新失败模式时才扩展 diagnostics 字段
4. 至少测试三类场景：
   - 只有推断依赖，没有显式依赖
   - 显式依赖 handle 合法
   - 无效显式依赖或 cycle

## 16. 实际调试路径

当 frame graph 行为不对时：

1. 先看 UI 里的 diagnostics，确认是否有 invalid edges / cycle
2. 看 cache key 是否变化但执行顺序没有重建
3. 检查节点 draw bindings，确认 `CollectResourceAccess()` 的读写分类是否正确
4. 检查显式 `dependency_render_graph_nodes` 的源节点注册时机

---

## 附录：最小伪代码

```text
nodes = registered_nodes_this_frame
inferred_edges = infer_from_resource_access(nodes)
combined_edges, invalid_edges = merge_and_validate_explicit(nodes, inferred_edges)

signature = hash(nodes, accesses, explicit_deps, combined_edges)
need_rebuild = cache_key_changed || graph_validity_transition || cached_order_invalid

if need_rebuild:
  if invalid_edges exists:
    graph_valid = false
    cached_order = []
  else:
    sorted, order, cycle_nodes = topo_sort(nodes, combined_edges)
    if sorted:
      graph_valid = true
      cached_order = order
    else:
      graph_valid = false
      cached_order = []

publish_diagnostics(...)
if graph_valid:
  execute(cached_order)
```
