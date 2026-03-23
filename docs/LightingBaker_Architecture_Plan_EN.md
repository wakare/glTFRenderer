# LightingBaker Architecture And Phased Plan

This document is the English companion of `docs/LightingBaker_Architecture_Plan.md`.

Shared class diagram:

- `docs/LightingBaker_KeyClass_Diagram.md`

## 1. Goal And Scope

The goal is to add a new standalone executable, `glTFRenderer/LightingBaker`, for path-traced lightmap baking. It should be independent from `RendererDemo`, while reusing the maintained runtime path:

- `glTFRenderer/RendererCore`
- `glTFRenderer/RHICore`
- `glTFRenderer/RendererScene`

Existing legacy DXR/path-tracing code is still useful, but only as an algorithm and data-layout reference. It should not become the new host framework.

Out of scope for the first stage:

- automatic unwrap
- true headless RHI
- dirty-region incremental rebake
- animation baking
- denoising

Stage 1 should still support progressive accumulation, pause, and resume. That is not the same thing as local incremental rebake.

## 2. What Can Be Reused

### 2.1 Directly reusable

- `RHICore` already provides RT PSO, shader table, BLAS/TLAS, and `TraceRay(...)`.
- `RendererCore` already understands `RenderPassType::RAY_TRACING`.
- `RendererScene` already loads glTF scenes and exposes mesh/material traversal.
- `RendererSceneResourceManager` already bridges scene data into the maintained module/system path.

### 2.2 Reference-only legacy pieces

These files are still valuable references:

- `glTFRenderer/glTFRenderer/glTFRenderPass/glTFRayTracingPass/glTFRayTracingPassPathTracing.cpp`
- `glTFRenderer/glTFRenderer/glTFRenderPass/glTFRayTracingPass/glTFRayTracingPassWithMesh.cpp`
- `glTFRenderer/glTFRenderer/glTFResources/ShaderSource/RayTracing/PathTracingMain.hlsl`
- `glTFRenderer/glTFRenderer/glTFResources/ShaderSource/RayTracing/PathTracingRays.hlsl`
- `glTFRenderer/glTFRenderer/glTFResources/ShaderSource/RayTracing/LightSampling.hlsl`

But the current legacy path tracer is screen-space and camera-driven:

- it depends on `SceneView.hlsl`
- it maps `DispatchRaysIndex()` to screen UV
- it generates primary rays from camera matrices
- it outputs `screen_uv_offset`

That is the wrong domain for a lightmap baker, which needs atlas-texel-domain ray generation.

## 3. Current Framework Gaps

### 3.1 UV1 / lightmap UV gap

The maintained mesh accessor path currently exposes only `VERTEX_TEXCOORD0_FLOAT2`. It does not expose `VERTEX_TEXCOORD1_FLOAT2`.

The gap exists in multiple places:

- `glTFRenderer/RendererScene/Private/RendererSceneGraph.cpp` only loads `TEXCOORD_0`
- `glTFRenderer/RendererCore/Private/RendererInterface.cpp` only exports UV0 in `RendererSceneResourceManager::AccessSceneData(...)`
- `glTFRenderer/RendererDemo/RendererModule/RendererModuleSceneMesh.cpp` only consumes UV0

That blocks independent lightmap UV usage.

Additional clarification:

- the presence of `UV1` does not mean it is valid as lightmap UVs
- the baker must validate overlap, padding, degenerate triangles, out-of-range UVs, and texel coverage at the target resolution
- `LightmapAtlasBuilder` must build a `valid texel mask`, not just check that the attribute exists
- if authored UV1 is invalid in the MVP stage, the baker should fail fast and report the bad primitive or chart instead of silently producing wrong results

### 3.2 Modern RT execution gap

The maintained runtime already accepts:

- `RenderPassType::RAY_TRACING`
- `ExecuteCommandType::RAY_TRACING_COMMAND`

But the modern execution path is incomplete:

- `RenderGraph::ExecuteRenderGraphNode(...)` currently leaves `RAY_TRACING_COMMAND` empty
- `RenderExecuteCommand` has no RT-specific payload for trace dimensions or shader-table dispatch
- `RenderPassDrawDesc` has no maintained binding path for TLAS or shader table objects

So if `LightingBaker` is built on the maintained framework, that RT chain must be completed first.

### 3.3 Offline output gap

Current visible output paths are mostly:

- blitting final output to the swapchain
- window screenshot capture from `RendererDemo`

Lightmap baking instead needs:

- GPU texture readback
- HDR texture export
- bake metadata output
- intermediate debug dumps

That requires a real offline output path, not a screenshot path.

## 4. Recommended Module Split

Recommended first-pass module layout inside `glTFRenderer/LightingBaker`:

| Module | Responsibility |
| --- | --- |
| `App/LightingBakerApp` | CLI entry, runtime host, hidden-window phase-1 execution, batch-job control |
| `App/BakeJobConfig` | job parsing for scene path, output path, bake resolution, samples, bounces, environment, and quality presets |
| `Scene/BakeSceneImporter` | wraps `RendererSceneResourceManager` and builds baker-facing mesh/material/light views |
| `Bake/Atlas/LightmapAtlasBuilder` | validates authored UV1 overlap, padding, and coverage in MVP, later extends to unwrap/chart/atlas generation |
| `Bake/Scene/BakeTexelScene` | builds atlas-ordered texel records: texel-to-triangle mapping, barycentrics, world position, normal, tangent, material, chart id |
| `Bake/RT/BakerRayTracingScene` | builds BLAS/TLAS, geometry tables, instance tables, and shader-table dependencies |
| `Bake/Passes/LightmapPathTracingPass` | performs atlas-domain ray generation and multi-bounce light transport |
| `Bake/Post/BakeAccumulator` | accumulation, convergence tracking, pause/resume state, dilation, and seam repair |
| `Output/BakeOutputWriter` | texture readback, published package output, debug export, and bake manifests |
| `Debug/BakeDebugDumps` | exports intermediate validity, chart, sample-count, or first-bounce debug data |

Suggested project tree:

```text
glTFRenderer/LightingBaker/
  LightingBaker.vcxproj
  App/
  Scene/
  Bake/
    Atlas/
    Scene/
    RT/
    Passes/
    Post/
  Output/
  Debug/
  Resources/
    Shaders/
```

## 5. Required Changes By Subsystem

### 5.1 `glTFRenderer/RendererCore`

This is the most important layer to extend.

Required:

1. Add RT-specific dispatch data to `RenderExecuteCommand`
2. Execute `RAY_TRACING_COMMAND` by calling `RHIUtilInstanceManager::Instance().TraceRay(...)`
3. Add a maintained binding path for TLAS and shader-table resources
4. Add offline bake texture readback support

Recommended later:

- a true no-swapchain offline mode
- optional final-output blit/present for offline jobs

### 5.2 `glTFRenderer/RendererScene` and `RendererSceneResourceManager`

Required:

1. Load and store `TEXCOORD_1`
2. Add `VERTEX_TEXCOORD1_FLOAT2` to `MeshDataAccessorType`
3. Export UV1 through `RendererSceneResourceManager::AccessSceneData(...)`

Recommended later:

- bake-side auxiliary geometry access such as per-triangle area or chart helpers
- sidecar structures for generated lightmap UV data

### 5.3 `glTFRenderer/RHICore`

Large RT refactors should not be needed first, because the low-level pieces already exist. The likely additions are:

- readback/staging helpers
- later cleanup for swapchain-optional or headless initialization

### 5.4 `glTFRenderer/RendererDemo`

`LightingBaker` should not live inside `RendererDemo`. However, if shared maintained interfaces change, `RendererDemo` must follow:

- `RendererModuleSceneMesh`
- `SceneRendererCommon.hlsl`
- any implementation of `RendererSceneMeshDataAccessorBase`

Those are compatibility updates, not feature hosting.

Recommended runtime integration should use a sidecar package instead of rewriting the source glTF in the first stage:

- after loading the scene, `RendererDemo` should first look for `<scene_stem>.lmbake/manifest.json`
- a compatibility fallback may also check `<scene>.lmbake/manifest.json`
- lightmap binding should be per primitive or per instance, not per material
- the binding must not rely on runtime-generated `mesh_id` or `node.GetID()` values
- the more stable key design is `primitive_hash`, plus an optional instance override key `node_key`
- the runtime should add a dedicated `RendererModuleLightmap` or equivalent module for atlas textures and binding tables
- geometry or lighting stages should read `UV1 + lightmap_binding_index` and compose `direct_dynamic + baked_diffuse_indirect + specular_indirect`
- baked diffuse indirect must not be double-counted with the current environment diffuse path

## 6. Recommended Lightmap Data Flow

Recommended bake pipeline:

1. Load the scene through `BakeSceneImporter`
2. Validate or build atlas data in `LightmapAtlasBuilder`
3. Build atlas-texel records in `BakeTexelScene`
4. Build BLAS/TLAS and related tables in `BakerRayTracingScene`
5. Run atlas-domain RT in `LightmapPathTracingPass`
6. Accumulate and repair in `BakeAccumulator`
7. Read back and export in `BakeOutputWriter`

### 6.1 Output package layout

The baker output should be split into two layers:

- `cache`: internal progressive accumulation state used only by the baker
- `published package`: runtime-facing artifacts consumed by `RendererDemo` or other clients

Suggested layout:

```text
<scene_stem>.lmbake/
  manifest.json
  atlases/
    indirect_00.master.rgba16f.bin
    indirect_00.runtime.logluv8.bin
    indirect_00.preview.png
  debug/
    coverage_00.png
    chart_id_00.png
    invalid_uv.png
  cache/
    resume.json
    texel_records_00.bin
    accum_00.rgb32f.bin
    sample_count_00.r32ui.bin
    variance_00.r32f.bin
```

`manifest.json` should contain at least:

- schema version
- source scene path, bake id, and profile
- integrator settings such as target spp and max bounces
- validation hashes for geometry, materials, and UV1
- atlas list, dimensions, format, and semantic
- primitive or instance to atlas bindings
- stable binding keys such as `primitive_hash`, plus an optional instance override key `node_key`
- runtime codec and decode parameters

`resume.json` should remain baker-only and serve as the authoritative index for `cache/`, including at least:

- `atlas_inputs`
- per-atlas `texel_record_file`, `texel_record_count`, and `texel_record_stride`
- progressive cache files such as `accumulation_file`, `sample_count_file`, and `variance_file`
- progressive state such as `completed_samples`

The current scaffold already writes:

- `debug/import_summary.json`
- `debug/atlas_summary.json`
- `cache/texel_records_00.bin`
- `cache/accum_00.rgb32f.bin`
- `cache/sample_count_00.r32ui.bin`
- `cache/variance_00.r32f.bin`

The first shipped step is currently "`--resume` validates and preserves cache":

- re-import the scene and rebuild atlas texel records
- validate `resume.json`, texel record counts, cache file sizes, and key bake parameters
- preserve the existing progressive cache without rewriting `resume.json` or accumulation payloads once validation passes

Actual "continue accumulating from the previous sample count" remains a later stage and should be completed after the DXR bake pass is integrated.

Even if the internal cache format changes later, `resume.json` should remain the single entry point so DXR bake passes and tools do not hardcode file names.

### 6.2 Compression and codec strategy

Indirect diffuse lighting is usually low-frequency and smooth. The storage design should therefore separate:

- signal design: store `diffuse_irradiance` instead of mixing in high-frequency direct lighting
- encoding design: provide a compact runtime codec while keeping a high-precision master

Recommended rollout order:

1. keep `cache` high precision and lossless
2. use `RGBA16F` for the first published master
3. add a low-frequency-friendly runtime codec such as `LogLuv8` or `YCoCg8`
4. add `BC6H` later if the lower-level compressed texture path is implemented

The first stage should not make runtime loading depend on EXR decoding. A safer initial package is:

- master: `*.rgba16f.bin`
- runtime: `*.logluv8.bin`, or the master payload temporarily
- preview: `*.png`

The key architectural shift is this:

- current legacy path tracing is screen-space
- the baker must become atlas-texel-space

## 7. Suggested Milestones

### Phase A: Project scaffold

- create `LightingBaker.vcxproj`
- wire `RendererCore`, `RHICore`, `RendererScene`, and `RendererCommonLib`
- add CLI and hidden-window runtime startup

### Phase B: Modern RT execution enablement

- complete `RAY_TRACING_COMMAND`
- add shader-table and TLAS binding
- validate a minimal modern RT node

### Phase C: UV1 and texel-scene path

- add UV1 loading/export
- build atlas texel records
- output debug masks for validation

### Phase D: Lightmap path tracing MVP

- support authored UV1
- support static meshes
- implement direct light, environment, and diffuse bounces
- support full-scene progressive accumulation / pause / resume

### Phase E: Offline export

- texture readback
- master HDR output
- runtime codec output
- bake metadata and debug artifacts
- progressive cache / resume files

### Phase F: Extensions

- automatic unwrap/charting
- true headless mode
- incremental baking
- denoising
- additional importance sampling and transport refinements

## 8. Recommended MVP Scope

To get a first version working quickly, keep the first scope constrained to:

- static scenes
- static lights
- authored UV1 first
- one HDR lightmap output plus PNG debug views
- full-scene progressive baking
- explicit failure on invalid authored UV1 instead of silent fallback
- no skinned meshes, no animation, no auto-unwrap, no denoiser in the first pass

This is an implementation staging decision, not a long-term capability reduction.

## 9. Current Conclusion

From the current codebase, a standalone `LightingBaker` is feasible. But the first priority is not the baking algorithm itself. The first priority is to close three shared infrastructure gaps:

1. modern `RendererCore` RT command execution
2. UV1 / lightmap data flow through `RendererScene` and `RendererSceneResourceManager`
3. offline texture readback and export

Once those are in place, porting the existing DXR/path-tracing prototype into atlas-texel space becomes a much lower-risk step.
