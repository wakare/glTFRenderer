# Project Documentation Directory

This file is the canonical documentation index for the repo.

## Repo Root Convention

- Canonical repo root is the current directory (currently `C:\glTFRenderer`).
- `docs/` and `scripts/` are top-level folders under repo root.
- Source code and solution content live under the `glTFRenderer/` solution subtree.
- Repo-relative code paths should therefore start with `glTFRenderer/`.
- Runtime-CWD-sensitive paths such as `Resources/...` or `glTFResources/...` are only used when documenting executable launch behavior.

## Directory Map

- `README.md`
  - Repo overview and quick entry point.
- `AGENTS.md`
  - Root workflow conventions for build/log/doc handling.
- `docs/`
  - Canonical project documentation tree for architecture, plans, contracts, feature-note archives, and debug notes.
- `glTFRenderer/scripts/*.md`
  - Workflow and perf-loop documentation that belongs with the corresponding automation scripts.

## Root Entry Points

- `README.md`
  - Start here for high-level repo layout.
- `AGENTS.md`
  - Start here for operational rules during coding/debugging sessions.
- `docs/DocReferencePolicy.md`
  - Start here when editing documentation references.

## Architecture And Design Docs

- `docs/RendererFramework_Architecture_Summary.md`
  - End-to-end overview: layer split, frame flow, resource lifecycle, resize/swapchain behavior, current app-layer feature toolkit.
- `docs/RendererDemo_AppLayer_Toolkit_Design.md`
  - Current `RendererDemo` app-layer toolkit design: `DemoBase`, typed outputs, execution plans, pass builder, node lifecycle helpers.
- `docs/RendererDemo_AppLayer_Toolkit_Usage.md`
  - Practical usage guide for adding demos and render features with the current toolkit.
- `docs/RDG_Algorithm_Notes.md`
  - RenderGraph algorithm details: access extraction, inferred dependencies, execution plan build/apply, diagnostics.
- `docs/SurfaceResizeCoordinator_Design.md`
  - Resize coordinator and surface resource rebuilder split.
- `docs/DocReferencePolicy.md`
  - Stable documentation reference rules and validation workflow.

## Contracts And Plans

- `docs/RendererDemo_FrostedGlass_Development_Acceptance_Plan.md`
  - Main implementation and acceptance contract for frosted-glass work.
- `docs/RendererDemo_FrostedGlass_FramePassReference.md`
  - Frame-level pass/resource contract for the current frosted-glass path.

## Feature Note Archive

- `docs/FeatureNotes/Feature_Note_Template.md`
  - Template for new archived feature notes.
- `docs/FeatureNotes/20260223_A1_LinearOutput_ToneMapPass.md`
- `docs/FeatureNotes/20260223_A2_ViewBufferPrevVP.md`
- `docs/FeatureNotes/20260223_A2_VelocityGBuffer.md`
- `docs/FeatureNotes/20260223_A2_VelocityDebugView.md`
- `docs/FeatureNotes/20260223_A3_PostFXSharedResources.md`
- `docs/FeatureNotes/20260223_B1_MultipassScaffold.md`
- `docs/FeatureNotes/20260223_B1_QuarterBlurPyramid.md`
- `docs/FeatureNotes/20260224_A4_TemporalHistoryInfrastructure.md`
- `docs/FeatureNotes/20260224_B1_FrostedMaskPass.md`
- `docs/FeatureNotes/20260224_B2_PanelStateMachineRuntimeControl.md`
- `docs/FeatureNotes/20260224_B3_ShapeMaskLayering.md`
- `docs/FeatureNotes/20260224_B6_AdaptiveMultilayerRefinement.md`
- `docs/FeatureNotes/20260224_B6_MultilayerCompositionPlan.md`
- `docs/FeatureNotes/20260224_B6_Top2MultilayerCore.md`
- `docs/FeatureNotes/20260225_B7_PanelGBufferTwoLayerCompatibilityPlan.md`
- `docs/FeatureNotes/20260225_DX12VK_DescriptorPolicyUnificationPlan.md`
- `docs/FeatureNotes/20260227_B8_BlurSourceOptimizationPlan.md`
- `docs/FeatureNotes/20260228_B9_FixedViewpointScreenshotRegressionPlan.md`
- `docs/FeatureNotes/20260228_B9_RegressionWorkflow_Runbook.md`

Naming convention:

- `YYYYMMDD_<work-item>_<short-title>.md`
- Use `Feature_Note_Template.md` for new entries.

## Runtime-Local Docs

- `docs/debug-notes/README.md`
  - Accepted bug-note rules and current note list.
- `docs/debug-notes/TEMPLATE.md`
  - Template for accepted bug notes.
- `docs/debug-notes/*.md`
  - Accepted runtime/debug investigations with final root cause and validation.

## Workflow Docs Beside Scripts

- `glTFRenderer/scripts/RendererDemo-PerfLoop.md`
  - DX12/Vulkan regression and perf loop usage.
- `glTFRenderer/scripts/RendererFramework-RefactorRoadmap.md`
  - Refactor order, non-regression gates, and perf guardrails.

## Placement Rules

- Put long-lived architecture/design/plan documents under `docs/`.
- Put accepted runtime bug notes and runtime-local operational notes under `docs/debug-notes/`.
- Put script-specific workflow docs beside the corresponding scripts in `glTFRenderer/scripts/`.
- Do not create another project-owned documentation tree outside `docs/` unless the current split becomes insufficient.

## Maintenance Rules

1. Prefer `path + symbol` references; use line numbers only when line context is essential.
2. When moving or renaming symbols, update docs in the same change.
3. Update this file in the same change when a project-owned doc is added, removed, or repurposed.
4. Validate references before merge:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Validate-DocReferences.ps1
```

