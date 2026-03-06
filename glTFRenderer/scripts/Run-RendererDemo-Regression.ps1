param(
    [ValidateSet('DX12', 'Vulkan')]
    [string]$Api,
    [string]$SuitePath,
    [string]$DemoName = 'DemoAppModelViewer',
    [ValidateSet('Mailbox', 'VSync')]
    [string]$PresentMode = 'Mailbox',
    [string]$ExePath = '',
    [string]$OutputRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build_logs\regression'),
    [string]$LogDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build_logs'),
    [string]$Configuration = 'Debug',
    [int]$TimeoutSec = 600
)

$ErrorActionPreference = 'Stop'

function Find-RendererDemoExe {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [string]$ConfigurationName,
        [string]$ExplicitExePath
    )

    if ($ExplicitExePath) {
        return (Resolve-Path $ExplicitExePath).Path
    }

    $searchRoots = @(
        (Join-Path $RepoRoot '.verify\bin'),
        (Join-Path $RepoRoot "x64\$ConfigurationName"),
        (Join-Path $RepoRoot 'x64')
    )

    foreach ($root in $searchRoots) {
        if (-not (Test-Path $root)) {
            continue
        }

        $candidate = Get-ChildItem -Path $root -Recurse -Filter RendererDemo.exe -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($candidate) {
            return $candidate.FullName
        }
    }

    throw 'RendererDemo.exe not found. Build the project first or pass -ExePath.'
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
    $psi.CreateNoWindow = $false

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

function Resolve-SummaryPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$StdoutLogPath,
        [Parameter(Mandatory = $true)]
        [string]$RegressionOutputRoot
    )

    if (Test-Path $StdoutLogPath) {
        $matches = Select-String -Path $StdoutLogPath -Pattern '\[Regression\] Suite completed\. Summary: (?<path>.+)$' -ErrorAction SilentlyContinue
        if ($matches) {
            $candidatePath = $matches[-1].Matches[0].Groups['path'].Value.Trim()
            if ($candidatePath -and (Test-Path $candidatePath)) {
                return (Resolve-Path $candidatePath).Path
            }
        }
    }

    if (Test-Path $RegressionOutputRoot) {
        $candidate = Get-ChildItem -Path $RegressionOutputRoot -Recurse -Filter suite_result.json -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($candidate) {
            return $candidate.FullName
        }
    }

    return $null
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedSuitePath = (Resolve-Path $SuitePath).Path
$resolvedOutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
$resolvedLogDir = [System.IO.Path]::GetFullPath($LogDir)
$resolvedExePath = Find-RendererDemoExe -RepoRoot $repoRoot -ConfigurationName $Configuration -ExplicitExePath $ExePath

New-Item -ItemType Directory -Path $resolvedOutputRoot -Force | Out-Null
New-Item -ItemType Directory -Path $resolvedLogDir -Force | Out-Null

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$apiTag = if ($Api -eq 'DX12') { 'dx12' } else { 'vulkan' }
$runLogDir = Join-Path $resolvedLogDir "regression_${apiTag}_$stamp"
$apiOutputRoot = Join-Path $resolvedOutputRoot $apiTag
New-Item -ItemType Directory -Path $runLogDir -Force | Out-Null
New-Item -ItemType Directory -Path $apiOutputRoot -Force | Out-Null

$stdoutLog = Join-Path $runLogDir 'stdout.log'
$stderrLog = Join-Path $runLogDir 'stderr.log'
$resultJson = Join-Path $runLogDir 'run_result.json'

$apiArg = if ($Api -eq 'DX12') { '-dx12' } else { '-vulkan' }
$presentArg = if ($PresentMode -eq 'Mailbox') { '-mailbox' } else { '-vsync' }
$args = @(
    $DemoName,
    '--no-assert-dialog',
    $apiArg,
    $presentArg,
    '-regression',
    "-regression-suite=$resolvedSuitePath",
    "-regression-output=$apiOutputRoot"
)

$startTime = Get-Date
$process = $null
$status = 'FAILED'
$exitCode = -1
$durationMs = 0

try {
    $runResult = Invoke-ExternalProcess `
        -FilePath $resolvedExePath `
        -Arguments $args `
        -WorkingDirectory (Split-Path $resolvedExePath -Parent) `
        -StdoutPath $stdoutLog `
        -StderrPath $stderrLog `
        -TimeoutSeconds $TimeoutSec
    $process = $runResult.process

    if ($runResult.timed_out) {
        $status = 'TIMEOUT'
    }
    else {
        $exitCode = $runResult.exit_code
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

$summaryPath = Resolve-SummaryPath -StdoutLogPath $stdoutLog -RegressionOutputRoot $apiOutputRoot
$suiteOutputRoot = $null
$failedCount = $null
$caseCount = $null

if ($summaryPath -and (Test-Path $summaryPath)) {
    $summaryJson = Get-Content $summaryPath -Raw | ConvertFrom-Json
    $suiteOutputRoot = $summaryJson.output_root
    $failedCount = $summaryJson.failed_count
    $caseCount = $summaryJson.case_count
}

if ($status -ne 'TIMEOUT') {
    if ($exitCode -eq 0 -and $summaryPath -and $failedCount -eq 0) {
        $status = 'SUCCESS'
    }
    else {
        $status = 'FAILED'
    }
}

$manifest = [ordered]@{
    status = $status
    exit_code = $exitCode
    duration_ms = $durationMs
    process_pid = if ($process) { $process.Id } else { $null }
    api = $Api
    present_mode = $PresentMode
    exe_path = $resolvedExePath
    suite_path = $resolvedSuitePath
    requested_output_root = $apiOutputRoot
    suite_output_root = $suiteOutputRoot
    summary_path = $summaryPath
    case_count = $caseCount
    failed_count = $failedCount
    stdout_log = $stdoutLog
    stderr_log = $stderrLog
    result_json = $resultJson
}

$manifest | ConvertTo-Json -Depth 5 | Out-File -FilePath $resultJson -Encoding utf8

Write-Output "STATUS=$status"
Write-Output "EXIT_CODE=$exitCode"
Write-Output "PROCESS_PID=$($manifest.process_pid)"
Write-Output "DURATION_MS=$durationMs"
Write-Output "API=$Api"
Write-Output "PRESENT_MODE=$PresentMode"
Write-Output "EXE_PATH=$resolvedExePath"
Write-Output "SUITE_PATH=$resolvedSuitePath"
Write-Output "SUMMARY_PATH=$summaryPath"
Write-Output "FAILED_COUNT=$failedCount"
Write-Output "CASE_COUNT=$caseCount"
Write-Output "STDOUT_LOG=$stdoutLog"
Write-Output "STDERR_LOG=$stderrLog"
Write-Output "RESULT_JSON=$resultJson"

if ($status -ne 'SUCCESS') {
    exit 1
}
