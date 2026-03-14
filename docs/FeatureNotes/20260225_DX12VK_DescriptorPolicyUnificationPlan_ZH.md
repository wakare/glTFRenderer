# Feature Note - DX12 / Vulkan Descriptor 策略统一计划

配套文档：

- 英文版本：
  - `docs/FeatureNotes/20260225_DX12VK_DescriptorPolicyUnificationPlan.md`

- Date: 2026-02-25
- Work Item ID: INFRA-DESC-UNIFY
- Title: 在不强迫后端机制同构的前提下，统一 DX12 和 Vulkan 的 descriptor 管理策略
- Owner: AI coding session
- Related Plan:
  - `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. 范围

- In scope:
  - 统一 descriptor budget policy、lifecycle policy、diagnostics 和 failure behavior。
  - 保留 backend-specific 的分配机制（DX12 heap 模型、Vulkan descriptor-pool / set 模型）。
  - 在当前 frosted-glass pass 数增长的前提下，降低 DX12 descriptor exhaustion 风险。
- Out of scope:
  - 完整的 backend-isomorphic rewrite（不要求 DX12 去模仿 Vulkan descriptor set）。
  - 立刻重做全部 root-signature 或 bindless 架构。

## 2. 基线差异（当前）

- 在 memory-manager 初始化时，两个后端都收到相同的输入预算（`cbv_srv_uav=512, rtv=64, dsv=64`）：
  - `glTFRenderer/RendererCore/Private/ResourceManager.cpp`
- Vulkan 会在这组基线上再叠加更大的有效 pool 预算：
  - `glTFRenderer/RHICore/Private/RHIVKImpl/VKDescriptorManager.cpp`
  - （`base=max(cbv_srv_uav,256)`，然后继续扩展 buffer / image / maxSets 容量）
- DX12 直接用原始基线作为 heap size，并且只按线性消耗增长：
  - `glTFRenderer/RHICore/Private/RHIDX12Impl/DX12DescriptorManager.cpp`
  - `glTFRenderer/RHICore/Private/RHIDX12Impl/DX12DescriptorHeap.cpp`
- DX12 descriptor heap 路径当前在写入下一个 descriptor slot 前，没有显式的 hard-cap precheck。
- Vulkan 路径有更强的 pool-side limit，并且在 root signature 中有显式的 set allocation / free 生命周期：
  - `glTFRenderer/RHICore/Private/RHIVKImpl/VKRootSignature.cpp`

## 3. 风险评估

## 3.1 如果直接把 “Vulkan 机制复制到 DX12”

- Risk level: Medium-High
- Main reasons:
  - DX12 路径在多个调用点中依赖连续的 GPU descriptor handle 区间来做 descriptor-table 绑定。
  - 如果在不保留“连续表区域”前提的情况下，硬改成 pooled / freelist 分配，可能会破坏 shader table binding。
  - 当前 DX12 heap 中的 descriptor cache 是按资源指针做 key，尚不具备健壮的压缩 / 重定位能力。

## 3.2 如果只在策略层统一

- Risk level: Low-Medium
- Main reasons:
  - 后端实现仍保持对各自 API 模型自然。
  - 统一点集中在可预测的控制面：budget、guardrail、telemetry、fallback。
  - 可以分阶段 rollout，并通过现有 render graph regression suite 验证。

## 4. 目标架构（推荐）

- 在 renderer-core 中增加统一的策略合同（backend-agnostic）：
  - `DescriptorBudgetPolicy`：
    - baseline capacities（cbv / srv / uav、rtv、dsv）
    - backend multiplier / floor 规则
    - warning threshold 和 hard-limit 行为
  - `DescriptorLifecyclePolicy`：
    - release latency
    - stale-resource pruning cadence
    - per-frame budget pressure sampling
  - `DescriptorDiagnosticsPolicy`：
    - 周期性 usage log cadence
    - OOM / exhaustion event format
    - per-frame high-watermark reporting
- Backend adapter：
  - DX12 adapter 把策略映射到 heap sizing + allocation guardrail。
  - Vulkan adapter 把策略映射到 pool size + set count。

## 5. 分阶段执行计划

## P0 - 即刻加固（快、低风险）

- DX12：
  - 在 descriptor 分配前增加 hard-cap 检查。
  - 以显式错误码 / 日志失败（descriptor type + used / capacity + 如果能拿到则带 pass 名）。
  - 引入类似 Vulkan margin policy 的保守 capacity floor / multiplier。
- Vulkan：
  - 保持当前放大的 pool 逻辑。
  - 在日志 / debug UI 中暴露 high-watermark 和 remaining-budget 计数器。

预期结果：

- 不再存在 silent overflow 路径。
- 在 exhaustion 行为和 diagnostics 上实现更好的后端一致性。

## P1 - 统一策略层（中等工作量）

- 增加由两个 descriptor manager 共同消费的共享 config object。
- 把后端各自的常量（floor / multiplier）移动到显式 policy knob 下。
- 统一 telemetry 字段：
  - used、capacity、peak、allocation_fail_count、fallback_count。

预期结果：

- DX12 / Vulkan 使用同一套调参接口。
- 更容易做跨后端回归跟踪。

## P2 - DX12 分配器策略精修（更高风险，可选）

- 在 P0 / P1 稳定后可选推进：
  - 按 usage class 做 segmented heap
  - 显式区分 transient / persistent descriptor lane
  - 在 safe point 上做受控 rebuild / rebase
- 但必须保持连续 descriptor-table 的保证不被破坏。

预期结果：

- 改善长时间运行稳定性，并降低 descriptor 压力。

## 6. 验证与证据

- Functional checks:
  - 在 frosted strict multilayer 路径 + raster payload 路径下，不出现 descriptor allocation assert / crash。
  - DX12 / Vulkan 都不回归 render graph 的 pass 执行。
- Performance checks:
  - Descriptor-management 开销不会引入可测量的 frame spike。
- Stress checks:
  - 重复执行 resize + mode switching（`single/auto/multilayer`，payload compute / raster）至少 5 分钟。
- Required evidence:
  - 每个 backend 的 descriptor usage 快照（start、peak、steady state）
  - 带频率限制的 error / fallback log（如有）

## 7. 验收标准

- Policy parity:
  - DX12 和 Vulkan 都遵守同一套高层 budget / lifecycle / diagnostic 合同。
- Stability:
  - 在目标 frosted 场景中，没有已知的 descriptor exhaustion crash。
- Observability:
  - Diagnostics 能清楚上报压力和失败，同时避免日志洪泛。

## 8. 推荐决策

- 不要直接把 Vulkan 机制克隆到 DX12。
- 优先做策略层统一（P0 -> P1）。
- 只有在 P1 之后的 telemetry 仍显示压力明显时，再决定是否做更深的 DX12 allocator 重构。

## 9. 下一步任务

1. 实现 P0 的 DX12 hard-cap guard + 显式 failure log。
2. 为两个 backend 都增加 descriptor usage telemetry counter。
3. 引入共享的 `DescriptorBudgetPolicy` 配置路径。
4. 执行跨后端 stress validation，并在 follow-up note 中记录证据。
