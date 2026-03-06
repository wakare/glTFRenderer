param(
    [string]$SolutionPath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'glTFRenderer.sln'),
    [string]$Target = 'RendererDemo',
    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64',
    [string]$VerifyRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) '.verify'),
    [string]$LogDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build_logs'),
    [int]$TimeoutSec = 900
)

$ErrorActionPreference = 'Stop'

function Find-MSBuild {
    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    $command = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    throw 'MSBuild.exe not found.'
}

function ConvertTo-ArgumentString {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $quoted = foreach ($argument in $Arguments) {
        if ($argument -notmatch '[\s"]') {
            $argument
            continue
        }

        '"' + ($argument -replace '"', '\"') + '"'
    }

    return ($quoted -join ' ')
}

function Invoke-ExternalProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,
        [Parameter(Mandatory = $true)]
        [string]$StdoutPath,
        [Parameter(Mandatory = $true)]
        [string]$StderrPath,
        [Parameter(Mandatory = $true)]
        [int]$TimeoutSeconds
    )

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    $psi.Arguments = ConvertTo-ArgumentString -Arguments $Arguments
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    $null = $process.Start()

    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()

    $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
    if ($timedOut) {
        try {
            taskkill /PID $process.Id /T /F | Out-Null
        }
        catch {
        }
        $process.WaitForExit()
    }

    $stdoutTask.Wait()
    $stderrTask.Wait()

    [System.IO.File]::WriteAllText($StdoutPath, $stdoutTask.Result)
    [System.IO.File]::WriteAllText($StderrPath, $stderrTask.Result)

    return [ordered]@{
        process = $process
        timed_out = $timedOut
        exit_code = if ($timedOut) { $null } else { $process.ExitCode }
    }
}

function Find-RendererDemoExe {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SearchRoot
    )

    if (-not (Test-Path $SearchRoot)) {
        return $null
    }

    $candidate = Get-ChildItem -Path $SearchRoot -Recurse -Filter RendererDemo.exe -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($candidate) {
        return $candidate.FullName
    }
    return $null
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedSolutionPath = (Resolve-Path $SolutionPath).Path
$resolvedVerifyRoot = [System.IO.Path]::GetFullPath($VerifyRoot)
$resolvedLogDir = [System.IO.Path]::GetFullPath($LogDir)
$verifyBin = Join-Path $resolvedVerifyRoot 'bin'
$verifyObj = Join-Path $resolvedVerifyRoot 'obj'

New-Item -ItemType Directory -Path $resolvedLogDir -Force | Out-Null
New-Item -ItemType Directory -Path $verifyBin -Force | Out-Null
New-Item -ItemType Directory -Path $verifyObj -Force | Out-Null

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$txtLog = Join-Path $resolvedLogDir "rendererdemo_$stamp.msbuild.log"
$stdoutLog = Join-Path $resolvedLogDir "rendererdemo_$stamp.stdout.log"
$stderrLog = Join-Path $resolvedLogDir "rendererdemo_$stamp.stderr.log"
$binLog = Join-Path $resolvedLogDir "rendererdemo_$stamp.binlog"
$resultJson = Join-Path $resolvedLogDir "rendererdemo_$stamp.result.json"

$msbuild = Find-MSBuild
$args = @(
    $resolvedSolutionPath,
    "/t:$Target",
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    "/p:BaseOutputPath=$verifyBin\",
    "/p:BaseIntermediateOutputPath=$verifyObj\",
    '/m:1',
    '/nr:false',
    '/p:UseMultiToolTask=false',
    '/p:MultiProcessorCompilation=false',
    '/p:CL_MPCount=1',
    '/v:minimal',
    '/nologo',
    '/noconlog',
    '/fl',
    "/flp:LogFile=$txtLog;Verbosity=diagnostic;Encoding=UTF-8",
    "/bl:$binLog"
)

$startTime = Get-Date
$process = $null
$status = 'FAILED'
$exitCode = -1
$durationMs = 0

try {
    $runResult = Invoke-ExternalProcess `
        -FilePath $msbuild `
        -Arguments $args `
        -WorkingDirectory $repoRoot `
        -StdoutPath $stdoutLog `
        -StderrPath $stderrLog `
        -TimeoutSeconds $TimeoutSec
    $process = $runResult.process

    if ($runResult.timed_out) {
        $status = 'TIMEOUT'
    }
    else {
        $exitCode = $runResult.exit_code
        $status = if ($exitCode -eq 0) { 'SUCCESS' } else { 'FAILED' }
    }
}
catch {
    $status = 'FAILED'
    $exitCode = -1
    Add-Content -Path $stderrLog -Value $_.Exception.ToString()
}
finally {
    $durationMs = [int][Math]::Round(((Get-Date) - $startTime).TotalMilliseconds)
}

$outputExe = $null
if ($status -eq 'SUCCESS') {
    $outputExe = Find-RendererDemoExe -SearchRoot $verifyBin
    if (-not $outputExe) {
        $outputExe = Find-RendererDemoExe -SearchRoot (Join-Path $repoRoot 'x64')
    }
}

$manifest = [ordered]@{
    status = $status
    exit_code = $exitCode
    duration_ms = $durationMs
    msbuild_pid = if ($process) { $process.Id } else { $null }
    msbuild = $msbuild
    solution_path = $resolvedSolutionPath
    target = $Target
    configuration = $Configuration
    platform = $Platform
    verify_root = $resolvedVerifyRoot
    output_exe = $outputExe
    txt_log = $txtLog
    stdout_log = $stdoutLog
    stderr_log = $stderrLog
    binlog = $binLog
    result_json = $resultJson
}

$manifest | ConvertTo-Json -Depth 4 | Out-File -FilePath $resultJson -Encoding utf8

Write-Output "STATUS=$status"
Write-Output "EXIT_CODE=$exitCode"
Write-Output "MSBUILD_PID=$($manifest.msbuild_pid)"
Write-Output "DURATION_MS=$durationMs"
Write-Output "MSBUILD=$msbuild"
Write-Output "OUTPUT_EXE=$outputExe"
Write-Output "TXT_LOG=$txtLog"
Write-Output "STDOUT_LOG=$stdoutLog"
Write-Output "STDERR_LOG=$stderrLog"
Write-Output "BINLOG=$binLog"
Write-Output "RESULT_JSON=$resultJson"

if ($status -ne 'SUCCESS') {
    exit 1
}
