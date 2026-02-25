# Feature Note - DX12/Vulkan Descriptor Policy Unification Plan

- Date: 2026-02-25
- Work Item ID: INFRA-DESC-UNIFY
- Title: Unify descriptor management policy across DX12 and Vulkan without forcing identical backend mechanisms
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Scope

- In scope:
  - Unify descriptor budget policy, lifecycle policy, diagnostics, and failure behavior.
  - Keep backend-specific allocation mechanisms (DX12 heap model, Vulkan descriptor-pool/set model).
  - Reduce DX12 descriptor exhaustion risk under current frosted-glass pass growth.
- Out of scope:
  - Full backend-isomorphic rewrite (do not force DX12 to mimic Vulkan descriptor sets).
  - Immediate redesign of all root-signature or bindless architecture.

## 2. Baseline Differences (Current)

- Same input budget is passed at memory-manager init (`cbv_srv_uav=512, rtv=64, dsv=64`):
  - `RendererCore/Private/ResourceManager.cpp`
- Vulkan applies a larger effective pool budget on top of this baseline:
  - `RHICore/Private/RHIVKImpl/VKDescriptorManager.cpp`
  - (`base=max(cbv_srv_uav,256)`, then expanded buffer/image/maxSets capacities)
- DX12 uses the raw baseline directly for heap size and grows only by linear consumption:
  - `RHICore/Private/RHIDX12Impl/DX12DescriptorManager.cpp`
  - `RHICore/Private/RHIDX12Impl/DX12DescriptorHeap.cpp`
- DX12 descriptor heap path currently has no explicit hard-cap precheck before writing next descriptor slot.
- Vulkan path has stronger pool-side limits and explicit set allocation/free lifecycle in root signature:
  - `RHICore/Private/RHIVKImpl/VKRootSignature.cpp`

## 3. Risk Assessment

## 3.1 If we directly “copy Vulkan mechanism into DX12”

- Risk level: Medium-High
- Main reasons:
  - DX12 path assumes contiguous GPU descriptor handle regions for descriptor-table usage in multiple call paths.
  - Retrofitting pooled/freelist allocation without preserving contiguous-table assumptions can break shader table bindings.
  - Existing descriptor caching in DX12 heap is resource-pointer keyed and not currently structured for robust compaction/relocation.

## 3.2 If we unify at policy layer only

- Risk level: Low-Medium
- Main reasons:
  - Backend implementations remain natural to API model.
  - Unification happens in predictable control points (budget, guardrails, telemetry, fallback).
  - Rollout can be phased and validated with current render graph regression suite.

## 4. Target Architecture (Recommended)

- Unified policy contract in renderer-core (backend-agnostic):
  - `DescriptorBudgetPolicy`:
    - baseline capacities (cbv/srv/uav, rtv, dsv)
    - backend multiplier/floor rules
    - warning thresholds and hard-limit behavior
  - `DescriptorLifecyclePolicy`:
    - release latency
    - stale-resource pruning cadence
    - per-frame budget pressure sampling
  - `DescriptorDiagnosticsPolicy`:
    - periodic usage log cadence
    - OOM/exhaustion event format
    - per-frame high-watermark reporting
- Backend adapters:
  - DX12 adapter maps policy into heap sizing + allocation guardrails.
  - Vulkan adapter maps policy into pool sizes + set counts.

## 5. Phased Execution Plan

## P0 - Immediate hardening (fast, low risk)

- DX12:
  - Add hard-cap check before descriptor allocation in heap.
  - Fail with explicit error code/log (descriptor type + used/capacity + pass name if available).
  - Apply conservative capacity floor/multiplier similar to Vulkan margin policy.
- Vulkan:
  - Keep current enlarged pool logic.
  - Expose high-watermark and remaining-budget counters in logs/debug UI.

Expected outcome:
- No silent overflow path.
- Better parity in exhaustion behavior and diagnostics.

## P1 - Unified policy layer (moderate)

- Add shared config object consumed by both descriptor managers.
- Move backend-specific constants (floors/multipliers) under explicit policy knobs.
- Standardize telemetry fields:
  - used, capacity, peak, allocation_fail_count, fallback_count.

Expected outcome:
- Same tuning interface for DX12/Vulkan.
- Easier cross-backend regression tracking.

## P2 - DX12 allocator strategy refinement (higher risk, optional)

- Optional improvements after P0/P1 stability:
  - segmented heaps by usage class
  - explicit transient vs persistent descriptor lanes
  - controlled rebuild/rebase path during safe points
- Must preserve contiguous descriptor-table guarantees.

Expected outcome:
- Better long-session stability and less descriptor pressure.

## 6. Validation and Evidence

- Functional checks:
  - no descriptor allocation asserts/crashes under frosted strict multilayer path + raster payload path.
  - no regression in render graph pass execution for DX12/Vulkan.
- Performance checks:
  - descriptor-management overhead does not introduce measurable frame spikes.
- Stress checks:
  - repeated resize + mode switching (`single/auto/multilayer`, payload compute/raster) for at least 5 minutes.
- Required evidence:
  - per-backend descriptor usage snapshots (start, peak, steady state)
  - error/fallback logs (if any) with capped-frequency reporting

## 7. Acceptance Criteria

- Policy parity:
  - DX12 and Vulkan both honor the same high-level budget/lifecycle/diagnostic contract.
- Stability:
  - no known descriptor exhaustion crash in target frosted scenes.
- Observability:
  - diagnostics clearly report pressure and failures without log flooding.

## 8. Recommended Decision

- Do not perform direct Vulkan-mechanism cloning on DX12.
- Proceed with policy-level unification first (P0 -> P1).
- Re-evaluate need for deeper DX12 allocator refactor only after telemetry confirms remaining pressure after P1.

## 9. Next Tasks

1. Implement P0 DX12 hard-cap guard + explicit failure logs.
2. Add descriptor usage telemetry counters to both backends.
3. Introduce shared `DescriptorBudgetPolicy` configuration path.
4. Run cross-backend stress validation and record evidence in a follow-up note.
