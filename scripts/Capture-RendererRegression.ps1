param(
    [Parameter(Mandatory = $true)]
    [string]$Suite,
    [ValidateSet("dx", "vk")]
    [string]$Backend = "dx",
    [string]$Executable = "glTFRenderer\\x64\\Debug\\RendererDemo.exe",
    [string]$WorkingDirectory = "glTFRenderer\\x64\\Debug",
    [string]$DemoApp = "DemoAppModelViewerFrostedGlass",
    [string]$OutputBase = ".tmp\\regression_capture",
    [switch]$RenderDocCapture,
    [switch]$RenderDocRequired,
    [switch]$PIXCapture,
    [switch]$PIXRequired,
    [bool]$DisableDebugUI = $true,
    [bool]$NoAssertDialog = $true,
    [int]$TimeoutSec = 900,
    [string[]]$ExtraArgs = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-RepoPath {
    param(
        [string]$Path,
        [string]$RepoRoot,
        [switch]$RequireExists
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "Path cannot be empty."
    }

    $candidate = if ([System.IO.Path]::IsPathRooted($Path)) {
        $Path
    }
    else {
        Join-Path $RepoRoot $Path
    }

    if ($RequireExists) {
        if (-not (Test-Path -LiteralPath $candidate)) {
            throw "Path not found: $candidate"
        }
        return (Resolve-Path -LiteralPath $candidate).Path
    }

    return $candidate
}

function Stop-ProcessTree {
    param(
        [int]$ParentProcessId
    )

    try {
        Get-CimInstance Win32_Process |
            Where-Object { $_.ParentProcessId -eq $ParentProcessId } |
            ForEach-Object {
                try {
                    Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
                }
                catch {
                }
            }
    }
    catch {
    }

    try {
        Stop-Process -Id $ParentProcessId -Force -ErrorAction SilentlyContinue
    }
    catch {
    }
}

$repoRoot = (Resolve-Path -LiteralPath ".").Path
$exePath = Resolve-RepoPath -Path $Executable -RepoRoot $repoRoot -RequireExists
$workDirPath = Resolve-RepoPath -Path $WorkingDirectory -RepoRoot $repoRoot -RequireExists
$suitePath = Resolve-RepoPath -Path $Suite -RepoRoot $repoRoot -RequireExists
$outputBasePath = Resolve-RepoPath -Path $OutputBase -RepoRoot $repoRoot

New-Item -ItemType Directory -Path $outputBasePath -Force | Out-Null

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$invocationRoot = Join-Path $outputBasePath ("rc_" + $Backend + "_" + $stamp)
New-Item -ItemType Directory -Path $invocationRoot -Force | Out-Null

$stdoutPath = Join-Path $invocationRoot "run.stdout.log"
$stderrPath = Join-Path $invocationRoot "run.stderr.log"

if ($RenderDocCapture -and $PIXCapture) {
    throw "RenderDoc and PIX capture automation cannot be enabled in the same run."
}
if (($PIXCapture -or $PIXRequired) -and $Backend -ne "dx") {
    throw "PIX capture automation is only supported on DX12. Use -Backend dx."
}

$args = New-Object System.Collections.Generic.List[string]
$args.Add($DemoApp)
if ($Backend -eq "dx") {
    $args.Add("-dx")
}
else {
    $args.Add("-vk")
}
if ($DisableDebugUI) {
    $args.Add("-disable-debug-ui")
}
if ($NoAssertDialog) {
    $args.Add("--no-assert-dialog")
}
$args.Add("-regression")
$args.Add("-regression-suite=$suitePath")
$args.Add("-regression-output=$invocationRoot")
if ($RenderDocCapture) {
    $args.Add("-renderdoc-capture")
}
if ($RenderDocRequired) {
    $args.Add("-renderdoc-required")
}
if ($PIXCapture) {
    $args.Add("-pix-capture")
}
if ($PIXRequired) {
    $args.Add("-pix-required")
}
foreach ($extraArg in $ExtraArgs) {
    if (-not [string]::IsNullOrWhiteSpace($extraArg)) {
        $args.Add($extraArg)
    }
}

$process = Start-Process -FilePath $exePath -WorkingDirectory $workDirPath -ArgumentList $args.ToArray() -NoNewWindow -PassThru `
    -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$finished = $process.WaitForExit($TimeoutSec * 1000)
$stopwatch.Stop()

$timedOut = $false
if (-not $finished) {
    $timedOut = $true
    Stop-ProcessTree -ParentProcessId $process.Id
}

$exitCode = if ($timedOut) { 124 } else { [int]$process.ExitCode }
$summaryPath = ""
$runDirPath = ""
$caseCount = 0
$resultCount = 0
$failedCount = 0
$suiteSuccess = $false
$renderDocEnabled = $false
$renderDocAvailable = $false
$renderDocSuccessCount = 0
$renderDocRetainedCount = 0
$rdcCount = 0
$pixEnabled = $false
$pixAvailable = $false
$pixSuccessCount = 0
$pixRetainedCount = 0
$wpixCount = 0

$summaryCandidates = @(
    Get-ChildItem -LiteralPath $invocationRoot -Recurse -Filter suite_result.json -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime
)
if ($summaryCandidates.Count -gt 0) {
    $summaryPath = $summaryCandidates[-1].FullName
    $runDirPath = Split-Path -Parent $summaryPath
    $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    if ($null -ne $summary.case_count) {
        $caseCount = [int]$summary.case_count
    }
    if ($null -ne $summary.result_count) {
        $resultCount = [int]$summary.result_count
    }
    if ($null -ne $summary.failed_count) {
        $failedCount = [int]$summary.failed_count
    }
    if ($null -ne $summary.success) {
        $suiteSuccess = [bool]$summary.success
    }
    else {
        $suiteSuccess = ($failedCount -eq 0) -and ($caseCount -eq $resultCount)
    }
    if ($null -ne $summary.renderdoc_capture_enabled) {
        $renderDocEnabled = [bool]$summary.renderdoc_capture_enabled
    }
    if ($null -ne $summary.renderdoc_capture_available) {
        $renderDocAvailable = [bool]$summary.renderdoc_capture_available
    }
    if ($null -ne $summary.renderdoc_capture_success_count) {
        $renderDocSuccessCount = [int]$summary.renderdoc_capture_success_count
    }
    if ($null -ne $summary.renderdoc_capture_retained_count) {
        $renderDocRetainedCount = [int]$summary.renderdoc_capture_retained_count
    }
    $rdcCount = @(
        Get-ChildItem -LiteralPath $runDirPath -Recurse -Filter *.rdc -File -ErrorAction SilentlyContinue
    ).Count
    if ($renderDocRetainedCount -eq 0 -and $rdcCount -gt 0) {
        $renderDocRetainedCount = $rdcCount
    }
    if ($null -ne $summary.pix_capture_enabled) {
        $pixEnabled = [bool]$summary.pix_capture_enabled
    }
    if ($null -ne $summary.pix_capture_available) {
        $pixAvailable = [bool]$summary.pix_capture_available
    }
    if ($null -ne $summary.pix_capture_success_count) {
        $pixSuccessCount = [int]$summary.pix_capture_success_count
    }
    if ($null -ne $summary.pix_capture_retained_count) {
        $pixRetainedCount = [int]$summary.pix_capture_retained_count
    }
    $wpixCount = @(
        Get-ChildItem -LiteralPath $runDirPath -Recurse -Filter *.wpix -File -ErrorAction SilentlyContinue
    ).Count
    if ($pixRetainedCount -eq 0 -and $wpixCount -gt 0) {
        $pixRetainedCount = $wpixCount
    }
}

$status = "RunUnknown"
if ($timedOut) {
    $status = "RunTimeout"
}
elseif ($exitCode -eq 0 -and -not [string]::IsNullOrWhiteSpace($summaryPath) -and $suiteSuccess) {
    $status = "RunSucceeded"
}
elseif ($exitCode -eq 0 -and -not [string]::IsNullOrWhiteSpace($summaryPath)) {
    $status = "RunCompletedWithFailures"
}
elseif ($exitCode -ne 0) {
    $status = "RunFailed"
}

Write-Host "STATUS=$status"
Write-Host "EXITCODE=$exitCode"
Write-Host "BACKEND=$Backend"
Write-Host "DEMO_APP=$DemoApp"
Write-Host "SUITE=$suitePath"
Write-Host "OUTPUT_ROOT=$invocationRoot"
Write-Host "RUN_DIR=$runDirPath"
Write-Host "SUMMARY=$summaryPath"
Write-Host "CASES=$caseCount"
Write-Host "RESULTS=$resultCount"
Write-Host "FAILED=$failedCount"
Write-Host "SUITE_SUCCESS=$suiteSuccess"
Write-Host "RENDERDOC_ENABLED=$renderDocEnabled"
Write-Host "RENDERDOC_AVAILABLE=$renderDocAvailable"
Write-Host "RENDERDOC_SUCCESS_COUNT=$renderDocSuccessCount"
Write-Host "RENDERDOC_RETAINED_COUNT=$renderDocRetainedCount"
Write-Host "RDC_COUNT=$rdcCount"
Write-Host "PIX_ENABLED=$pixEnabled"
Write-Host "PIX_AVAILABLE=$pixAvailable"
Write-Host "PIX_SUCCESS_COUNT=$pixSuccessCount"
Write-Host "PIX_RETAINED_COUNT=$pixRetainedCount"
Write-Host "WPIX_COUNT=$wpixCount"
Write-Host "STDOUT=$stdoutPath"
Write-Host "STDERR=$stderrPath"
Write-Host "DURATION_MS=$($stopwatch.ElapsedMilliseconds)"

if ($status -eq "RunSucceeded") {
    exit 0
}
if ($status -eq "RunCompletedWithFailures") {
    exit 1
}
if ($timedOut) {
    exit 124
}
exit $exitCode
