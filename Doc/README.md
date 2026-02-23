# Documentation Index

## Scope

This folder contains architecture/algorithm docs for the current renderer framework implementation.

## Files

- `Doc/RendererFramework_Architecture_Summary.md`
  - End-to-end overview: layer split, frame flow, resource lifecycle, resize/swapchain behavior.
- `Doc/RDG_Algorithm_Notes.md`
  - RenderGraph algorithm details: access extraction, inferred dependencies, plan build/apply, diagnostics.
- `Doc/SurfaceResizeCoordinator_Design.md`
  - Internal split between resize coordinator and resource rebuilder.
- `Doc/DocReferencePolicy.md`
  - Rules for stable references and doc reference validation.

## Maintenance Rules

1. Prefer `path + symbol` references; use line numbers only when line context is essential.
2. When moving/renaming symbols, update docs in the same change.
3. Validate references before merge:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Validate-DocReferences.ps1
```

## Correctness Audit (2026-02-23)

- Updated core docs to remove stale hard-pinned line references.
- Rebound key references to concrete symbols in current implementation files.
