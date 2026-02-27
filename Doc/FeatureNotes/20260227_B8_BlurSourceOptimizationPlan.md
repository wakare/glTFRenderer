# Feature Note - B8 Frosted Blur Source Optimization Plan

- Date: 2026-02-27
- Work Item ID: B8
- Title: Replace fixed frosted blur pyramid with shared low-cost blur source
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
  - `Doc/RendererDemo_FrostedGlass_FramePassReference.md`
  - `Doc/FeatureNotes/20260225_B7_PanelGBufferTwoLayerCompatibilityPlan.md`

## 1. Requirement Update

- New requirement:
  - Reduce runtime cost of frosted scene blur generation.
  - Keep AVP-like subjective blur appearance as first priority.
  - Preserve B7 two-layer sequential composition behavior (`back -> front`) and 2D/3D panel compatibility.
- Scope:
  - Blur-source generation architecture only.
  - Keep current panel payload and composite feature set unless required by integration.

## 2. Current Baseline Cost Snapshot

As of current B7 baseline implementation:

- Fixed blur chain levels:
  - `1/2`, `1/4`, `1/8`, `1/16`, `1/32`
- Per blur chain pass count:
  - `5 x Downsample` + `5 x Horizontal Blur` + `5 x Vertical Blur` = `15 passes`
- Strict multilayer path:
  - Builds an additional full blur chain for front-over-back path
  - Extra `15 passes`
- Blur generation total:
  - Single-path runtime: `15` blur passes
  - Strict multilayer runtime: `30` blur passes

Blur-related render-target footprint (active handles):

- Base chain:
  - Frosted-owned dedicated RTs: `11`
  - Plus shared half/quarter ping-pong from `PostFxSharedResources`: `4`
- Strict multilayer chain:
  - Additional frosted-owned RTs: `15`
- Worst-case blur-source RT handles touched by frosted path:
  - about `30` handles (including shared postfx resources)

Notes:

- This count is for source-generation stage only (not including mask/composite/history outputs).
- Existing B7 acceptance snapshot already validated visual quality on this baseline.

## 3. Optimization Goal

Target a new blur-source path with much lower pass and resource cost while keeping current visual direction:

- Keep low-frequency background blur strong enough for AVP-like glass perception.
- Keep compositing, edge highlight, refraction and multilayer policies unchanged.
- Support quality tiers so low-cost mode can be default and higher quality can be optional.

## 4. Proposed Replacement Architecture

## 4.1 Path A - Shared Mip Source (Primary)

- Generate one low-resolution global frosted blur source from scene color.
- Use mip selection (`SampleLevel`) in frosted composite shader as blur source.
- Optional two-source mode in strict multilayer:
  - Source 0 from scene color.
  - Source 1 from back-composited color (only when strict path is active).

Expected cost:

- `1~2` passes per source (`downsample` + optional explicit mip generation pass).
- `1` mipmapped low-resolution color RT per source.

## 4.2 Path B - Shared Dual Filter Source (Optional Quality Tier)

- Keep global source idea, but add low-resolution dual-filter passes.
- Use `2~4` dual passes depending on quality preset.

Expected cost:

- `3~5` passes per source.
- `2` low-resolution ping-pong RTs per source.

## 4.3 Recommended Rollout

- Default: Path A (`SharedMip`) for best cost/perf ratio.
- Optional high-quality preset: Path B (`SharedDual`).
- Keep legacy fixed pyramid as fallback during migration.

## 5. Budget Comparison (Target)

| Mode | Blur Source Strategy | Passes (Single) | Passes (Strict) | Blur RTs (Single) | Blur RTs (Strict) |
|---|---|---:|---:|---:|---:|
| Legacy | Fixed 5-level pyramid | 15 | 30 | ~15 touched | ~30 touched |
| B8 Path A | Shared mip source | 1~2 | 2~4 | 1 | 2 |
| B8 Path B | Shared dual source | 3~5 | 6~10 | 2 | 4 |

Acceptance-oriented target:

- Reduce blur-source pass count by at least `50%` in strict multilayer mode.
- Keep overlap visual behavior unchanged (same ordering and blend policy).

## 6. Migration Plan

1. B8.1 - Runtime path switch and resource abstraction
- Add blur source mode enum:
  - `LegacyPyramid`
  - `SharedMip`
  - `SharedDual`
- Add debug/runtime switch and safe fallback path.

2. B8.2 - Shared mip source implementation
- Add low-resolution source RT creation and update pass.
- Add optional explicit mip-chain generation pass (backend-dependent).

3. B8.3 - Composite shader integration
- Replace fixed multi-texture blur sampling dependency with source+mip sampling path for new mode.
- Keep legacy code path for A/B comparison.

4. B8.4 - Strict multilayer integration
- Add optional second source generated from back-composited input.
- Keep current `back -> front` sequential behavior and panel mask logic.

5. B8.5 - Validation and default selection
- Compare quality/perf against B7 baseline scene and overlap-heavy scene.
- Decide default mode (`SharedMip` expected).

## 7. Acceptance Checklist

Functional:

- Single and strict multilayer modes both render correctly under new blur source modes.
- Resize and temporal history lifecycle remain valid.

Visual:

- AVP-style subjective blur level is preserved or improved in agreed test scenes.
- Front/back overlap behavior remains deterministic and stable.

Performance:

- Pass count and per-frame frosted timing show measurable reduction versus legacy.
- No new descriptor/resource validation errors.

## 8. Progress Tracking (Source of Truth for B8)

Status legend: `Planned`, `In Progress`, `Blocked`, `Accepted`

| Sub-item | Scope | Status | Last Update | Evidence |
|---|---|---|---|---|
| B8.1 | Runtime switch + abstraction | Planned | 2026-02-27 | This document |
| B8.2 | Shared mip source generation | Planned | 2026-02-27 | This document |
| B8.3 | Composite shader source migration | Planned | 2026-02-27 | This document |
| B8.4 | Strict multilayer second-source integration | Planned | 2026-02-27 | This document |
| B8.5 | Perf/visual acceptance and default-mode decision | Planned | 2026-02-27 | This document |

Progress maintenance rule:

- Every B8 iteration must update this table in the same PR.
- Any pass/resource contract change must also update:
  - `Doc/RendererDemo_FrostedGlass_FramePassReference.md`

## 9. Next Action

- Start B8.1 implementation:
  - add blur source mode runtime switch
  - keep legacy pyramid as temporary fallback
  - prepare resource wrappers for shared source path
