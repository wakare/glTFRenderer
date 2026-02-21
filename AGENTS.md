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
