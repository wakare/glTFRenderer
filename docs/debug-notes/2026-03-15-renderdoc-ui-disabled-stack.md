# Debug Note - RenderDoc UI disabled-stack crash

- Date: 2026-03-15
- Scope: `RendererDemo -> DemoBase -> Runtime / Diagnostics > RenderDoc`
- Commit: `(working tree, not committed yet)`
- Companion note:
  - `docs/debug-notes/2026-03-15-renderdoc-ui-disabled-stack_ZH.md`

## Symptom

After the shared `DemoBase` RenderDoc UI was added, clicking `Capture Current Frame` could crash the app inside `ImGui::EndDisabled()`. The observed stack pointed at the RenderDoc section in `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`, not at RenderDoc API calls.

## Reproduction

- Start `RendererDemo` with debug UI enabled.
- Open `Runtime / Diagnostics > RenderDoc`.
- Click `Capture Current Frame`.
- In the failing build, the app could break or crash in `imgui.cpp` at `ImGui::EndDisabled()`.

## Wrong Hypotheses Or Detours

### Detour 1: RenderDoc replay launch or capture completion callback corrupted UI state

- Why it looked plausible:
  The crash only happened after the new RenderDoc UI landed, so it was natural to suspect the new capture or replay-open path first.
- Why it was not the final cause:
  The stack stopped in ImGui's disabled-stack bookkeeping before any replay launch was needed. The crash reproduced on button click, not only after capture finalization.

### Detour 2: The problem came from pending capture polling in `TickFrame`

- Why it looked plausible:
  `PollPendingRenderDocCapture()` mutates status strings and capture state on later frames, so it looked like a possible source of mismatched UI state.
- Why it was not the final cause:
  The failing frame was the click frame itself. The mismatch already existed inside one `DrawDebugUI()` pass before polling had a chance to finish a capture.

## Final Root Cause

The RenderDoc UI used mutable member state directly to decide both `ImGui::BeginDisabled()` and `ImGui::EndDisabled()`. Clicking `Capture Current Frame` changed `m_renderdoc_manual_capture_pending` from `false` to `true` in the middle of the same UI branch. That meant the frame could skip `BeginDisabled()` but still execute `EndDisabled()`, corrupting ImGui's disabled stack and crashing in `ImGui::EndDisabled()`.

## Final Fix

- Capture the disabled conditions into frame-local booleans before the buttons run:
  - `disable_capture_button`
  - `disable_open_last_capture`
- Use those stable booleans for the matching `BeginDisabled()` / `EndDisabled()` pairs.
- Keep the actual state mutations inside `QueueManualRenderDocCapture()` and replay-open helpers, but never let those mutations decide the matching ImGui stack operations for the current frame.
- Separately, harden RenderDoc lifecycle behavior so runtime RHI recreation re-runs the target-backend preload step when RenderDoc startup flags are active. This keeps the shared manual UI aligned with the same opt-in startup contract after DX12/Vulkan switching.

Affected files:

- `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoBase.h`

## Validation

- Build result:
  `RendererDemo` isolated verify build succeeded after the fix.
  Recorded artifact: `.tmp/build_renderdoc_uibase_fix_disabled.stdout.log`
- Runtime validation:
  RenderDoc regression validation passed on DX12 and Vulkan after the fix.
  Recorded artifact: `.tmp/validate_renderdoc_demobase_ui_v2/rv_20260315_162713/summary.json`
- User acceptance:
  The user reproduced the crash, then confirmed that the issue was fixed after the disabled-stack correction.

## Reflection And Prevention

- Any ImGui `Begin*` / `End*` pair that depends on mutable runtime state should snapshot the condition into a local variable first when the button callback can mutate that same state.
- UI code that arms asynchronous work should avoid reading member state twice for stack-structured ImGui APIs in the same branch.
- A useful guardrail is to review every newly added `BeginDisabled()` / `EndDisabled()` pair with the question: "Can the condition change inside the guarded block this frame?" If the answer is yes, convert it to a local snapshot immediately.
