# RDG Algorithm Notes

## 1. Scope

This document explains the RenderGraph (RDG) algorithm implemented in:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:44`
- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:1501`

It focuses on dependency inference, plan build, cache validation, topological sorting, and diagnostics.

## 2. Data Model

## 2.1 Node-level input

Per node, dependency input comes from two sources:

- Explicit dependencies: `dependency_render_graph_nodes` in node desc.
  - `glTFRenderer/RendererCore/Public/Renderer.h`
- Resource usage declared in draw info (buffers/textures/render targets).
  - Access extraction starts at `glTFRenderer/RendererCore/Private/RendererInterface.cpp:101`

## 2.2 Resource key space

RDG normalizes resources into `(kind, value)`:

- `ResourceKind::Buffer`
- `ResourceKind::Texture`
- `ResourceKind::RenderTarget`

Reference:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:44`
- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:51`

## 2.3 Edge representation

Dependencies are stored as adjacency map:

- `DependencyEdgeMap = map<Node, set<Node>>` meaning `from -> to`

Reference:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:98`

## 3. Access Classification Rules

`CollectResourceAccess()` classifies each binding into `reads` / `writes`:

- Buffer
  - `CBV`, `SRV` => read
  - `UAV` => read + write
- Texture
  - `SRV` => read
  - `UAV` => read + write
- RenderTargetTexture binding
  - `SRV` => read
  - `UAV` => read + write
- RenderTarget attachment
  - always write
  - if `load_op == LOAD`, also read

Reference:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:101`

## 4. Dependency Inference

## 4.1 Readers/Writers index

`CollectResourceReadersAndWriters()` builds:

- `resource -> readers set`
- `resource -> writers set`

Reference:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:173`

## 4.2 Inferred edge construction

`BuildResourceInferredEdges()` logic per resource:

1. `consumers = writers U readers`
2. For each `writer` in `writers`
3. Add edge `writer -> consumer` for each `consumer != writer`

This enforces writer-before-use and writer-before-writer ordering on the same resource.

Reference:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:197`

## 5. Explicit Dependency Validation

`ValidateDependencyPlan()` merges explicit dependencies into inferred graph and validates each explicit edge:

A dependency is valid iff:

- handle is valid
- not self dependency
- index in bounds
- dependency node is registered this frame

Invalid explicit edges are collected and marked.

Reference:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:508`

## 6. Execution Signature and Cache Key

`ComputeExecutionSignature()` hashes:

- node handle list
- node resource read set
- node resource write set
- explicit dependency list
- merged outgoing edges

The signature plus node count is used as execution-cache key.

Reference:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:240`
- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:548`

## 7. Cached Order Validation

`ValidateExecutionOrder()` verifies cached order against current graph:

- same node count
- all nodes unique and present
- for every edge `u -> v`, `order[u] < order[v]`

Reference:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:369`

## 8. Topological Sort and Cycle Detection

`TopologicalSortExecutionNodes()`:

- computes indegree
- keeps zero-indegree nodes in ordered set
- repeatedly pops smallest handle, relaxes outgoing edges

If output count != node count, remaining indegree>0 nodes are cycle nodes.

Reference:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:286`

## 9. Plan Build Pipeline

`BuildExecutionPlan()` composes:

1. `CollectResourceAccessPlan()`
2. `ValidateDependencyPlan()`
3. `ComputeExecutionSignature()`
4. cache-key comparison
5. cached order validation
6. rebuild decision

Reference:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:537`

## 10. Apply Stage Semantics

`ApplyExecutionPlanResult()` behavior:

- If rebuild not needed: keep cache as-is.
- If invalid explicit dependencies: mark graph invalid, clear execution order, skip execution.
- Else run topological sort:
  - sorted => cache order updated, graph valid
  - cycle => graph invalid, cache cleared, cycle nodes returned

Reference:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:561`

## 11. Diagnostics Model

`UpdateDependencyDiagnostics()` publishes:

- graph validity
- invalid explicit edges
- auto-merged dependency count
- execution signature
- cached node/order size
- cycle nodes

Public diagnostics type:

- `glTFRenderer/RendererCore/Public/RendererInterface.h:186`

Update function:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:641`

## 12. Runtime Integration

RDG algorithm runs once per frame in `ExecuteRenderGraphFrame()`:

1. snapshot registered nodes
2. build plan context
3. build plan
4. apply plan / update cache
5. update diagnostics
6. execute cached order and collect timing stats

Reference:

- `glTFRenderer/RendererCore/Private/RendererInterface.cpp:1501`

## 13. Complexity (High-level)

Let:

- `N` = registered nodes in frame
- `E` = merged dependency edges
- `A` = total binding entries across all nodes

Then roughly:

- Access collection: `O(A log N)` (set/map inserts)
- Inferred edge build: `O(sum over resources of W_r * C_r)`
- Cached order validation: `O(N + E)` map lookups (ordered containers add log factors)
- Topological sort: `O((N + E) log N)` due to ordered ready-set operations

Note: current implementation uses `std::map`/`std::set`, favoring deterministic order over raw throughput.

## 14. Correctness Invariants

When `graph_valid == true`:

- cached execution order must contain all registered nodes exactly once
- for any dependency edge `u -> v`, `u` appears before `v`

When explicit dependency is invalid or cycle exists:

- `graph_valid == false`
- execution order is cleared
- pass execution is skipped for safety

## 15. Extension Checklist

If you introduce a new binding type/resource kind:

1. extend `CollectResourceAccess()` classification
2. ensure `ComputeExecutionSignature()` can observe its access changes
3. update diagnostics fields only if new failure mode is introduced
4. test three cases:
   - no explicit deps, inferred deps only
   - explicit deps with valid handles
   - invalid explicit dep / cycle

## 16. Practical Debugging Path

When frame graph behavior is wrong:

1. check diagnostics in UI (`GetDependencyDiagnostics`) for invalid edges/cycles
2. check whether cache key changed but order was not rebuilt unexpectedly
3. inspect node draw bindings and verify read/write classification in `CollectResourceAccess()`
4. inspect explicit `dependency_render_graph_nodes` source node registration timing

---

## Appendix: Minimal pseudocode

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
