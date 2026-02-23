# Documentation Reference Policy

## 1. Purpose

Keep architecture/algorithm docs trustworthy while minimizing maintenance cost during active refactors.

## 2. Preferred Reference Style

Use `path + symbol` as the primary reference. Keep line numbers optional.

- Recommended:
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
  - Mention symbol name in plain text, e.g. `ExecuteRenderGraphFrame`.
- Optional (only when line context is important):
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp:1501`
  - `glTFRenderer/RendererCore/Private/RendererInterface.cpp#L1501`

Reason:
- Path + symbol is stable across most edits.
- Hard-pinned line numbers become stale quickly in fast-moving code.

## 3. When References Must Be Updated

Update doc references in the same change when:

1. A referenced file is moved/renamed/deleted.
2. A referenced symbol is renamed or moved to another file.
3. A line-pinned reference points to a wrong/out-of-range location.

## 4. Validation Command

Run before merge:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Validate-DocReferences.ps1
```

Behavior:
- Scans `Doc/**/*.md`.
- Validates backticked file refs like `path`, `path:line`, `path#Lline`.
- Returns non-zero on any error.

## 5. PR Checklist Suggestion

Add this checklist item to PRs that touch docs or referenced code:

- [ ] Documentation references validated (`scripts/Validate-DocReferences.ps1`).
