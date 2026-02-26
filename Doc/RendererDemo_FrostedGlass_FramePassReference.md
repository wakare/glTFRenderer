# RendererDemo Frosted Glass Frame Pass Reference

- Version: v1.2
- Date: 2026-02-26
- Scope: one-frame frosted-glass render path in `RendererSystemFrostedGlass`
- Source files:
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedPanelProducer.cpp`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedPanelProducer.h`
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlassPostfx.hlsl`

## 1. Maintenance Contract (Required)

- This document is the frame-level contract for frosted-glass passes.
- Any PR that changes frosted-glass render passes must update this document in the same PR.
- Required sync cases:
  - pass add/remove/rename (`debug_name` changes included)
  - dependency change (`dependency_render_graph_nodes`)
  - resource binding change (`sampled_render_targets`, `render_targets`, `buffer_resources`)
  - runtime path switch logic change in `Tick`
  - render target creation/format/resolution policy changes for frosted resources
- Reviewer checklist:
  - pass order here matches runtime registration in `Tick`
  - resource producer/consumer mapping here matches code
  - strict multilayer and payload path switches are still accurate

## 2. Runtime Path Switches

### 2.1 Panel payload producer switch

- Runtime flags:
  - request: `m_panel_payload_path == PanelPayloadPath::RasterPanelGBuffer`
  - ready: `m_panel_payload_raster_ready`
- Registration behavior:
  - raster ready: register `Frosted Mask/Parameter Raster Front` + `Frosted Mask/Parameter Raster Back`
  - otherwise: register compute `Frosted Mask/Parameter`
- Fallback flag:
  - `m_panel_payload_compute_fallback_active = request_raster && !use_raster`

### 2.2 Composite branch switch

- Strict multilayer branch condition:
  - `m_global_params.multilayer_mode == MULTILAYER_MODE_FORCE`
  - and `m_multilayer_runtime_enabled == true`
- Branches:
  - strict branch: `Back Composite -> Multilayer Pyramid -> Front Composite`
  - fast branch: legacy single composite pass

### 2.3 Temporal history ping-pong

- Active node selection:
  - read A / write B: `... History A->B`
  - read B / write A: `... History B->A`
- End of frame toggle:
  - `m_temporal_history_read_is_a = !m_temporal_history_read_is_a`

### 2.4 Panel source aggregation (pre-upload)

- `g_frosted_panels` upload merges, in order:
  - internal editable panels (`m_panel_descs`)
  - callback producer world-space panels (`m_producer_world_space_panel_descs`)
  - callback producer overlay panels (`m_producer_overlay_panel_descs`)
  - manual external world-space panels (`m_external_world_space_panel_descs`)
  - manual external overlay panels (`m_external_overlay_panel_descs`)
- upload is clipped to `MAX_PANEL_COUNT`.
- temporal history validity follows effective uploaded `panel_count` (not only internal panel count).

## 3. Pass Graph Per Frame

### 3.1 Base blur pyramid (always registered)

| ID | Pass debug name | Type | Explicit dependency | Main input resources | Main output resources |
|---|---|---|---|---|---|
| A01 | Downsample Half | Compute | - | Lighting output | Half ping (shared postfx) |
| A02 | Blur Half Horizontal | Compute | A01 | Half ping | Half pong |
| A03 | Blur Half Vertical | Compute | A02 | Half pong | `m_half_blur_final_output` |
| A04 | Downsample Quarter | Compute | A03 | `m_half_blur_final_output` | Quarter ping (shared postfx) |
| A05 | Blur Quarter Horizontal | Compute | A04 | Quarter ping | Quarter pong |
| A06 | Blur Quarter Vertical | Compute | A05 | Quarter pong | `m_quarter_blur_final_output` |
| A07 | Downsample Eighth | Compute | A06 | `m_quarter_blur_final_output` | `m_eighth_blur_ping` |
| A08 | Blur Eighth Horizontal | Compute | A07 | `m_eighth_blur_ping` | `m_eighth_blur_pong` |
| A09 | Blur Eighth Vertical | Compute | A08 | `m_eighth_blur_pong` | `m_eighth_blur_final_output` |
| A10 | Downsample Sixteenth | Compute | A09 | `m_eighth_blur_final_output` | `m_sixteenth_blur_ping` |
| A11 | Blur Sixteenth Horizontal | Compute | A10 | `m_sixteenth_blur_ping` | `m_sixteenth_blur_pong` |
| A12 | Blur Sixteenth Vertical | Compute | A11 | `m_sixteenth_blur_pong` | `m_sixteenth_blur_final_output` |
| A13 | Downsample ThirtySecond | Compute | A12 | `m_sixteenth_blur_final_output` | `m_thirtysecond_blur_ping` |
| A14 | Blur ThirtySecond Horizontal | Compute | A13 | `m_thirtysecond_blur_ping` | `m_thirtysecond_blur_pong` |
| A15 | Blur ThirtySecond Vertical | Compute | A14 | `m_thirtysecond_blur_pong` | `m_thirtysecond_blur_final_output` |

### 3.2 Panel payload producer (one path active per frame)

| ID | Pass debug name | Type | Explicit dependency | Main input resources | Main output resources |
|---|---|---|---|---|---|
| B01 | Frosted Mask/Parameter | Compute | - | scene depth/normal, panel buffer, global buffer | `m_frosted_mask_parameter_output`, `m_frosted_mask_parameter_secondary_output`, `m_frosted_panel_optics_output`, `m_frosted_panel_optics_secondary_output`, `m_frosted_panel_profile_output`, `m_frosted_panel_profile_secondary_output` |
| B02 | Frosted Mask/Parameter Raster Front | Graphics | - | scene depth/normal, panel buffer, global buffer | `m_frosted_mask_parameter_output`, `m_frosted_panel_optics_output`, `m_frosted_panel_profile_output`, `m_frosted_panel_payload_depth` |
| B03 | Frosted Mask/Parameter Raster Back | Graphics | B02 | scene depth/normal, `FrontMaskParamTex`, panel buffer, global buffer | `m_frosted_mask_parameter_secondary_output`, `m_frosted_panel_optics_secondary_output`, `m_frosted_panel_profile_secondary_output`, `m_frosted_panel_payload_depth_secondary` |

Notes:
- Raster path draws instanced panel quads (`vertex_count = 6`, `instance_count = MAX_PANEL_COUNT`).
- Back raster pass depends on front raster pass to avoid depth/payload self-cycle.

### 3.3 Composite branch

### Strict multilayer branch (`Back -> Front`)

| ID | Pass debug name | Type | Explicit dependency | Main input resources | Main output resources |
|---|---|---|---|---|---|
| C01 | Frosted Composite Back | Compute | A15 | lighting output + base blur finals + front/back mask/optics/profile payloads | `m_frosted_back_composite_output` |
| C02 | Downsample Half Multilayer | Compute | C01 | `m_frosted_back_composite_output` | `m_half_multilayer_ping` |
| C03 | Blur Half Multilayer Horizontal | Compute | C02 | `m_half_multilayer_ping` | `m_half_multilayer_pong` |
| C04 | Blur Half Multilayer Vertical | Compute | C03 | `m_half_multilayer_pong` | `m_half_multilayer_blur_final_output` |
| C05 | Downsample Quarter Multilayer | Compute | C04 | `m_half_multilayer_blur_final_output` | `m_quarter_multilayer_ping` |
| C06 | Blur Quarter Multilayer Horizontal | Compute | C05 | `m_quarter_multilayer_ping` | `m_quarter_multilayer_pong` |
| C07 | Blur Quarter Multilayer Vertical | Compute | C06 | `m_quarter_multilayer_pong` | `m_quarter_multilayer_blur_final_output` |
| C08 | Downsample Eighth Multilayer | Compute | C07 | `m_quarter_multilayer_blur_final_output` | `m_eighth_multilayer_ping` |
| C09 | Blur Eighth Multilayer Horizontal | Compute | C08 | `m_eighth_multilayer_ping` | `m_eighth_multilayer_pong` |
| C10 | Blur Eighth Multilayer Vertical | Compute | C09 | `m_eighth_multilayer_pong` | `m_eighth_multilayer_blur_final_output` |
| C11 | Downsample Sixteenth Multilayer | Compute | C10 | `m_eighth_multilayer_blur_final_output` | `m_sixteenth_multilayer_ping` |
| C12 | Blur Sixteenth Multilayer Horizontal | Compute | C11 | `m_sixteenth_multilayer_ping` | `m_sixteenth_multilayer_pong` |
| C13 | Blur Sixteenth Multilayer Vertical | Compute | C12 | `m_sixteenth_multilayer_pong` | `m_sixteenth_multilayer_blur_final_output` |
| C14 | Downsample ThirtySecond Multilayer | Compute | C13 | `m_sixteenth_multilayer_blur_final_output` | `m_thirtysecond_multilayer_ping` |
| C15 | Blur ThirtySecond Multilayer Horizontal | Compute | C14 | `m_thirtysecond_multilayer_ping` | `m_thirtysecond_multilayer_pong` |
| C16 | Blur ThirtySecond Multilayer Vertical | Compute | C15 | `m_thirtysecond_multilayer_pong` | `m_thirtysecond_multilayer_blur_final_output` |
| C17 | Frosted Composite Front History A->B / B->A (active one per frame) | Compute | C16 | `BackCompositeTex` + multilayer blur finals + front/back mask/optics/profile payloads + velocity + history read | `m_frosted_pass_output`, history write (`m_temporal_history_a` or `m_temporal_history_b`) |

### Fast legacy branch

| ID | Pass debug name | Type | Explicit dependency | Main input resources | Main output resources |
|---|---|---|---|---|---|
| L01 | Frosted Composite History A->B / B->A (active one per frame) | Compute | A15 | lighting output + base blur finals + front/back mask/optics/profile payloads + velocity + history read | `m_frosted_pass_output`, history write (`m_temporal_history_a` or `m_temporal_history_b`) |

### 3.4 External consumer after frosted

- `RendererSystemToneMap` pass `Tone Map Composite` reads `m_frosted_pass_output` and writes `ToneMap_Output`.
- So frosted final output is a direct upstream dependency of tone mapping.

### 3.5 Pass Compute Logic (Input -> Output)

This section focuses on per-pass compute semantics (not graph topology).

#### 3.5.1 Blur pyramid passes (`A01`~`A15`, `C02`~`C16`)

- `DownsampleMain`:
  - output pixel maps to `2x2` source block (`base_pixel = output_xy * 2`).
  - output = arithmetic mean of 4 clamped source samples.
- `BlurHorizontalMain` / `BlurVerticalMain`:
  - 1D Gaussian convolution along X or Y.
  - radius = `max(blur_radius, 1)`.
  - sigma = `max(radius * 0.5 * blur_kernel_sigma_scale, 0.8)`.
  - normalized weighted sum over offsets `[-radius, +radius]`.

#### 3.5.2 Payload generation - compute path (`B01`)

- Per pixel:
  - compute scene edge factor from depth gradient:
    - `scene_edge = saturate((|dDepth/dx| + |dDepth/dy|) * scene_edge_scale)`.
  - compute view-dependent term:
    - reconstruct world position from depth, build view dir, `n_dot_v = saturate(abs(dot(N, V)))`.
- For each panel candidate:
  - shape SDF (`RoundedRect` / `Circle` / `ShapeMask`) -> mask and rim.
  - refraction direction from SDF normal + scene normal.
  - refraction UV:
    - `uv + dir * thickness * refraction_strength * (0.2 + mixed_fresnel)`.
  - depth-aware blur attenuation:
    - `depth_aware_weight = exp(-abs(refracted_depth - center_depth) * depth_weight_scale)`.
    - `effective_blur_strength = blur_strength * lerp(depth_aware_min_strength, 1, depth_aware_weight)`.
  - rank candidate by:
    - `layer_order` (higher first), then `mask`, then `panel_index`.
- Pack top-2 layers:
  - `mask_param = (mask, rim, mixed_fresnel, panel_index)`
  - `panel_optics = (refraction_uv.x, refraction_uv.y, effective_blur_strength, scene_edge)`
  - `panel_profile = (profile_normal_uv.x, profile_normal_uv.y, rim, optical_thickness)`

#### 3.5.3 Payload generation - raster path (`B02`, `B03`)

- `PanelPayloadVS`:
  - expands each panel to an instanced quad.
  - panel mode switch:
    - screen-space panel: clip XY from `center_uv/half_size_uv`, depth from `EncodePanelLayerDepth`.
    - world-space panel: clip position from projected world quad (`world_center + corner.x * world_axis_u + corner.y * world_axis_v`).
  - emits `local_uv` for raster payload evaluation (shape/profile in panel-local space).
- `PanelPayloadFrontPS`:
  - evaluates one panel payload at rasterized pixel.
  - evaluator switch:
    - screen-space panel: reuses existing screen-space SDF payload evaluator.
    - world-space panel: uses local-UV SDF/profile evaluator and screen-UV refraction/depth-aware optics.
  - depth policy:
    - `Overlay`: no scene-depth occlusion reject (intended for 2D overlay backgrounds).
    - `Scene Occlusion`: compare panel depth against scene depth and discard occluded pixels.
  - writes front payload MRTs; discard when mask is invalid.
- `PanelPayloadBackPS`:
  - same evaluator, but rejects panel equal to front panel at that pixel (`FrontMaskParamTex`) to produce second layer.
  - applies the same depth-policy gate as front pass.
  - writes back payload MRTs.

#### 3.5.4 Frosted color evaluator (used by `C01`, `C17`, `L01`)

- Entry: `EvaluatePanelFrostedColor(scene_color, center_normal, view_dir, mask_param, panel_optics, panel_profile, ...)`.
- Steps:
  - validate `mask` and `panel_index`.
  - sample blur pyramid at refracted UV (`1/2`, `1/4`, `1/8`, `1/16`, `1/32`) with linear (bilinear) reconstruction to avoid point-sampling shimmer/leak.
  - apply highlight de-speckle suppression:
    - detect scene luminance outliers above blur baseline (`scene_luminance - blur_luminance`)
    - reduce only this high-frequency bright residual before final blur blend.
  - build sigma-driven blend factors (`quarter/eighth/sixteenth/thirtysecond`).
  - blend toward lower-frequency result as sigma increases (coarse/veil path), then apply contrast compression and tint neutralization controls.
  - combine with original scene by `effective_blur_strength`.
  - thickness coupling:
    - normalize thickness by global range (`thickness_range_min/max`).
    - boost refraction amplitude by `thickness_refraction_boost_max`.
    - compute edge response (`edge_term`) by thickness edge power.
    - apply bounded edge shadow (`thickness_edge_shadow_strength`) and highlight boost (`thickness_highlight_boost_max`).
  - directional-light coupling:
    - per pixel, composite pass resolves `center_normal` and `view_dir`.
    - evaluator builds panel highlight normal from panel profile (and panel world axes for world-space panels).
    - profile-direction bending is applied only near panel edges to avoid interior medial-axis seams.
    - evaluator reads `highlight_light_dir_weight` from `FrostedGlassGlobalBuffer`.
    - when valid (`w > 0`), edge highlight is modulated by dominant directional-light orientation.
    - when invalid (`w == 0`), modulation becomes neutral and behavior falls back to view-driven baseline.
    - directional modulation range/contrast is tunable by:
      - `directional_highlight_min`
      - `directional_highlight_max`
      - `directional_highlight_curve`
  - independent edge-specular layer:
    - strict edge gate from `max(rim, profile_edge)`; non-edge pixels force this term to zero.
    - edge coverage controlled by `edge_highlight_width`.
    - spec lobe sharpness controlled by `edge_spec_sharpness`.
    - spec contribution scaled by `edge_spec_intensity`.
    - spec is composed as core + halo (wide lobe) to avoid overly thin edge highlights.
    - tint toward white controlled by `edge_highlight_white_mix`.
- Outputs:
  - `layer_mask`, `out_frosted_color`, `out_blur_metric` (sigma-strength metric used by multilayer logic).

#### 3.5.5 Back composite (`C01`: `CompositeBackMain`)

- Inputs: scene color + back payload (and front payload for multilayer gating).
- Compute back frosted color via `EvaluatePanelFrostedColor`.
- If multilayer enabled:
  - `effective_back_mask = ComputeEffectiveBackMask(back_mask, front_mask)`.
  - blend scene/back frosted with effective back mask.
- Output: `BackCompositeOutput`.

#### 3.5.6 Front composite strict path (`C17`: `CompositeFrontMain`)

- Input scene is `BackCompositeTex` (already includes back layer when enabled).
- Apply front layer frosted evaluation and blend.
- Temporal stabilization:
  - reproject by motion vector: `prev_uv = current_uv - velocity`.
  - history weight:
    - `temporal_history_blend`
    - attenuated by velocity reject and edge reject
    - multiplied by current panel mask.
- Output:
  - `Output` (final frosted color)
  - `HistoryOutputTex` (next-frame history).

#### 3.5.7 Legacy composite path (`L01`: `CompositeMain`)

- Starts from original `InputColorTex`, evaluates front and back frosted colors.
- If multilayer enabled:
  - composite back first, then re-evaluate front over back-composited color.
  - includes guard to avoid front appearing sharper than back:
    - uses front/back blur metric ratio and `multilayer_front_transmittance` to scale front mix.
- Uses same temporal stabilization logic as strict path.

## 4. Resource Contract

### 4.1 External inputs

- Lighting: `m_lighting->GetLightingOutput()`
- Scene GBuffer:
  - depth: `m_base_pass_depth`
  - normal: `m_base_pass_normal`
  - velocity: `m_base_pass_velocity`

### 4.2 Frosted-owned render targets (full resolution unless noted)

- Final and history:
  - `m_frosted_pass_output` (`PostFX_Frosted_Output`)
  - `m_temporal_history_a`, `m_temporal_history_b`
- Layer payload:
  - `m_frosted_mask_parameter_output`, `m_frosted_mask_parameter_secondary_output`
  - `m_frosted_panel_optics_output`, `m_frosted_panel_optics_secondary_output`
  - `m_frosted_panel_profile_output`, `m_frosted_panel_profile_secondary_output`
  - `m_frosted_panel_payload_depth`, `m_frosted_panel_payload_depth_secondary` (raster payload path)
- Strict branch intermediate:
  - `m_frosted_back_composite_output`
- Base blur pyramid:
  - final outputs: `m_half_blur_final_output`, `m_quarter_blur_final_output`, `m_eighth_blur_final_output`, `m_sixteenth_blur_final_output`, `m_thirtysecond_blur_final_output`
  - ping/pong: `m_eighth_blur_ping/pong`, `m_sixteenth_blur_ping/pong`, `m_thirtysecond_blur_ping/pong`
  - shared half/quarter ping/pong from `PostFxSharedResources`
- Multilayer blur pyramid:
  - final outputs: `m_half_multilayer_blur_final_output`, `m_quarter_multilayer_blur_final_output`, `m_eighth_multilayer_blur_final_output`, `m_sixteenth_multilayer_blur_final_output`, `m_thirtysecond_multilayer_blur_final_output`
  - ping/pong: `m_half_multilayer_ping/pong`, `m_quarter_multilayer_ping/pong`, `m_eighth_multilayer_ping/pong`, `m_sixteenth_multilayer_ping/pong`, `m_thirtysecond_multilayer_ping/pong`

### 4.3 Buffer resources

- `g_frosted_panels`:
  - structured SRV
  - capacity: `MAX_PANEL_COUNT`
  - payload type: `FrostedGlassPanelGpuData`
  - source: aggregated internal + external(manual/callback producer) panel descriptors
- `FrostedGlassGlobalBuffer`:
  - CBV
  - payload type: `FrostedGlassGlobalParams`
  - includes per-frame dominant directional-light payload for highlight modulation:
    - `highlight_light_dir_weight.xyz`: dominant directional-light direction (world space)
    - `highlight_light_dir_weight.w`: validity flag (`1` valid, `0` fallback)
  - includes AVP-style edge-spec phase-1 controls:
    - `edge_spec_intensity`
    - `edge_spec_sharpness`
    - `edge_highlight_width`
    - `edge_highlight_white_mix`
  - includes directional highlight contrast controls:
    - `directional_highlight_min`
    - `directional_highlight_max`
    - `directional_highlight_curve`

## 5. Expected Active Pass Count (for quick sanity checks)

- Fast branch + compute payload: 17 frosted pass nodes/frame (`15 base + 1 payload + 1 legacy composite`)
- Fast branch + raster payload: 18 frosted pass nodes/frame (`15 base + 2 payload raster + 1 legacy composite`)
- Strict branch + compute payload: 33 frosted pass nodes/frame (`15 base + 1 payload + 17 strict branch`)
- Strict branch + raster payload: 34 frosted pass nodes/frame (`15 base + 2 payload raster + 17 strict branch`)

If runtime observed pass count diverges from these numbers, inspect `Tick` registration logic first.

## 6. Update Checklist For Future Render Changes

- Update Section 2 if any runtime flag or branch condition changes.
- Update Section 3 if any pass name/order/dependency changes.
- Update Section 3.5 if any shader math/packing or blend semantics change.
- Update Section 4 if any resource handle, format, or ownership changes.
- Update Section 5 if node registration counts change.
- Keep this document and code change in the same PR; do not defer as follow-up.
