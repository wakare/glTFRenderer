# Feature Note - B7 Panel GBuffer Two-Layer Compatibility Plan

- Date: 2026-02-25
- Work Item ID: B7
- Title: 3D perspective panel compatibility plan with Panel GBuffer and two-layer sequential frosted composition
- Owner: AI coding session
- Related Plan:
  - `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
  - `Doc/RendererDemo_FrostedGlass_FramePassReference.md` (frame pass role/dependency/resource contract)

## 1. Requirement Update

- New priority requirement:
  - Future frosted-glass work should prioritize compatibility with Apple Vision Pro-style 3D perspective panels.
  - Keep default layering target at two layers (`front`, `back`) with sequential composition.
  - Maintain compatibility with existing 2D UI rendering pipeline.

## 2. Baseline Risk

- Current frosted panel logic is primarily screen-space SDF and layer-order driven.
- Main risk:
  - perspective-transformed panel edges and optics do not always match real 3D geometry behavior
  - overlap ordering can diverge from true depth ordering
  - temporal stability pressure increases under aggressive camera/panel motion

## 3. Proposed Architecture

1. Panel GBuffer prepass (raster)
- Render frosted panel backgrounds to dedicated panel payload targets.
- Support both:
  - 3D panel geometry path
  - 2D frosted-background prepass path
- Keep top-2 payload per pixel: front layer and back layer.

2. Blur pyramid path (compute, existing)
- Reuse current downsample/blur pyramid pipeline.
- Keep quality tiers and runtime guardrails.

3. Sequential composite path (compute)
- Pass A: composite back layer over scene.
- Pass B: composite front layer using Pass A result as input.
- Keep single-layer fast path for pixels with only one valid panel layer.

4. UI content pass (unchanged)
- Text/icons/buttons and non-frosted widgets remain in existing UI path.
- Only frosted panel background writes Panel GBuffer payload.

## 4. Panel GBuffer Payload Scope (Minimum)

- Per front/back layer:
  - `coverage` / `mask`
  - `panel_id` (or compact index)
  - `depth` (for deterministic layer ordering)
  - `edge/profile normal` (or equivalent edge-direction payload for perspective edge lighting)
  - optics/material controls:
    - blur sigma / blur strength
    - thickness / refraction strength
    - fresnel intensity / fresnel power
    - rim intensity / tint controls

Notes:
- Exact packing can be tuned later; this list defines required semantics for B7 acceptance.

## 4.1 Thickness-Driven Edge Highlight Rule (Required)

- Goal:
  - Make edge highlight and thickness perception correlated, not only slider-driven.
- Required behavior:
  - increasing panel thickness must increase edge highlight visibility and edge shadowing tendency
  - center region should remain controlled and avoid full-frame over-brightening
  - refraction amplitude should scale with thickness in a bounded way
- Baseline composite terms (implementation-level guideline):
  - `edge_term = pow(saturate(1 - abs(dot(N, V))), edge_power)`
  - `thickness_factor = saturate((thickness - t_min) / max(t_max - t_min, eps))`
  - `edge_highlight = base_highlight * lerp(1.0, highlight_boost_max, thickness_factor) * edge_term`
  - `edge_shadow = base_shadow * thickness_factor * edge_term`
  - `refraction_scale = base_refraction * lerp(1.0, refraction_boost_max, thickness_factor)`
- Notes:
  - constants can be tuned during B7.5, but coupling behavior is mandatory for acceptance.

## 4.2 Dominant Directional-Light Highlight Rule (Required)

- Goal:
  - Make panel edge highlight react to scene lighting direction when directional light exists.
- Required behavior:
  - each frame, frosted global parameters should pick the brightest directional light direction from lighting system
  - edge highlight should be modulated by this direction in composite stage
  - when no directional light exists, preserve existing view-driven highlight behavior (fallback, no visual hard break)
- Selection rule:
  - directional light ranking metric: luminance of light intensity (`0.2126 * R + 0.7152 * G + 0.0722 * B`)
  - choose max-luminance directional light as highlight direction source.

## 4.3 AVP-Style Edge Highlight Enhancement Rule (Required)

- Goal:
  - Increase bright edge readability to approach Apple Vision Pro glass appearance.
- Required behavior:
  - keep existing rim/fresnel contribution
  - add an independent edge-specular layer (not only multiplicative modulation)
  - edge-specular coverage should be driven by profile/rim/fresnel edge basis with controllable width
  - keep fallback behavior stable when directional light is unavailable
- Phase-1 controls (global, advanced UI):
  - `edge_spec_intensity`
  - `edge_spec_sharpness`
  - `edge_highlight_width`
  - `edge_highlight_white_mix`

## 5. 2D and 3D Compatibility Policy

- 3D frosted panel:
  - writes Panel GBuffer with true world depth.
- 2D frosted panel background:
  - writes Panel GBuffer through dedicated UI prepass.
  - supports two modes:
    - overlay mode: fixed composite order/depth policy
    - world-space mode: depth-aware policy
- 2D non-frosted UI content:
  - does not write Panel GBuffer.
  - remains on current UI draw path after frosted composite.

## 6. Execution Plan (Phased)

1. B7.1 Panel GBuffer resource and payload schema
- Add render targets and binding layout.
- Define front/back payload packing and clear policy.

2. B7.2 3D panel raster prepass
- Render frosted panel geometry into Panel GBuffer.
- Validate per-pixel top-2 layer write policy.

3. B7.3 Two-layer sequential composite integration
- Wire back-then-front composite flow in render graph.
- Preserve single-layer fast path.

4. B7.4 2D frosted background compatibility prepass
- Route 2D frosted backgrounds to Panel GBuffer.
- Keep non-frosted UI unchanged.

5. B7.5 Thickness-edge coupling implementation
- Implement and tune thickness-driven edge highlight/shadow behavior in composite.
- Add debug controls for `edge_power`, `highlight_boost_max`, `refraction_boost_max`.

6. B7.6 Stabilization and acceptance
- Validate visual stability and pass timing.
- Tune defaults and update evidence in feature notes.
- Integrate dominant directional-light driven highlight modulation with no-light fallback.

7. B7.7 AVP-style edge highlight enhancement
- Add profile-aware independent edge-specular layer in composite.
- Expose global controls for spec intensity, sharpness, edge width and white tint mix.
- Tune defaults toward stronger edge readability while keeping stable fallback.

## 7. Acceptance Targets

- Functional:
  - Panel GBuffer top-2 payload is produced and consumed correctly.
  - Back->front sequential composite path is deterministic.
- Visual:
  - Perspective-transformed panels show stable edge/refraction behavior.
  - Overlap regions show expected two-layer stacking.
  - 2D non-frosted UI content is unaffected.
  - With fixed camera and background, increasing thickness produces stronger/wider edge highlights and slightly stronger edge darkening, without center overexposure.
- Performance:
  - Non-overlap dominant scenes: bounded overhead vs current B6 baseline.
  - Overlap-heavy scenes: overhead remains within agreed budget and fallback remains available.

## 8. Initial Risks and Mitigation

- Risk: bandwidth pressure from extra panel payload targets.
  - Mitigation: compact payload packing and optional half-resolution paths where acceptable.
- Risk: ordering ambiguity between overlay 2D and world-space 3D panels.
  - Mitigation: explicit per-panel mode and deterministic order policy.
- Risk: regression in UI readability or draw order.
  - Mitigation: preserve existing non-frosted UI pass and add validation checklist per scene type.

## 9. Next Action

- Continue B7 with shared producer integration:
  - hook external 3D panel producer into `world_space_mode` payload path
  - add 2D frosted background prepass producer for shared payload path
- Keep B6 outputs as baseline reference for regression and performance comparison.

## 10. Progress Snapshot (2026-02-25)

- B7.1 completed (schema + wiring):
  - Added front/back panel profile payload render targets (`PanelProfile`, `PanelProfileSecondary`).
  - Mask/payload pass now outputs profile normal direction and optical thickness per front/back layer.
  - Composite passes consume profile payload, enabling subsequent thickness-edge coupling work without another interface migration.
- B7.2 runtime + raster payload path completed:
  - Added runtime payload-path option (`Compute (SDF)` / `Raster (Panel GBuffer)`).
  - Added instanced raster front/back passes:
    - front pass writes front payload with depth-based layer resolve
    - back pass re-rasterizes with front-panel rejection and resolves second layer
  - Payload generation in raster mode is now panel-geometry coverage driven (per-panel quad raster), not fullscreen panel-loop traversal.
  - `Tick` now registers compute or raster payload path by runtime selection, with compute fallback status shown in debug UI.
- B7.3a world-space raster producer integration completed:
  - Added per-panel world-space payload fields (`world_center`, `world_axis_u`, `world_axis_v`, `world_space_mode`) in panel descriptor/GPU payload.
  - Raster payload VS now supports world-space quad projection for true geometry-driven depth/coverage.
  - Raster payload PS now evaluates world-space panel payload using panel-local UV profile path while keeping existing screen-space path unchanged.
  - Debug UI now exposes world-space panel toggle and transform vectors for live validation.
- B7.3b depth policy split for 2D/3D compatibility completed:
  - Added explicit per-panel depth policy (`Overlay` / `Scene Occlusion`) and packed it into panel layering payload.
  - World-space payload raster now applies scene-depth occlusion only when depth policy is set to `Scene Occlusion`.
  - This enables VisionOS-style world-space panel occlusion while keeping 2D overlay backgrounds on deterministic overlay behavior.
- B7.5 thickness-edge coupling implementation completed:
  - Added global tuning controls for thickness coupling (`edge power`, `highlight boost max`, `refraction boost max`, `edge shadow strength`, `thickness range min/max`).
  - Refraction amplitude now scales with normalized thickness in both compute and raster payload evaluators.
  - Composite highlight/shadow now applies thickness-weighted edge response with bounded center suppression.
- B7.6 dominant directional-light highlight modulation completed:
  - Added lighting-system query for brightest directional light direction.
  - Frosted global buffer now updates per frame with highlight directional-light vector and validity flag.
  - Composite highlight term now supports directional-light modulation and falls back to previous view-driven behavior when no directional light exists.
- B7.7 AVP-style edge highlight enhancement phase-1 completed:
  - Added independent edge-specular term in composite on top of existing rim/fresnel terms.
  - Edge coverage now uses profile/rim/fresnel basis with controllable width.
  - Added advanced global controls (`Edge Spec Intensity`, `Edge Spec Sharpness`, `Edge Highlight Width`, `Edge Highlight White Mix`).
- Pending for next phase:
  - hook 3D panel prepass and 2D frosted background prepass producers into this shared payload path
  - evaluate dual-lobe spec and bloom/tone-map coupling for closer AVP highlight response
  - capture final B7 visual/perf acceptance evidence in representative 2D+3D overlap scenes
