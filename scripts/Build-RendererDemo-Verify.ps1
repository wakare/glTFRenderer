param(
    [string]$MsBuildPath = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
    [string]$Solution = "glTFRenderer\glTFRenderer.sln",
    [string]$Target = "RendererDemo",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [string]$LogDir = "build_logs",
    [string]$VerifyRoot = ".verify",
    [int]$TimeoutSec = 900,
    [switch]$CleanVerifyOutputs
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-BuildCountFromLog {
    param(
        [string]$LogPath,
        [string]$Pattern
    )

    if (-not (Test-Path -LiteralPath $LogPath)) {
        return 0
    }

    $matches = Select-String -Path $LogPath -Pattern $Pattern -CaseSensitive:$false
    if (-not $matches) {
        return 0
    }

    $lastMatch = $matches[-1].Matches[0].Groups[1].Value
    if ([string]::IsNullOrWhiteSpace($lastMatch)) {
        return 0
    }

    return [int]$lastMatch
}

if (-not (Test-Path -LiteralPath $MsBuildPath)) {
    Write-Host "STATUS=InvalidMSBuildPath"
    Write-Host "MSBUILD=$MsBuildPath"
    exit 2
}

if ($CleanVerifyOutputs -and (Test-Path -LiteralPath $VerifyRoot)) {
    Remove-Item -LiteralPath $VerifyRoot -Recurse -Force
}

New-Item -ItemType Directory -Path $LogDir -Force | Out-Null

$repoRoot = (Get-Location).Path
$verifyRootFull = Join-Path $repoRoot $VerifyRoot
$verifyBin = Join-Path $verifyRootFull "bin"
$verifyObj = Join-Path $verifyRootFull "obj"
New-Item -ItemType Directory -Path $verifyRootFull -Force | Out-Null
New-Item -ItemType Directory -Path $verifyBin -Force | Out-Null
New-Item -ItemType Directory -Path $verifyObj -Force | Out-Null

$verifyBinWithSlash = $verifyBin + "\"
$verifyObjWithSlash = $verifyObj + "\"

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$txt = Join-Path $LogDir "rendererdemo_$stamp.msbuild.log"
$std = Join-Path $LogDir "rendererdemo_$stamp.stdout.log"
$err = Join-Path $LogDir "rendererdemo_$stamp.stderr.log"
$bin = Join-Path $LogDir "rendererdemo_$stamp.binlog"

$args = @(
    $Solution,
    "/t:$Target",
    "/p:Configuration=$Configuration", "/p:Platform=$Platform",
    "/m:1", "/nr:false",
    "/p:UseMultiToolTask=false", "/p:MultiProcessorCompilation=false", "/p:CL_MPCount=1",
    "/p:BaseOutputPath=$verifyBinWithSlash", "/p:BaseIntermediateOutputPath=$verifyObjWithSlash",
    "/v:minimal", "/nologo", "/noconlog",
    "/fl", "/flp:LogFile=$txt;Verbosity=diagnostic;Encoding=UTF-8",
    "/bl:$bin"
)

$process = Start-Process -FilePath $MsBuildPath -ArgumentList $args -NoNewWindow -PassThru `
    -RedirectStandardOutput $std -RedirectStandardError $err

$timedOut = $false
$finished = $process.WaitForExit($TimeoutSec * 1000)
if (-not $finished) {
    $timedOut = $true
    try { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue } catch {}
    Get-CimInstance Win32_Process -Filter "name='MSBuild.exe'" |
        Where-Object { $_.ParentProcessId -eq $process.Id } |
        ForEach-Object {
            Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
        }
    $exitCode = 124
} else {
    $exitCode = [int]$process.ExitCode
}

$warningCount = Get-BuildCountFromLog -LogPath $txt -Pattern '(\d+)\s+Warning\(s\)'
$errorCount = Get-BuildCountFromLog -LogPath $txt -Pattern '(\d+)\s+Error\(s\)'
$buildFailed = Select-String -Path $txt -Pattern 'Build FAILED\.' -CaseSensitive:$false -Quiet
$buildSucceeded = Select-String -Path $txt -Pattern 'Build succeeded\.' -CaseSensitive:$false -Quiet

$status = "BuildUnknown"
if ($timedOut) {
    $status = "BuildTimeout"
} elseif ($exitCode -eq 0 -and $buildSucceeded) {
    $status = "BuildSucceeded"
} elseif ($buildFailed -or $exitCode -ne 0) {
    $status = "BuildFailed"
}
if ($status -eq "BuildFailed" -and $exitCode -eq 0) {
    $exitCode = 1
}

Write-Host "STATUS=$status"
Write-Host "EXITCODE=$exitCode"
Write-Host "WARNINGS=$warningCount"
Write-Host "ERRORS=$errorCount"
Write-Host "TXT=$txt"
Write-Host "STD=$std"
Write-Host "ERR=$err"
Write-Host "BIN=$bin"
Write-Host "OUTDIR=$verifyBinWithSlash"
Write-Host "INTDIR_BASE=$verifyObjWithSlash"

exit $exitCode
