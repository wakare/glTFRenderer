# RendererDemo Manual Frame-Capture Usage

Companion references:

- Chinese companion: `docs/RendererDemo_FrameCapture_Usage.md`
- Related background: `docs/FeatureNotes/20260315_B9_RenderDocFrameCapturePlan.md`

## 1. Goal and Scope

This document describes the shared manual frame-capture entry points on the maintained `RendererDemo` path.

Current coverage:

- shared single-frame RenderDoc capture through `DemoBase`
- shared single-frame PIX capture through `DemoBase`
- `DemoAppModelViewerFrostedGlass` regression entry points for automatic RenderDoc / PIX artifacts
- startup flags, UI workflow, default output locations, runtime limits, and regression entry points

Out of scope:

- the legacy `glTFApp` runtime path
- implementation details of the regression compare / reporting pipeline
- installation guides for the external tools themselves

## 2. Capability Matrix

| Tool | Backend | Default artifact | Entry | Notes |
| --- | --- | --- | --- | --- |
| RenderDoc | DX12 / Vulkan | `.rdc` | `Runtime / Diagnostics > RenderDoc` | Can reopen the last successful capture. |
| PIX | DX12 | `.wpix` | `Runtime / Diagnostics > PIX` | Currently DX12 only. |

Shared constraints:

- both paths are request-driven one-frame captures, not continuous recording
- the capture runtime must be preloaded before device initialization
- RenderDoc and PIX cannot arm capture for the same frame at the same time

## 3. Launch Contract

### 3.1 Demo-name argument

`RendererDemo.exe` expects the first positional argument to be the demo command name.

Current built-in demo commands:

- `DemoTriangleApp`
- `DemoAppModelViewer`
- `DemoAppModelViewerFrostedGlass`

If `-dx12` or `-vulkan` is placed first, the program treats it as the demo name and reports `Unknown demo`.

### 3.2 Working directory

Prefer launching from the output directory, or set the IDE working directory to `$(OutDir)`.

Recommended repo-root workflow:

```powershell
Set-Location .\glTFRenderer\x64\Debug
```

### 3.3 Backend arguments

Common backend switches:

- DX12: `-dx` or `-dx12`
- Vulkan: `-vk` or `-vulkan`

### 3.4 Capture startup flags

The shared `DemoBase` manual-capture UI currently supports:

- `-renderdoc-ui`
  - preload RenderDoc before device creation so the shared manual UI can use it
- `-renderdoc-capture`
  - for the shared manual UI, this is effectively the same preload/enable contract for the run
  - by itself it does not automatically emit an `.rdc` at process startup
- `-renderdoc-required`
  - fail initialization when the RenderDoc API is unavailable
- `-pix-ui`
  - preload the PIX GPU capturer before device creation so the shared manual UI can use it
- `-pix-capture`
  - for the shared manual UI, this is effectively the same preload/enable contract for the run
  - by itself it does not automatically emit a `.wpix` at process startup
- `-pix-required`
  - fail initialization when the PIX capture API is unavailable

Notes:

- demo-specific regression paths may layer extra meaning onto `-renderdoc-capture` and `-pix-capture`; the shared `DemoBase` manual UI still treats them as preload / enable flags
- PIX flags are currently meaningful only on DX12

## 4. Recommended Command Templates

### 4.1 DX12 + PIX

```powershell
Set-Location .\glTFRenderer\x64\Debug
.\RendererDemo.exe DemoTriangleApp -dx12 -pix-ui
```

If the run should fail when PIX is unavailable:

```powershell
Set-Location .\glTFRenderer\x64\Debug
.\RendererDemo.exe DemoTriangleApp -dx12 -pix-required
```

### 4.2 DX12 / Vulkan + RenderDoc

```powershell
Set-Location .\glTFRenderer\x64\Debug
.\RendererDemo.exe DemoTriangleApp -dx12 -renderdoc-ui
```

```powershell
Set-Location .\glTFRenderer\x64\Debug
.\RendererDemo.exe DemoTriangleApp -vulkan -renderdoc-ui
```

### 4.3 DX12 + PIX regression automation (`DemoAppModelViewerFrostedGlass` only)

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Capture-RendererRegression.ps1 `
  -Suite glTFRenderer\RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json `
  -Backend dx `
  -DemoApp DemoAppModelViewerFrostedGlass `
  -PIXCapture
```

If PIX availability must be treated as required for the run, add `-PIXRequired`.

Relevant suite JSON fields:

- global defaults:
  - `default_capture_pix`
  - `default_pix_capture_frame_offset`
  - `default_keep_pix_on_success`
- per-case `capture` block:
  - `capture_pix`
  - `pix_capture_frame_offset`
  - `keep_pix_on_success`
- compatibility fallback: the parser also accepts the same three keys directly on the case root

Constraints for this regression path:

- DX12 only
- only `DemoAppModelViewerFrostedGlass` currently emits PIX regression artifacts
- RenderDoc and PIX automation are mutually exclusive per regression run
- on the shared `DemoBase` manual UI path, `-pix-capture` still means preload / enable, not automatic startup capture

## 5. Manual Workflow

1. Launch `RendererDemo.exe` with the target backend and capture startup flag.
2. Open `Runtime / Diagnostics`.
3. Expand the target tool section:
   - `RenderDoc`
   - `PIX`
4. Adjust as needed:
   - `Capture Name`
   - `Capture Dir`
   - auto-open UI option
5. Click `Capture Current Frame`.
6. Inspect:
   - `Pending Capture Frame`
   - `Last Capture`
   - `Status`
7. Reopen the last successful artifact through:
   - `Open Last Capture In Replay UI`
   - `Open Last Capture In PIX`

RenderDoc replay launch note:

- When replay is opened from the shared `DemoBase` UI through `Open Last Capture In Replay UI`, or through RenderDoc auto-open after capture, the runtime now applies these environment variables for that launch:
  - `DISABLE_LAYER_NV_PRESENT_1=1`
  - `DISABLE_LAYER_NV_OPTIMUS_1=1`
- This mitigates `qrenderdoc.exe` crashes on some NVIDIA machines where Vulkan replay layers conflict with RenderDoc replay startup.
- This automatic mitigation only covers RenderDoc replay launches triggered from the in-app UI. If `qrenderdoc.exe` is started directly from Explorer, command-line shells, or other external tools, the same environment variables still need to be set manually.

## 6. Default Output and Status Reading

Default output directories are resolved relative to the current working directory.

When the working directory is `glTFRenderer\x64\Debug`, the default outputs are:

- RenderDoc: `build_logs/renderdoc/*.rdc`
- PIX: `build_logs/pix/*.wpix`

Meaning of the common UI status fields:

- `Capture Enabled`
  - whether this run has enabled the capture path
- `Capture Available`
  - whether the current process actually obtained the corresponding capture API
- `Startup Status`
  - result of the preload phase, including failure reason when relevant
- `Pending Capture Frame`
  - the target frame index that has been armed but not finalized yet
- `Last Capture`
  - the output path of the most recent successful capture
- `Status`
  - result text for the latest request, completion, or tool-UI open action

## 7. Code Touchpoints and Maintenance Boundary

The shared manual frame-capture path currently lives mainly in:

- `glTFRenderer/RendererCore/Public/RendererInterface.h`
- `glTFRenderer/RendererCore/Private/RendererInterface.cpp`
- `glTFRenderer/RendererDemo/DemoApps/DemoBase.h`
- `glTFRenderer/RendererDemo/DemoApps/DemoBase.cpp`

Project and dependency wiring currently touches:

- `glTFRenderer/RendererCore/RendererCore.vcxproj`
- `glTFRenderer/RendererCore/packages.config`
- `glTFRenderer/RendererDemo/RendererDemo.vcxproj`
- `glTFRenderer/RendererDemo/packages.config`
- `glTFRenderer/glTFRenderer/glTFRenderer.vcxproj`
- `glTFRenderer/glTFRenderer/packages.config`

Constraints that should remain intact:

- preload must happen before target-backend device initialization
- capture start/end must stay aligned with the shared frame lifecycle, not with transient UI state alone
- runtime recreate or shutdown must end capture before GPU-backed helpers are torn down
- RenderDoc and PIX must not be allowed to overlap in armed/capturing states

## 8. Current Limits

- PIX supports DX12 only; there is no Vulkan PIX capture path today
- the shared `DemoBase` UI supports one-frame manual capture only
- PIX regression automation is currently implemented only for `DemoAppModelViewerFrostedGlass`, not as a shared `DemoBase` feature
- RenderDoc and PIX automation are mutually exclusive per regression run
- in the shared manual UI path, `-pix-capture` and `-renderdoc-capture` mean preload/enable, not automatic startup capture
- the NVIDIA-layer mitigation for RenderDoc replay currently covers only the shared `DemoBase` UI replay launch path, not externally launched `qrenderdoc.exe`

## 9. Current Validation Status

As of `2026-03-23`, this path has the following confirmed status:

- the `RendererDemo` build path succeeds and the PIX project dependencies are wired correctly
- the shared `DemoBase` manual PIX UI is connected to the DX12 runtime path
- manual PIX capture on the current workstation has been confirmed by the user to be functional
- `Capture-RendererRegression.ps1` and `Validate-RendererRegression.ps1` accept PIX automation flags for DX12 `DemoAppModelViewerFrostedGlass` regression runs

That makes the shared manual capture path usable as the day-to-day GPU debugging entry point for `RendererDemo`, while the frosted-glass regression path now also has a PIX automation entry point under DX12.
