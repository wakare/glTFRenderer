# Debug Note - directional light zero-speed flicker

- Status: Accepted
- Date: 2026-03-22
- Scope: `RendererDemo -> DemoAppModelViewer / RendererModuleLighting / frame-buffered light constants`
- Commit: `(this commit)`
- Companion note:
  - `docs/debug-notes/2026-03-22-directional-light-zero-speed-flicker_ZH.md`

## Symptom

In `DemoAppModelViewer`, setting `Directional Light Speed` to `0` could produce occasional flicker. Setting the same control to a very small non-zero value made the flicker disappear.

## Reproduction

- Start `RendererDemo`.
- Open `DemoAppModelViewer`.
- Leave the camera fixed enough that lighting changes are easy to notice.
- Set `Directional Light Speed` to `0.0`.
- Observe that lighting can flicker intermittently instead of remaining fully stable.
- Set the same control to a small non-zero value such as `0.001` to `0.01` rad/s.
- Observe that the intermittent flicker disappears even though the light appears visually almost static.

## Wrong Hypotheses Or Detours

### Detour 1: zero-speed rotation math or the direction epsilon gate is numerically unstable

- Why it looked plausible:
  `DemoAppModelViewer` has an explicit branch difference between `speed == 0` and `speed != 0`, and only calls `UpdateLight()` when the direction changes beyond an epsilon threshold.
- Why it was not the final cause:
  The actual direction math is stable. Once the light stops changing, the real problem is that some frame-buffered GPU copies keep older light data while others contain the latest value.

### Detour 2: the flicker must primarily come from directional shadow matrix updates

- Why it looked plausible:
  The artifact reads like a shadow instability, and `RendererSystemLighting` does have per-frame shadow view-buffer updates.
- Why it was not the final cause:
  The underlying mismatch already exists in the main light-info buffers. Shadow updates can make the symptom easier to notice, but they are not required to trigger the stale-state alternation.

## Final Root Cause

`RendererModuleLighting` stores light data in frame-buffered buffers, but `UploadAllLightInfos()` previously used `UploadFrameBufferedBufferData()`, which only uploads to the current frame slot. After that single upload, the module cleared `m_need_upload_light_infos`.

That means a light change could refresh only one slot while other in-flight slots kept older light data. The lighting pass binds the current frame slot every frame, so when frame slots rotate, the renderer can alternate between updated and stale directional-light constants. The bug becomes most visible when `Directional Light Speed` is `0`, because the light soon stops changing and no further uploads arrive to backfill the remaining frame slots. A tiny non-zero speed keeps marking the light dirty often enough to gradually refresh all slots, which hides the bug.

## Final Fix

- Updated `glTFRenderer/RendererDemo/RendererModule/RendererModuleLighting.cpp`.
- Changed `UploadAllLightInfos()` so that dirty light-info uploads write to every handle in:
  - `m_light_buffer_handles`
  - `m_light_count_buffer_handles`
- Kept the existing dirty-flag behavior, but made the synchronization scope match the frame-buffered binding model used by the lighting pass.

## Validation

- Build result:
  `RendererDemo` verification build succeeded with `0` errors and `54` warnings.
  Logs:
  - `build_logs/rendererdemo_20260322_193011.msbuild.log`
  - `build_logs/rendererdemo_20260322_193011.stdout.log`
  - `build_logs/rendererdemo_20260322_193011.stderr.log`
  - `build_logs/rendererdemo_20260322_193011.binlog`
- Runtime validation:
  No new automated visual capture was run in this turn. Validation here is based on the repro-path inspection, the confirmed upload/binding mismatch, and the successful verification build.
- User acceptance:
  After reviewing the diagnosis and patch, the user said the issue looked fixed and asked for the change to be committed.

## Reflection And Prevention

- What signal should have been prioritized earlier:
  The strongest clue was "speed = 0 flickers, tiny non-zero speed does not". That pattern points more strongly to stale per-frame state refresh than to pure shading math or random numerical instability.
- What guardrail or refactor would reduce recurrence:
  Frame-buffered data producers should have an explicit helper for "upload to all slots", instead of relying on a current-slot upload helper for state that may become static immediately after a change.
- What to check first if a similar symptom appears again:
  Compare the producer upload scope against the consumer binding scope. If a pass binds per-frame resources but the producer only updates the current slot, expect alternating old/new state across in-flight frames.
