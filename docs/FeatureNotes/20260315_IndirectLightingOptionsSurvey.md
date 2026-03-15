# Feature Note - 2026-03 Indirect Lighting Options Survey For ModelViewer

## 1. Purpose

This note records the current mainstream real-time indirect lighting options that are relevant to `DemoAppModelViewer`, and narrows them down to a practical implementation order for this repository.

The goal is not to pick a final solution immediately. The goal is to establish:

- what the industry is commonly shipping today,
- what each option needs from the renderer,
- which option best matches the current `glTFRenderer` architecture.

## 2. Current Repository Baseline

The current `ModelViewer` lighting path is still direct-light dominant:

- `glTFRenderer/RendererDemo/Resources/Shaders/RendererModule/RendererModuleLighting.hlsl`
  `GetLighting(...)` loops over lights and accumulates direct BRDF lighting with visibility / shadowing.
- `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
  reads the SSAO buffer and currently applies it to a small ambient placeholder term.

What the renderer already has:

- deferred-style scene buffers from `RendererSystemSceneRenderer`,
- normal + depth buffers suitable for SSAO and future screen-space GI,
- DX12 and Vulkan backends,
- a compute-heavy render graph style that is friendly to post / lighting compute passes.

What it does not have yet:

- environment IBL diffuse / specular lighting,
- reflection probe or irradiance probe infrastructure,
- lightmaps or baked probe volume workflow,
- screen-space GI temporal reconstruction history,
- ray tracing acceleration structure path,
- sparse distance field / radiance cache GI infrastructure.

This means the current SSAO integration is a transitional step, not a complete indirect-light solution.

## 3. Mainstream Industry Option Families

### 3.1 Image-Based Lighting (IBL) And Reflection Probes

This is still the baseline indirect-light solution for PBR model viewers, product viewers, and many game engines.

Typical form:

- diffuse indirect from irradiance,
- specular indirect from prefiltered environment maps,
- optional local reflection probes for indoor or room-scale variation,
- SSAO used only to attenuate diffuse indirect, not direct light.

Why it is mainstream:

- low implementation risk,
- stable image quality,
- fits glTF / PBR workflows naturally,
- works well for static model viewing and look-dev.

What it needs:

- HDR environment input,
- irradiance generation or SH projection,
- prefiltered specular cubemap and BRDF LUT,
- integration into the lighting pass.

What it does not solve well:

- local diffuse bounce caused by nearby moving geometry,
- strong multi-bounce color bleeding in large interior scenes,
- dynamic emissive-to-scene GI without extra probe capture or GI logic.

### 3.2 Baked Lightmaps Plus Probe Volumes

This remains common for scenes where geometry and lighting are mostly static, or where quality-per-watt matters more than fully dynamic lighting.

Typical form:

- baked lightmaps for static geometry,
- light probes / adaptive probe volumes for dynamic objects,
- reflection probes or sky lighting for indirect specular,
- SSAO used as local contact shadow enhancement.

Why it is mainstream:

- excellent cost-to-quality ratio,
- predictable performance,
- mature editor workflows across engines.

What it needs:

- baking pipeline and authoring workflow,
- persistent probe / lightmap assets,
- streaming and blending rules for large scenes.

What it does not solve well:

- highly dynamic scenes,
- runtime-destructible environments,
- dynamic lighting changes without rebake.

### 3.3 Screen-Space GI (SSGI)

SSGI is the most common incremental real-time GI step after SSAO when a renderer already has G-buffer and temporal history.

Typical form:

- ray march in screen space using depth / normal / color,
- compute diffuse bounce only from visible pixels,
- reproject and denoise temporally,
- blend with existing ambient / sky / probe lighting.

Why it is mainstream:

- comparatively low integration cost,
- naturally reuses deferred buffers,
- good local bounce and contact color bleeding for on-screen content,
- works on non-RT hardware.

What it needs:

- depth, normals, scene color or lighting history,
- motion vectors and temporal stabilization,
- denoising / history rejection logic,
- careful artifact management.

What it does not solve well:

- off-screen lighting contribution,
- missing data behind thin geometry or disocclusions,
- stable large-scale indoor GI without help from probes or IBL.

### 3.4 Probe-Based Dynamic GI (DDGI / Probe Volumes)

This family is now one of the most practical "scene-scale dynamic diffuse GI" solutions in production pipelines.

Typical form:

- place probes or probe volumes through world space,
- capture / trace incident lighting into probes,
- store irradiance or SH,
- sample probes during lighting,
- optionally update probes dynamically or partially.

Why it is mainstream:

- off-screen indirect lighting support,
- better scene-scale stability than SSGI,
- scalable across quality tiers,
- more practical than full path-traced GI for many titles.

What it needs:

- probe placement and update policy,
- irradiance storage and blending,
- leakage handling and visibility heuristics,
- scene update scheduling,
- optionally ray tracing, SDF tracing, or raster-assisted capture.

What it does not solve well:

- very sharp or high-frequency indirect lighting,
- fully accurate glossy multi-bounce transport,
- zero-setup authoring if large worlds or fast moving geometry are involved.

### 3.5 Full Dynamic GI Systems (Lumen / Brixelizer GI / RTGI Families)

At the high end, the industry trend is toward fully dynamic or near-fully dynamic GI systems that combine multiple representations:

- screen traces,
- software or hardware ray tracing,
- distance fields or scene cards,
- radiance caches,
- probe-like or cache-like reuse structures.

Representative public examples:

- Unreal Engine 5 Lumen,
- AMD FidelityFX Brixelizer GI,
- hardware RTGI stacks built around ray tracing plus temporal caches.

Why this family is popular:

- strong scene-scale bounce lighting,
- better handling of dynamic lighting and emissives,
- better path toward "modern AAA" fully dynamic lighting.

What it needs:

- substantially more renderer infrastructure,
- richer geometry acceleration or scene proxy structures,
- temporal caches and denoisers,
- stricter memory and update budgeting,
- more debugging tools.

What it does not solve well:

- low implementation complexity,
- low-end hardware support without fallback tiers,
- quick adoption in a compact renderer.

## 4. Current Industry Reading

As of 2026-03, the most common patterns visible in public engine and vendor documentation are:

1. IBL is still the baseline indirect-light layer for physically based renderers and viewers.
2. Baked lightmaps plus probes remain common whenever scenes are mostly static.
3. SSGI remains a mainstream incremental dynamic GI technique for deferred renderers.
4. Probe-based dynamic GI remains attractive when off-screen diffuse bounce matters.
5. High-end engines increasingly move toward hybrid GI systems with caches plus tracing rather than a single simple pass.

In short:

- for viewers and simple runtime scenes, IBL is still the first missing piece,
- for medium-cost dynamic GI, SSGI and probe GI are the practical middle tier,
- for high-end dynamic GI, hybrid cache-plus-tracing systems are where the industry is heading.

## 5. Comparison For This Repository

| Option | Industry role today | Scene dynamism | Typical outputs | Integration cost here | Main risk |
|---|---|---|---|---|---|
| IBL + reflection probes | Baseline and very common | Low to medium | Diffuse indirect + specular indirect | Low | Limited local bounce |
| Baked lightmaps + probes | Common for static / mixed scenes | Low | Stable diffuse GI | High outside editor tooling | Needs bake workflow |
| SSGI | Common incremental dynamic GI | Medium | Diffuse indirect only | Medium | On-screen only, temporal artifacts |
| DDGI / probe volumes | Common practical dynamic GI tier | Medium to high | Scene-scale diffuse indirect | Medium to high | Probe leakage / update complexity |
| Lumen / Brixelizer GI / RTGI | High-end trend | High | Large-scale diffuse and often specular GI | Very high | Large system scope |

## 6. Recommended Implementation Order For glTFRenderer

### 6.1 First Recommended Step

Implement environment indirect lighting first:

- HDR sky / cubemap input,
- irradiance diffuse term,
- prefiltered specular IBL,
- BRDF LUT,
- SSAO applied only to diffuse indirect.

Why this should be first:

- it closes the biggest correctness gap in the current `ModelViewer`,
- it is the standard expectation for a glTF/PBR viewer,
- it makes the existing SSAO integration land in the right semantic place,
- it avoids committing early to a much heavier GI architecture.

### 6.2 Second-Step Candidates

After IBL is in place, choose one of these depending on the goal:

- If the goal is low-to-medium cost dynamic bounce on the current architecture:
  SSGI is the most direct next step.
- If the goal is stable off-screen diffuse bounce across a full scene:
  DDGI / probe volumes are the stronger long-term candidate.

### 6.3 What Should Not Be The First GI Implementation Here

The following should not be the first indirect-light implementation for this repository:

- full Lumen-like hybrid GI,
- Brixelizer-class sparse-distance-field GI,
- hardware RTGI-first architecture.

They are important references for long-term direction, but they demand too much supporting infrastructure relative to the current renderer state.

## 7. Suggested Decision Path

The most defensible decision path for this repository is:

1. Add environment IBL diffuse + specular.
2. Re-home SSAO so it modulates diffuse indirect rather than a placeholder constant.
3. Decide whether the next target is:
   - SSGI for lower integration cost, or
   - probe-based GI for better scene-scale stability.
4. Revisit high-end hybrid GI only after the renderer has:
   - stable history management,
   - better indirect-light debugging views,
   - a clearer scene representation strategy for off-screen tracing.

## 8. Sources

- [Epic Games: Lumen Technical Details in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/lumen-technical-details-in-unreal-engine)
- [Epic Games Tech Blog: Unreal Engine 5 goes all-in on dynamic global illumination with Lumen](https://www.unrealengine.com/en-US/tech-blog/unreal-engine-5-goes-all-in-on-dynamic-global-illumination-with-lumen)
- [Unity HDRP Features Overview](https://docs.unity.cn/Packages/com.unity.render-pipelines.high-definition%4017.0/manual/HDRP-Features.html)
- [Unity HDRP: Screen Space Global Illumination](https://docs.unity.cn/Packages/com.unity.render-pipelines.high-definition%4016.0/manual/Override-Screen-Space-GI)
- [NVIDIA RTXGI-DDGI](https://github.com/NVIDIAGameWorks/RTXGI-DDGI)
- [AMD GPUOpen: FidelityFX Brixelizer / GI](https://gpuopen.com/fidelityfx-brixelizer/)
- [AMD GPUOpen Manual: FidelityFX Brixelizer GI](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/brixelizer-gi/)
- [Google Filament: Image Based Lights](https://google.github.io/filament/main/filament.html)
