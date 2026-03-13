# AGENTS.md

Build/Command Output Rules
- Never stream long command output into AI Chat.
- For MSBuild, always use `/bl:build.binlog` and `/v:minimal`.
- Prefer `/m:1` and `/nr:false` when building under agent.
- If output is needed, print only a short summary (<=100 lines).

Shell Usage
- Do not use `Get-Content` to read code files.
- For code and text lookup, prefer `rg` and `rg -n`.
- For large logs or text files, prefer bounded reads or targeted filtering instead of full dumps.

Repository Root Convention
- Canonical workspace root for this repository is the current directory, currently `D:\Work\DevSpace\glTFRenderer\glTFRenderer`.
- Treat `glTFRenderer/` as the solution and runtime subtree, not as an alternative workspace root.
- Use repo-root-relative paths in docs and commands:
  - solution: `glTFRenderer\glTFRenderer.sln`
  - root utility scripts: `.\scripts\...`
  - RendererDemo scripts: `.\glTFRenderer\scripts\...`
  - runtime source trees: `glTFRenderer/RendererCore`, `glTFRenderer/RHICore`, `glTFRenderer/RendererDemo`
- Runtime-only relative paths such as `Resources/...` or `glTFResources/...` are exceptions when the executable resolves them from its working directory.

Maintenance Scope
- For `RendererDemo` runtime, treat `glTFRenderer/RendererCore` as the maintained frame-lifecycle path.
- For `RendererDemo` runtime issues, use `glTFRenderer/RendererCore` as the source of truth for frame lifecycle and resource management.
- Do not investigate or modify `glTFApp`-based runtime logic unless the user explicitly says the issue reproduces there or asks for legacy-path changes.
- For Vulkan runtime teardown or demo or RHI switching, treat runtime services such as ImGui Vulkan backend state, timestamp-profiler query pools, and similar device-backed helpers as GPU-referenced resources. Wait for frame completion and queue or device idle before shutting those services down; destroying them first can surface later as `vkQueueWaitIdle` or `vkDeviceWaitIdle` failures during cleanup.
- When a Vulkan recreate or switching issue moves from `vkCreateDevice` failures to `vkQueueWaitIdle` or `vkDeviceWaitIdle` failures, inspect teardown ordering and old-runtime object lifetime before changing requested capabilities.

Behavior Preservation
- Do not keep or ship a workaround that disables, narrows, or bypasses legal application-layer behavior just to avoid a bug.
- Capability narrowing may be used only as a short-lived diagnostic step to isolate root cause, and must be reverted before final delivery unless the user explicitly approves a behavioral change.
- For valid runtime requests, including legal Vulkan feature use and device recreation paths, treat the behavior as supported and fix teardown, state management, or root cause instead of suppressing the request.

Rendering Root-Cause First
- For visual artifacts such as banding, discontinuity, seams, or flicker, prioritize upstream data-source debugging before post-process concealment.
- Do not default to TAA, dither, or noise as a fix; if used for temporary diagnosis, remove it after root cause is fixed.
- Use this artifact triage order:
  - find the first pass where the artifact appears: payload, mask, composite, or final
  - verify continuity of key fields in that pass; for frosted glass: `panel_mask`, `panel_rim`, `panel_profile`, `refraction_direction`
  - inspect derivative- and interpolation-sensitive logic such as `fwidth`, piecewise SDF gradients, triangle interpolation seams, and depth or normal reconstruction
  - fix at the data-generation stage, then re-validate downstream passes
- Frosted-glass specific rule:
  - avoid applying piecewise SDF gradient direction across the full panel interior; use a stable interior basis and edge-weighted SDF influence

Bug Fix Notes
- For any bug that the user explicitly accepts as fixed, add a note under `docs/debug-notes/` in the same turn unless the user asks not to.
- Do not write a final root-cause note for bugs that are still under investigation or not yet accepted.
- Use file name format `YYYY-MM-DD-short-bug-name.md`.
- Every accepted bug note must include:
  - Symptom
  - Reproduction
  - Wrong hypotheses or detours
  - Final root cause
  - Final fix
  - Validation
  - Reflection and prevention
- If the investigation started in the wrong direction, keep that history in the note and explain why it looked plausible and why it was not the real cause.
- When a fix exposes a structural weakness, add a prevention item and note whether a refactor or validation hook should be considered.

Build Log Policy (MSBuild)
- Goal: prevent AI Chat UI from being flooded or blocked by build output.
- Always redirect build output to files; do not stream full logs to chat.
- Prefer single-node and no node reuse to reduce hang risk: `/m:1 /nr:false`.
- For long validation builds, keep console logging disabled and emit redirected logs: use `/noconlog`, `/fl`, and `/bl`.

Recommended Build Pattern
```powershell
$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
$logDir = "build_logs"; New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"

$txt = Join-Path $logDir "rendererdemo_$stamp.msbuild.log"
$std = Join-Path $logDir "rendererdemo_$stamp.stdout.log"
$err = Join-Path $logDir "rendererdemo_$stamp.stderr.log"
$bin = Join-Path $logDir "rendererdemo_$stamp.binlog"

$args = @(
  "glTFRenderer\glTFRenderer.sln",
  "/t:RendererDemo",
  "/p:Configuration=Debug", "/p:Platform=x64",
  "/m:1", "/nr:false",
  "/p:UseMultiToolTask=false", "/p:MultiProcessorCompilation=false", "/p:CL_MPCount=1",
  "/v:minimal", "/nologo", "/noconlog",
  "/fl", "/flp:LogFile=$txt;Verbosity=diagnostic;Encoding=UTF-8",
  "/bl:$bin"
)

Start-Process -FilePath $msbuild -ArgumentList $args -NoNewWindow -Wait `
  -RedirectStandardOutput $std -RedirectStandardError $err
```

Reporting Rules
- In chat, only report:
  - final status such as `Build succeeded` or `Build FAILED`
  - warning and error counts
  - key error summaries with file, code, and message
  - log file paths for `.msbuild.log`, `.stdout.log`, `.stderr.log`, and `.binlog`
- Do not paste large raw build logs into chat unless explicitly requested.

Quiet Execution Notes
- Use `Start-Process` with `-RedirectStandardOutput` and `-RedirectStandardError` for long-running commands to avoid flooding AI Chat.
- Prefer MSBuild args as an array, not a single command string.
- Do not use unescaped `;` in inline PowerShell command text, because `;` is a PowerShell statement separator.
- If a `;` is needed inside one MSBuild argument, keep it inside one array element.

Build Isolation and Watchdog
- Do not share `obj` or `bin` with Rider background builds when running validation builds from AI Chat.
- Use isolated outputs:
  - `"/p:BaseOutputPath=<repo>\\.verify\\bin\\"`
  - `"/p:BaseIntermediateOutputPath=<repo>\\.verify\\obj\\"`
- Use a timeout watchdog for long-running builds and kill only the launched build process tree on timeout.
- Preferred script:
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Build-RendererDemo-Verify.ps1
```

Build Completion Detection
- Do not infer build completion from global `msbuild.exe` process count; Rider may keep background temp-project hosts alive.
- For `scripts/Build-RendererDemo-Verify.ps1`, use the wrapper result as source of truth:
  - process exit code
  - `STATUS=...` line in wrapper stdout log
  - recorded `MSBUILD_PID` and `DURATION_MS`
- Avoid nested `Start-Process` wrappers when launching the verify script from AI Chat; run the script directly and redirect its stdout and stderr to files.

Log Inspection Safety
- Do not run unbounded `Get-Content <file>` for large logs or files in AI Chat.
- Prefer bounded reads such as `Get-Content <file> -TotalCount 200` or `Get-Content <file> -Tail 200`.
- Prefer targeted search such as `Select-String -Path <file> -Pattern "error|failed|exception|VUID" -CaseSensitive:$false -Context 2,2`.
- For code and text lookup, prefer `rg` and `rg -n` over full file dumping.
- If output may still be large, redirect filtered results to a file and only report a summary in chat.
- Avoid direct `Get-Content` output in AI Chat unless the file is very small and explicitly requested.

AI Chat Output Throttle
- Never print high-volume command output directly to chat.
- Treat chat as summary-only and default to file redirection for any command that may emit non-trivial output.
- Do not run potentially noisy commands in direct or streaming mode, including `msbuild`, broad `rg`, unbounded `Get-Content`, or test runners.
- For potentially huge result sets such as `rg --files`, broad `rg`, or large logs, always redirect output to a file first, then report only summary metrics and top-N samples in chat.

Hang and Stuck Triage (MSBuild)
- If Task Manager shows only `MSBuild.exe` at `0% CPU` and no `cl.exe` or `link.exe`, suspect a stuck or orphaned host process.
- Check active processes:
```powershell
Get-Process msbuild,cl,link -ErrorAction SilentlyContinue |
  Select-Object Name,Id,CPU,StartTime
```
- Check command line for temp-project host process:
```powershell
Get-CimInstance Win32_Process -Filter "name='MSBuild.exe'" |
  Select-Object ProcessId,ParentProcessId,CreationDate,CommandLine
```
- If needed, terminate orphaned `MSBuild.exe` then rerun a quiet build:
```powershell
Get-Process msbuild -ErrorAction SilentlyContinue | Stop-Process -Force
```
