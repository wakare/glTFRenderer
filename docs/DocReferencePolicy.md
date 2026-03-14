# Documentation Reference Policy

## 1. Purpose

Keep architecture/algorithm docs trustworthy while minimizing maintenance cost during active refactors.

## 2. Repo Root Convention

All repo-relative documentation paths are based on the current repo root (currently `C:\glTFRenderer`).

- Use `glTFRenderer/...` for source code and solution-tree files.
- Use `scripts/...`, `docs/...`, `AGENTS.md`, and `README.md` for top-level utilities and docs.
- Keep runtime-only working-directory-sensitive paths such as `Resources/...` or `glTFResources/...` only when documenting executable launch behavior.

## 3. Preferred Reference Style

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

## 4. When References Must Be Updated

Update doc references in the same change when:

1. A referenced file is moved/renamed/deleted.
2. A referenced symbol is renamed or moved to another file.
3. A line-pinned reference points to a wrong/out-of-range location.

## 5. Validation Command

Run before merge:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Validate-DocReferences.ps1
```

Behavior:
- Scans `docs/**/*.md`.
- Validates backticked file refs like `path`, `path:line`, `path#Lline`.
- Returns non-zero on any error.

## 6. Language And Diagram Policy

For project-owned docs under `docs/`:

1. Any new doc or materially updated doc should have both Chinese and English maintenance coverage.
2. Existing unsuffixed docs may stay in place for backward compatibility, but add a companion file beside them using `_EN.md` or `_ZH.md`.
3. Rendering-architecture and app-layer framework docs should include a key-class dependency diagram.
4. That diagram should show:
   - the main class dependency direction
   - each key class's core responsibility
5. Diagram docs may be maintained as a single bilingual source file when the content is primarily graphical and the labels/captions are bilingual.

Preferred format:

- Mermaid in markdown under `docs/`

## 7. PR Checklist Suggestion

Add this checklist item to PRs that touch docs or referenced code:

- [ ] Documentation references validated (`scripts/Validate-DocReferences.ps1`).
- [ ] Documentation statements re-checked against current symbol behavior (not only path existence).
- [ ] If the doc is new or materially updated, Chinese and English coverage was updated together.
- [ ] If the doc is rendering-architecture related, the key-class diagram was updated or re-checked.

## 8. Lightweight Correctness Audit

For architecture/algorithm docs, run this quick audit before merge:

1. Pick each section's key claim (for example: "who owns lifecycle", "which function drives execution").
2. Verify it against the current symbol implementation in code.
3. If behavior changed, update the statement and symbol reference in the same PR.

