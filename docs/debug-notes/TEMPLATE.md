# Bug Title

- Status: Accepted
- Date: YYYY-MM-DD
- Scope: Component / backend / test case
- Commit: `<hash>`
- Path convention: repo-relative code paths should start with `glTFRenderer/`; runtime-only `Resources/...` or `glTFResources/...` paths are allowed when the executable resolves them from its working directory.

## Symptom

Describe what was visible to the user.

## Reproduction

List the test case, backend, camera state, or snapshot needed to reproduce the bug.

## Wrong Hypotheses Or Detours

### Detour 1

- Why it looked plausible:
- Why it was not the final cause:

### Detour 2

- Why it looked plausible:
- Why it was not the final cause:

## Final Root Cause

Describe the exact failing lifetime, synchronization, math, or state transition issue.

## Final Fix

- List the actual code changes.
- Mention helper tooling or repro improvements separately from the fix if needed.

## Validation

- Build result:
- Runtime validation:
- User acceptance:

## Reflection And Prevention

- What signal should have been prioritized earlier:
- What guardrail or refactor would reduce recurrence:
- What to check first if a similar symptom appears again:
