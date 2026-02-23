# RendererDemo Frosted Glass Development and Acceptance Plan

- Version: v1.0
- Date: 2026-02-23
- Scope: `RendererCore` + `RendererDemo`
- Status: Approved baseline for implementation and acceptance

## 1. Background and Current Baseline

This plan is based on the current implementation in repository `glTFRenderer`.

### Existing pipeline

- Scene base pass writes color/normal/depth render targets in `RendererSystemSceneRenderer`.
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemSceneRenderer.cpp`
- Lighting pass runs as compute and writes a single lighting output render target.
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
- Frosted glass pass runs as full-screen compute and currently acts as final output.
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`

### Existing framework capabilities

- RenderGraph supports graphics/compute/ray-tracing pass nodes and inferred dependencies.
  - `glTFRenderer/RendererCore/Public/RendererInterface.h`
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- Pass timing and dependency diagnostics are already exposed in debug UI.
  - `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`
- Window-relative render targets and resize lifecycle are implemented.
  - `glTFRenderer/RendererCore/Public/RendererInterface.h`
  - `glTFRenderer/RendererCore/Private/ResourceManagerSurfaceSync.cpp`

### Main limitations to address

- Lighting shader writes `LinearToSrgb(...)` before post effects, which limits physically coherent post-processing.
  - `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
- GBuffer is minimal and does not provide velocity/history-friendly data.
- Frosted glass is a single heavy pass and lacks temporal stabilization.
- Interactive state flow (hover/grab/move/scale) is not yet integrated into panel behavior.

## 2. Goals and Non-Goals

## In scope

- Extend framework and renderer pipeline to support high-quality frosted glass evolution.
- Provide a documented implementation path and acceptance criteria for phased delivery.
- Keep existing demo stability and debug visibility while adding new passes.

## Out of scope for first delivery

- Platform-native passthrough device integration.
- Eye-tracking driven foveated rendering integration.
- XR runtime and space-anchoring platform APIs.

## 3. Workstream A: Framework and Pipeline Extensions

## A1. Color pipeline upgrade to linear HDR (P0)

- Description:
  - Keep scene and lighting in linear HDR buffers.
  - Add a dedicated tone mapping pass before swapchain blit.
- Deliverables:
  - HDR intermediate target(s), tone map shader/pass, config switches.
- Primary files:
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemLighting.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/SceneLighting.hlsl`
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- Acceptance:
  - No direct sRGB conversion inside lighting pass output path.
  - Tone mapping is isolated and can be toggled for debugging.

## A2. GBuffer and view data extension (P0)

- Description:
  - Add material/aux channels needed by advanced glass composition.
  - Extend view buffer with previous frame matrix data.
- Deliverables:
  - Updated `ViewBuffer`, updated base pass outputs, updated consumers.
- Primary files:
  - `glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.h`
  - `glTFRenderer/RendererDemo/RendererModule/RendererModuleCamera.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/SceneViewCommon.hlsl`
  - `glTFRenderer/RendererDemo/Resources/Shaders/ModelRenderingShader.hlsl`
- Acceptance:
  - Previous-frame transform is uploaded per frame.
  - New channels are valid and visualized in debug checks.

## A3. PostFX shared resource foundation (P0)

- Description:
  - Introduce reusable half/quarter resolution render targets and ping-pong resources.
  - Standardize resource naming and resize behavior for postfx passes.
- Deliverables:
  - Reusable resource creation utilities/patterns and pass wiring.
- Primary files:
  - `glTFRenderer/RendererDemo/RendererSystem/*`
  - `glTFRenderer/RendererCore/Public/RendererInterface.h`
- Acceptance:
  - All postfx buffers are window-relative and resize-correct.
  - No stale descriptor/resource binding after resize.

## A4. Temporal history infrastructure (P1)

- Description:
  - Add history buffer lifecycle for temporal stabilization.
  - Add reset/invalidation paths on camera jump or resize.
- Deliverables:
  - History RT handles, frame-to-frame swap logic, invalidation policy.
- Acceptance:
  - Temporal buffers are valid across frames and reset correctly on resize.

## A5. Optional rendergraph QoL improvements (P1)

- Description:
  - Add convenience helpers for pass setup conventions and dependency declarations.
- Deliverables:
  - Helper wrappers or utility builders to reduce repetitive setup code.
- Acceptance:
  - New passes can be added with lower boilerplate and fewer binding mistakes.

## 4. Workstream B: Application and Effect Implementation

## B1. Frosted glass V2 multi-pass architecture (P0)

- Description:
  - Replace single heavy pass with structured stages:
    - mask/parameter stage
    - blur pyramid stage
    - final composite stage
- Deliverables:
  - Updated `RendererSystemFrostedGlass` pass graph and shaders.
- Primary files:
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.h`
  - `glTFRenderer/RendererDemo/RendererSystem/RendererSystemFrostedGlass.cpp`
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- Acceptance:
  - Visual parity or improvement over v1 while reducing per-frame cost growth.

## B2. Panel state machine and runtime control (P0)

- Description:
  - Add panel interaction states: idle, hover, grab, move, scale.
  - Drive blur/rim/fresnel/alpha via state curves.
- Deliverables:
  - State parameters in panel descriptors and runtime updates.
- Acceptance:
  - State transitions are smooth and deterministic without abrupt flicker.

## B3. Shape and layering upgrades (P1)

- Description:
  - Implement real mask path for irregular shapes.
  - Add robust panel layering and overlap handling.
- Deliverables:
  - `ShapeMask` path no longer fallback-only.
- Primary files:
  - `glTFRenderer/RendererDemo/Resources/Shaders/FrostedGlass.hlsl`
- Acceptance:
  - Irregular panel masks render correctly and stack predictably.

## B4. Readability protection and UI-safe behavior (P1)

- Description:
  - Add optional text-safe region strategy to avoid excessive blur behind critical UI labels.
- Deliverables:
  - Readability policy parameters and debug toggles.
- Acceptance:
  - UI legibility remains stable under high-frequency backgrounds.

## B5. Immersion blend baseline (P1)

- Description:
  - Add controllable blend between virtual background layers and glass composition.
- Deliverables:
  - `immersion_strength` style control and transition handling.
- Acceptance:
  - Transition path is smooth without abrupt exposure or contrast jumps.

## 5. Milestone Plan

## M0: Baseline lock and instrumentation

- Freeze references and acceptance checkpoints.
- Capture current screenshots and pass timings.

## M1: Pipeline correctness

- Complete A1 and A2.
- Confirm linear HDR path and updated GBuffer/view data.

## M2: Frosted V2 core

- Complete A3 and B1.
- Reach stable multi-pass frosted output.

## M3: Interaction and temporal stability

- Complete A4 and B2.
- Add stable state transitions and history behavior.

## M4: Quality completion

- Complete B3/B4/B5 as required by release scope.
- Final acceptance and baseline refresh.

## 6. Acceptance Criteria

## Functional acceptance

- All planned passes execute in correct order with valid dependencies.
- Resize/minimize/recover flows keep effect output valid.
- Debug UI can inspect and tune critical frosted parameters.

## Visual acceptance

- Panel edges, rim highlights, and refraction remain stable during camera and panel motion.
- No obvious halo popping or mask instability at panel boundaries.
- Readability under bright/dark and high-frequency backgrounds is acceptable.

## Performance acceptance

- Compare against frozen baseline in same scene/camera path.
- Report per-pass CPU/GPU timings using existing render graph timing UI.
- Keep frosted upgrade within agreed budget on target test machine.

## Stability acceptance

- No descriptor/resource binding errors during normal run.
- No regressions in resize lifecycle behavior.
- No frame execution skip caused by new pass validation issues.

## 7. Task Backlog Summary

| ID | Stream | Priority | Title | Exit Condition | Status | Evidence |
|---|---|---|---|---|---|---|
| A1 | Framework | P0 | Linear HDR + tone map | Lighting no longer outputs final sRGB directly | In Progress (A1.1 complete) | `Doc/FeatureNotes/20260223_A1_LinearOutput_ToneMapPass.md` |
| A2 | Framework | P0 | GBuffer/View extension | Prev-frame and extra channels available | In Progress (A2.1/A2.2/A2.3 complete) | `Doc/FeatureNotes/20260223_A2_ViewBufferPrevVP.md`, `Doc/FeatureNotes/20260223_A2_VelocityGBuffer.md`, `Doc/FeatureNotes/20260223_A2_VelocityDebugView.md` |
| A3 | Framework | P0 | PostFX shared resources | Half/quarter and ping-pong resources ready | Planned | - |
| B1 | App | P0 | Frosted V2 multi-pass | Multi-pass output replaces v1 single-pass | Planned | - |
| A4 | Framework | P1 | Temporal history infra | Stable history lifecycle and reset path | Planned | - |
| B2 | App | P0 | Panel state machine | Idle/hover/grab/move/scale complete | Planned | - |
| B3 | App | P1 | Shape mask path | Irregular mask path implemented | Planned | - |
| B4 | App | P1 | Readability protection | Text-safe behavior verified | Planned | - |
| B5 | App | P1 | Immersion blend | Smooth blend control verified | Planned | - |
| A5 | Framework | P1 | RenderGraph QoL helpers | Reduced setup boilerplate | Planned | - |

## 8. Development and Review Rules for This Plan

- Every completed task must update:
  - changed file list
  - before/after behavior summary
  - acceptance evidence (timing/screenshot/checklist)
- No task is considered done without at least one measurable acceptance proof.
- Keep this file as the source of truth for scope and acceptance.

## 9. Document Location and Usage

- Archive path: `Doc/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
- This document is the implementation and acceptance contract for upcoming work.

## 10. Development Cadence Agreement

This section defines the default execution rhythm for all upcoming development work on this plan.

### 10.1 Smallest Work Item First

- Development should align to the smallest deliverable task granularity (single backlog item or one clear sub-item).
- One iteration should target one primary objective only:
  - one behavior change
  - one validation scope
  - one acceptance conclusion
- Avoid bundling unrelated improvements in a single implementation cycle.

### 10.2 Iteration Workflow (Required)

1. Select one backlog item (or one explicit sub-item) from Section 7.
2. Implement only that scoped change.
3. Run validation for that change (functional + visual/perf as applicable).
4. Write a feature note document for this iteration (Section 10.3).
5. Update this main plan document status fields (Section 10.4).
6. Mark the item as accepted only after evidence is attached.

### 10.3 Feature Note Archival (Required After Each Passed Iteration)

- A standalone note must be added for each validated iteration under:
  - `Doc/FeatureNotes/`
- Use template:
  - `Doc/FeatureNotes/Feature_Note_Template.md`
- Recommended naming:
  - `YYYYMMDD_WORKITEMID_ShortName.md`
  - Example: `20260224_B1_FrostedMaskPass.md`

Each feature note must include:

- Work item ID and scope boundary
- Functional logic summary
- Algorithm or shader logic summary
- Changed file list
- Validation evidence (timings/screenshots/checklists)
- Risks and follow-up tasks

### 10.4 Main Plan Update Rules (Required)

After each validated iteration, update this file with:

- task status change (planned/in-progress/done)
- acceptance evidence link to the feature note file
- any scope/schedule/risk adjustments

If acceptance fails, keep item open and record failure reason plus next action.

### 10.5 Definition of Done (Iteration Level)

An iteration is done only if all conditions are met:

- scoped code change is complete
- validation has been executed and recorded
- feature note has been archived in `Doc/FeatureNotes/`
- this main plan has been updated accordingly
