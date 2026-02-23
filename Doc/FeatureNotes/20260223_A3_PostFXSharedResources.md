# Feature Note - A3.1 PostFX Shared Half/Quarter Ping-Pong Resource Foundation

- Date: 2026-02-23
- Work Item ID: A3.1
- Title: Add reusable window-relative half/quarter ping-pong render targets for postfx
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Scope

- In scope:
  - Add a reusable PostFX shared resource helper that allocates half/quarter resolution ping-pong render targets.
  - Standardize naming for frosted-related postfx resources with `PostFX_*` prefix.
  - Wire initialization into `RendererSystemFrostedGlass` and expose handles for future multi-pass stages.
- Out of scope:
  - New blur pyramid passes or temporal history consumption.
  - Replacing current frosted single-pass composite algorithm.
  - ToneMap input/output behavior changes.

## 2. Functional Logic

- Behavior before:
  - Frosted path only owned one full-resolution output target.
  - No reusable half/quarter ping-pong target set existed for postfx extension.
- Behavior after:
  - `PostFxSharedResources` creates and owns:
    - `PostFX_Shared_Half_Ping`
    - `PostFX_Shared_Half_Pong`
    - `PostFX_Shared_Quarter_Ping`
    - `PostFX_Shared_Quarter_Pong`
  - All of the above are window-relative targets (`0.5x` and `0.25x`) and therefore resize-correct by framework lifecycle.
  - `RendererSystemFrostedGlass` now exposes getters for these handles as stable wiring points for upcoming B1 multi-pass implementation.
- Key runtime flow:
  - `RendererSystemFrostedGlass::Init` initializes shared postfx resources first.
  - Frosted output target is renamed to `PostFX_Frosted_Output` and uses same postfx usage flags.
  - Existing frosted pass execution path remains unchanged.

## 3. Algorithm and Rendering Logic

- Passes/shaders touched:
  - No new shader logic in this iteration.
  - No pass topology change in this iteration.
- Core algorithm changes:
  - Infrastructure-only step; prepares resource graph inputs for future downsample/blur/composite staging.
- Parameter/model changes:
  - New helper type: `PostFxSharedResources` with resolution enum and ping-pong pair abstraction.

## 4. Files Changed

- `glTFRenderer/RendererDemo/RendererSystem/PostFxSharedResources.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`

## 5. Validation and Evidence

- Functional checks:
  - `RendererDemo` verify build succeeded with new shared resource helper integrated.
- Visual checks:
  - Not executed in this iteration (infrastructure-only change).
- Performance checks:
  - Not executed in this iteration.
- Evidence files/links:
  - MSBuild summary:
    - `build_logs/rendererdemo_20260223_195837.msbuild.log`
  - Standard outputs:
    - `build_logs/rendererdemo_20260223_195837.stdout.log`
    - `build_logs/rendererdemo_20260223_195837.stderr.log`
  - Binlog:
    - `build_logs/rendererdemo_20260223_195837.binlog`

## 6. Acceptance Result

- Status: PASS (A3.1 foundation scope)
- Acceptance notes:
  - Build status: succeeded
  - Warning count: 26
  - Error count: 0
  - A3 acceptance target is partially addressed (shared resource foundation in place; multi-pass consumption remains in B1).

## 7. Risks and Follow-up

- Known risks:
  - Shared targets are allocated and exposed but not consumed yet, so runtime visual behavior is unchanged.
  - Descriptor/binding behavior under active resize stress still needs runtime scenario validation with future consuming passes.
- Next tasks:
  - B1.1: introduce downsample + blur stages that consume `PostFxSharedResources`.
  - A4.x: integrate history lifecycle using similar shared resource conventions.
