# Debug Notes

Companion references:

- Chinese companion:
  - `docs/debug-notes/README_ZH.md`

This directory stores accepted bug-fix notes for issues that took meaningful time to diagnose or are likely to repeat.

## Repo Root Convention

- Repo-relative paths in notes are based on the current repo root `C:\glTFRenderer`.
- Source code paths should therefore start with `glTFRenderer/`.
- Runtime-only working-directory-sensitive paths such as `Resources/...` or `glTFResources/...` can stay unprefixed when the note is describing executable launch behavior.

## When to add a note

- Add a note after the user explicitly accepts the fix.
- Add a note for bugs that are backend-specific, timing-sensitive, cross-frame, or otherwise easy to misdiagnose.
- If a bug is still under investigation, keep it out of this folder until the fix is accepted.
- For newly added notes, add a `_ZH.md` or `_EN.md` companion beside the primary note when the note is project-owned and expected to remain active as a reference.

## File naming

- `YYYY-MM-DD-short-bug-name.md`

## Required sections

- Symptom
- Reproduction
- Wrong hypotheses or detours
- Final root cause
- Final fix
- Validation
- Reflection and prevention

## Current notes

- [2026-03-11-dx12-shadow-lifetime-review.md](./2026-03-11-dx12-shadow-lifetime-review.md)
- [2026-03-11-vulkan-shadow-ghosting.md](./2026-03-11-vulkan-shadow-ghosting.md)
- [2026-03-15-renderdoc-ui-disabled-stack.md](./2026-03-15-renderdoc-ui-disabled-stack.md)
- [2026-03-23-renderdoc-vulkan-replay-nv-present.md](./2026-03-23-renderdoc-vulkan-replay-nv-present.md)
