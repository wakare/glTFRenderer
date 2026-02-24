# Feature Note - B6 Multi-layer Composition Plan

- Date: 2026-02-24
- Work Item ID: B6
- Title: Requirement update and implementation plan for Vision Pro-style multilayer frosted composition
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`

## 1. Requirement Update

- New requirement:
  - Frosted-glass overlap behavior should align as closely as feasible to Apple Vision Pro-style multilayer perception.
  - Overlap regions should support multi-layer mixing instead of single-winner-only behavior.
  - Performance overhead must remain controlled with explicit quality tiers and fallback strategy.

## 2. Current Baseline Gap

- Current overlap pipeline selects one panel per pixel (single winner), then performs one composite.
- This is deterministic and fast, but overlap regions cannot express stacked transmissive behavior.
- Gap to target:
  - insufficient layer depth response
  - limited overlap richness
  - difficult to match expected multilayer glass feel

## 3. Candidate Refactor Options

## Option A (Recommended): Top-2 Layered Composition + Fast Path

- Summary:
  - In mask pass, keep top-2 panels per pixel (front + back) using layer priority + mask score.
  - In composite, compute front and back frosted contributions and combine with transmittance-style weighting.
  - For pixels with only one valid layer, remain on single-layer path.
- Pros:
  - Large quality gain in overlap regions.
  - Cost increase bounded and predictable.
  - Minimal structural change from current architecture.
- Cons:
  - Additional payload bandwidth and extra shading in overlap pixels.

## Option B: Weighted Blended Multi-layer (Approximate OIT-style)

- Summary:
  - Accumulate multiple panel contributions with weighted formula in one pass.
- Pros:
  - Smooth aggregate behavior for many overlaps.
- Cons:
  - Harder to art-direct and debug.
  - Ordering semantics less explicit.
  - Can introduce color-energy instability without careful normalization.

## Option C: Full N-layer Ordered Composition

- Summary:
  - Sort/compose N layers per pixel with explicit ordered blend.
- Pros:
  - Highest theoretical control and fidelity.
- Cons:
  - Highest cost and complexity.
  - Not suitable as default for current performance target.

## 4. Recommended Execution Plan

1. Phase B6.1 - Data model and payload extension
- Extend mask payload from single panel index to top-2 panel payload.
- Keep deterministic selection policy:
  - higher `layer_order`
  - then higher mask
  - then stable index tie-breaker

2. Phase B6.2 - Layered composite implementation
- Add front/back two-layer composition model in `CompositeMain`.
- Reuse existing blur pyramid and optics derivation.
- Preserve single-layer branch for non-overlap pixels.

3. Phase B6.3 - Performance guardrails
- Add quality mode:
  - `single` (force current behavior)
  - `auto` (default, overlap-aware)
  - `multi_layer` (force top-2 everywhere)
- Add overlap-aware fallback:
  - if overlap score low, execute single-layer path.
- Add debug counters/timing labels for overlap pixel ratio and path usage.

4. Phase B6.4 - Stabilization and acceptance
- Validate temporal stability with layered result.
- Tune weights to avoid halo pumping on fast motion.
- Capture visual + timing acceptance evidence.

## 5. Proposed Acceptance and Budget Baseline

- Functional:
  - Overlap regions show deterministic two-layer behavior and remain stable during motion.
- Visual:
  - Layered overlap noticeably improves depth/stacking feel versus single-winner baseline.
- Performance (initial target for `auto` mode):
  - non-overlap dominant scenes: frosted pass cost increase <= 20% vs B3 baseline
  - overlap stress scenes: frosted pass cost increase <= 35% vs B3 baseline
  - system can fallback to single-layer when budget pressure is detected

## 6. Primary Files for B6

- `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
- `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoAppModelViewer.cpp`

## 7. Next Action

- Start B6.1 implementation with Option A (top-2 layered composition) as default strategy.
