# AGENTS.md

## Build Log Policy (MSBuild)

- Goal: prevent AIChat UI from being flooded/blocked by build output.
- Always redirect build output to files, do not stream full logs to chat.
- Prefer single-node, no node reuse to reduce hang risk: `/m:1 /nr:false`.
- Keep console logging disabled for MSBuild: use `/noconlog`.
- Always generate both text log and binlog for diagnostics: `/fl` + `/bl`.

### Recommended Build Pattern

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

### Reporting Rules

- In chat, only report:
  - final status (`Build succeeded` / `Build FAILED`)
  - warning/error counts
  - key error summaries (file + code + message)
  - log file paths (`.msbuild.log`, `.stdout.log`, `.stderr.log`, `.binlog`)
- Do not paste large raw build logs into chat unless explicitly requested.

### Quiet Execution Notes (Important)

- Use `Start-Process` with `-RedirectStandardOutput/-RedirectStandardError` for long-running commands to avoid flooding AIChat.
- Prefer MSBuild args as an array (`$args = @(...)`), not a single command string.
- Do not use unescaped `;` in inline PowerShell command text (for example `/clp:Summary;ForceNoAlign`), because `;` is a PowerShell statement separator.
- If a `;` is needed inside one MSBuild argument, keep it inside one array element (example: `"/flp:LogFile=...;Verbosity=diagnostic;Encoding=UTF-8"`).

### Build Isolation + Watchdog (Important)

- Do not share `obj/bin` with Rider background builds when running validation builds from AIChat.
- Use isolated outputs:
  - `"/p:BaseOutputPath=<repo>\\.verify\\bin\\"`
  - `"/p:BaseIntermediateOutputPath=<repo>\\.verify\\obj\\"`
- Use a timeout watchdog for long-running builds and kill only the launched build process tree on timeout.
- Preferred script:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Build-RendererDemo-Verify.ps1
```

### Build Completion Detection (Important)

- Do not infer build completion from global `msbuild.exe` process count (Rider may keep background temp-project hosts alive).
- For `scripts/Build-RendererDemo-Verify.ps1`, use the wrapper result as source of truth:
  - process exit code
  - `STATUS=...` line in wrapper stdout log
  - recorded `MSBUILD_PID` + `DURATION_MS`
- Avoid nested `Start-Process` wrappers when launching the verify script from AIChat; run the script directly and redirect its stdout/stderr to files.

### Log Inspection Safety (Important)

- Do not run unbounded `Get-Content <file>` for large logs/files in AIChat.
- Prefer bounded reads: `Get-Content <file> -TotalCount 200` or `Get-Content <file> -Tail 200`.
- Prefer targeted search: `Select-String -Path <file> -Pattern "error|failed|exception|VUID" -CaseSensitive:$false -Context 2,2`.
- For code/text lookup, prefer `rg`/`rg -n` over full file dumping.
- If output may still be large, redirect filtered results to a file and only report summary in chat.
- For `Get-Content`, default to no direct console output:
  - redirect to file first (`... | Out-File .tmp/<name>.log`)
  - then report only counts + top-N sample in chat (avoid raw dumps)
- Avoid direct `Get-Content` output in AIChat unless the file is very small and explicitly requested.

### AIChat Output Throttle (Important)

- Never print high-volume command output directly to chat.
- Rider AIChat becomes slow or can freeze on streamed large output; treat chat as summary-only and default to file redirection for any command that may emit non-trivial output.
- For potentially huge result sets (example: `rg --files`, broad `rg` over repo root), always redirect output to a file first, then report only summary metrics in chat.
- For `rg` usage, default to redirected mode when scope is unclear or large:
  - `rg ... > .tmp/<name>.log`
  - Then only report count + top-N sample (`Select-Object -First 20`) in chat.
- Prefer summary-first commands for discovery:
  - counts (`Measure-Object`, `rg ... | measure`)
  - top-N sampling (`Select-Object -First 20`)
  - scoped searches (limit folders/file globs)
- For long-running commands, use quiet execution with redirected stdout/stderr and share only:
  - status
  - warning/error counts
  - key diagnostics
  - output file paths
- If a command unexpectedly emits too much output, stop, switch to redirected mode, and continue with summarized reporting only.
- For log/file reads, use this safe pattern by default:
  - `Get-Content <file> -Tail 120 | Out-File .tmp/<name>.tail.log`
  - `Select-String -Path <file> -Pattern "<pattern>" | Out-File .tmp/<name>.match.log`
  - only print compact summary in chat.

### Hang / Stuck Triage (MSBuild)

- If Task Manager shows only `MSBuild.exe` at `0% CPU` and no `cl.exe`/`link.exe`, suspect a stuck/orphaned host process.
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

- If needed, terminate orphaned `MSBuild.exe` then rerun quiet build:

```powershell
Get-Process msbuild -ErrorAction SilentlyContinue | Stop-Process -Force
```

### Rendering Root-Cause First (Important)

- For visual artifacts (banding, discontinuity, seams, flicker), prioritize upstream data-source debugging before post-process concealment.
- Do not default to TAA/dither/noise as a fix; if used for temporary diagnosis, remove it after root cause is fixed.
- Use this triage order:
  - find the first pass where the artifact appears (payload/mask/composite/final)
  - verify continuity of key fields in that pass (for frosted glass: `panel_mask`, `panel_rim`, `panel_profile`, `refraction_direction`)
  - inspect derivative- and interpolation-sensitive logic (`fwidth`, piecewise SDF gradients, triangle interpolation seams, depth/normal reconstruction)
  - fix at data-generation stage, then re-validate downstream passes
- Frosted-glass specific rule:
  - avoid applying piecewise SDF gradient direction across the full panel interior; use stable interior basis and edge-weighted SDF influence.
