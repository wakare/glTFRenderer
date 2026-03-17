param(
    [string]$BaselineManifest = "build_logs\\regression_baselines\\a5_model_viewer_lighting\\latest.json",
    [string[]]$Backends = @("dx", "vk"),
    [string]$OutputBase = ".tmp\\regression_compare_a5_model_viewer_lighting",
    [string]$BuildScript = "scripts\\Build-RendererDemo-Verify.ps1",
    [string]$CaptureScript = "scripts\\Capture-RendererRegression.ps1",
    [string]$CompareScript = "scripts\\Compare-RendererRegression.ps1",
    [string]$Profile = "scripts\\RegressionCompareProfile.default.json",
    [switch]$SkipBuild,
    [int]$BuildTimeoutSec = 900,
    [int]$CaptureTimeoutSec = 900,
    [switch]$DisableVisualCompare,
    [switch]$EnablePerfCompare
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
        return $Path
    }

    $candidate = $Path
    if (-not [System.IO.Path]::IsPathRooted($candidate)) {
        $candidate = Join-Path $RepoRoot $candidate
    }

    $candidate = [System.IO.Path]::GetFullPath($candidate)
    if ($RequireExists -and -not (Test-Path -LiteralPath $candidate)) {
        throw "Path not found: $candidate"
    }

    return $candidate
}

function Stop-ProcessTree {
    param([int]$ParentProcessId)

    try {
        $children = Get-CimInstance Win32_Process -Filter "ParentProcessId = $ParentProcessId"
    }
    catch {
        $children = @()
    }

    foreach ($child in @($children)) {
        Stop-ProcessTree -ParentProcessId $child.ProcessId
        try {
            Stop-Process -Id $child.ProcessId -Force -ErrorAction SilentlyContinue
        }
        catch {}
    }

    try {
        Stop-Process -Id $ParentProcessId -Force -ErrorAction SilentlyContinue
    }
    catch {}
}

function Invoke-ChildScript {
    param(
        [string]$ScriptPath,
        [string[]]$Arguments,
        [string]$StdoutPath,
        [string]$StderrPath,
        [int]$TimeoutSec = 0,
        [int]$StatusGraceSec = 5
    )

    $launcherArgs = New-Object System.Collections.Generic.List[string]
    $launcherArgs.Add("-ExecutionPolicy")
    $launcherArgs.Add("Bypass")
    $launcherArgs.Add("-File")
    $launcherArgs.Add($ScriptPath)
    foreach ($arg in $Arguments) {
        if (-not [string]::IsNullOrWhiteSpace($arg)) {
            $launcherArgs.Add($arg)
        }
    }

    $process = Start-Process -FilePath "powershell" `
        -ArgumentList $launcherArgs `
        -NoNewWindow `
        -PassThru `
        -RedirectStandardOutput $StdoutPath `
        -RedirectStandardError $StderrPath

    $timedOut = $false
    $completedByStatus = $false
    $statusSeenAt = $null
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    while (-not $process.HasExited) {
        Start-Sleep -Milliseconds 250

        if ($null -eq $statusSeenAt -and (Test-Path -LiteralPath $StdoutPath)) {
            $statusSeen = Select-String -Path $StdoutPath -Pattern '^STATUS=' -Quiet
            if ($statusSeen) {
                $statusSeenAt = [System.Diagnostics.Stopwatch]::StartNew()
            }
        }

        if ($null -ne $statusSeenAt -and $statusSeenAt.Elapsed.TotalSeconds -ge $StatusGraceSec) {
            $completedByStatus = $true
            Stop-ProcessTree -ParentProcessId $process.Id
            break
        }

        if ($TimeoutSec -gt 0 -and $stopwatch.Elapsed.TotalSeconds -ge $TimeoutSec) {
            $timedOut = $true
            Stop-ProcessTree -ParentProcessId $process.Id
            break
        }
    }

    try {
        $process.WaitForExit()
    }
    catch {}

    $keyValues = @{}
    if (Test-Path -LiteralPath $StdoutPath) {
        $matches = Select-String -Path $StdoutPath -Pattern '^[A-Z0-9_]+='
        foreach ($match in @($matches)) {
            $line = [string]$match.Line
            $separator = $line.IndexOf('=')
            if ($separator -le 0) {
                continue
            }

            $key = $line.Substring(0, $separator)
            $value = $line.Substring($separator + 1)
            $keyValues[$key] = $value
        }
    }

    return [PSCustomObject]@{
        exit_code = if ($timedOut) { 124 } else { [int]$process.ExitCode }
        stdout = $StdoutPath
        stderr = $StderrPath
        values = $keyValues
        timed_out = $timedOut
        completed_by_status = $completedByStatus
    }
}

function Get-ChildValue {
    param(
        $Result,
        [string]$Name,
        [string]$Default = ""
    )

    if ($null -eq $Result -or $null -eq $Result.values) {
        return $Default
    }

    if ($Result.values.ContainsKey($Name)) {
        return [string]$Result.values[$Name]
    }

    return $Default
}

function To-Bool {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $false
    }

    return $Value.Equals("true", [System.StringComparison]::OrdinalIgnoreCase)
}

function To-Int {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return 0
    }

    $parsed = 0
    if ([int]::TryParse($Value, [ref]$parsed)) {
        return $parsed
    }

    return 0
}

function Normalize-Backends {
    param([string[]]$Values)

    $normalized = New-Object System.Collections.Generic.List[string]
    foreach ($value in $Values) {
        if ([string]::IsNullOrWhiteSpace($value)) {
            continue
        }

        foreach ($token in ($value -split ',')) {
            $backend = $token.Trim().ToLowerInvariant()
            if ([string]::IsNullOrWhiteSpace($backend)) {
                continue
            }
            if ($backend -ne "dx" -and $backend -ne "vk") {
                throw "Unsupported backend: $backend"
            }
            if (-not $normalized.Contains($backend)) {
                $normalized.Add($backend)
            }
        }
    }

    if ($normalized.Count -eq 0) {
        throw "At least one backend is required."
    }

    return $normalized.ToArray()
}

$repoRoot = (Resolve-Path -LiteralPath ".").Path
$baselineManifestPath = Resolve-RepoPath -Path $BaselineManifest -RepoRoot $repoRoot -RequireExists
$outputBasePath = Resolve-RepoPath -Path $OutputBase -RepoRoot $repoRoot
$buildScriptPath = Resolve-RepoPath -Path $BuildScript -RepoRoot $repoRoot -RequireExists
$captureScriptPath = Resolve-RepoPath -Path $CaptureScript -RepoRoot $repoRoot -RequireExists
$compareScriptPath = Resolve-RepoPath -Path $CompareScript -RepoRoot $repoRoot -RequireExists
$profilePath = Resolve-RepoPath -Path $Profile -RepoRoot $repoRoot -RequireExists
$Backends = Normalize-Backends -Values $Backends

$baselineManifestJson = Get-Content -LiteralPath $baselineManifestPath -Raw | ConvertFrom-Json
if ([string]$baselineManifestJson.status -ne "BaselinePromoted") {
    throw "Baseline manifest is not in BaselinePromoted state: $baselineManifestPath"
}

$suitePath = Resolve-RepoPath -Path ([string]$baselineManifestJson.suite) -RepoRoot $repoRoot -RequireExists
$baselineByBackend = @{}
foreach ($backendEntry in $baselineManifestJson.backends) {
    $backendKey = ([string]$backendEntry.backend).ToLowerInvariant()
    $baselineByBackend[$backendKey] = $backendEntry
}

foreach ($backend in $Backends) {
    if (-not $baselineByBackend.ContainsKey($backend)) {
        throw "Baseline manifest does not contain backend '$backend': $baselineManifestPath"
    }
}

New-Item -ItemType Directory -Path $outputBasePath -Force | Out-Null
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$comparisonRoot = Join-Path $outputBasePath ("a5cb_" + $stamp)
New-Item -ItemType Directory -Path $comparisonRoot -Force | Out-Null

$backendResults = New-Object System.Collections.Generic.List[object]

$buildPassed = $SkipBuild.IsPresent
$buildStatus = if ($SkipBuild) { "BuildSkipped" } else { "BuildNotRun" }
$buildInfo = [ordered]@{
    pass = $SkipBuild.IsPresent
    status = $buildStatus
    warnings = 0
    errors = 0
    duration_ms = 0
    stdout = ""
    stderr = ""
    txt = ""
    binlog = ""
}

if (-not $SkipBuild) {
    $buildStdout = Join-Path $comparisonRoot "build.stdout.log"
    $buildStderr = Join-Path $comparisonRoot "build.stderr.log"
    $buildResult = Invoke-ChildScript `
        -ScriptPath $buildScriptPath `
        -Arguments @() `
        -StdoutPath $buildStdout `
        -StderrPath $buildStderr `
        -TimeoutSec $BuildTimeoutSec

    $buildStatus = Get-ChildValue -Result $buildResult -Name "STATUS"
    $buildPassed = ($buildResult.exit_code -eq 0 -and $buildStatus -eq "BuildSucceeded")
    $buildInfo = [ordered]@{
        pass = $buildPassed
        status = $buildStatus
        warnings = To-Int (Get-ChildValue -Result $buildResult -Name "WARNINGS")
        errors = To-Int (Get-ChildValue -Result $buildResult -Name "ERRORS")
        duration_ms = To-Int (Get-ChildValue -Result $buildResult -Name "DURATION_MS")
        stdout = $buildStdout
        stderr = $buildStderr
        txt = Get-ChildValue -Result $buildResult -Name "TXT"
        binlog = Get-ChildValue -Result $buildResult -Name "BINLOG"
    }
}

if ($buildPassed) {
    foreach ($backend in $Backends) {
        $backendRoot = Join-Path $comparisonRoot $backend
        New-Item -ItemType Directory -Path $backendRoot -Force | Out-Null

        $baselineEntry = $baselineByBackend[$backend]
        $baselineRunDir = Resolve-RepoPath -Path ([string]$baselineEntry.run_dir) -RepoRoot $repoRoot -RequireExists
        $baselineSummary = Resolve-RepoPath -Path ([string]$baselineEntry.summary) -RepoRoot $repoRoot -RequireExists

        $captureArgs = @(
            "-Suite", $suitePath,
            "-Backend", $backend,
            "-DemoApp", "DemoAppModelViewer",
            "-OutputBase", (Join-Path $backendRoot "current")
        )
        $currentResult = Invoke-ChildScript `
            -ScriptPath $captureScriptPath `
            -Arguments $captureArgs `
            -StdoutPath (Join-Path $backendRoot "current.capture.stdout.log") `
            -StderrPath (Join-Path $backendRoot "current.capture.stderr.log") `
            -TimeoutSec $CaptureTimeoutSec

        $currentRunSucceeded = ($currentResult.exit_code -eq 0 -and (Get-ChildValue -Result $currentResult -Name "STATUS") -eq "RunSucceeded" -and (To-Bool (Get-ChildValue -Result $currentResult -Name "SUITE_SUCCESS")))

        $compareResult = $null
        $comparePassed = $false
        if ($currentRunSucceeded) {
            $compareArgs = @(
                "-Baseline", $baselineRunDir,
                "-Current", (Get-ChildValue -Result $currentResult -Name "RUN_DIR"),
                "-ReportOut", (Join-Path $backendRoot "compare"),
                "-Profile", $profilePath
            )
            if ($DisableVisualCompare) {
                $compareArgs += "-DisableVisualCompare"
            }
            if (-not $EnablePerfCompare) {
                $compareArgs += "-DisablePerfCompare"
            }

            $compareResult = Invoke-ChildScript `
                -ScriptPath $compareScriptPath `
                -Arguments $compareArgs `
                -StdoutPath (Join-Path $backendRoot "compare.stdout.log") `
                -StderrPath (Join-Path $backendRoot "compare.stderr.log") `
                -TimeoutSec 300

            $comparePassed = ($compareResult.exit_code -eq 0 -and (Get-ChildValue -Result $compareResult -Name "STATUS") -eq "ComparePassed")
        }

        $backendResults.Add([PSCustomObject][ordered]@{
            backend = $backend
            pass = ($currentRunSucceeded -and $comparePassed)
            baseline = [ordered]@{
                status = "BaselinePromoted"
                run_dir = $baselineRunDir
                summary = $baselineSummary
                manifest = $baselineManifestPath
            }
            current = [ordered]@{
                status = Get-ChildValue -Result $currentResult -Name "STATUS"
                exit_code = $currentResult.exit_code
                run_dir = Get-ChildValue -Result $currentResult -Name "RUN_DIR"
                summary = Get-ChildValue -Result $currentResult -Name "SUMMARY"
                suite_success = To-Bool (Get-ChildValue -Result $currentResult -Name "SUITE_SUCCESS")
                stdout = $currentResult.stdout
                stderr = $currentResult.stderr
            }
            compare = [ordered]@{
                status = if ($null -ne $compareResult) { Get-ChildValue -Result $compareResult -Name "STATUS" } else { "CompareNotRun" }
                exit_code = if ($null -ne $compareResult) { $compareResult.exit_code } else { -1 }
                failures = if ($null -ne $compareResult) { To-Int (Get-ChildValue -Result $compareResult -Name "FAILURES") } else { 0 }
                visual_failures = if ($null -ne $compareResult) { To-Int (Get-ChildValue -Result $compareResult -Name "VISUAL_FAILURES") } else { 0 }
                perf_failures = if ($null -ne $compareResult) { To-Int (Get-ChildValue -Result $compareResult -Name "PERF_FAILURES") } else { 0 }
                perf_skipped = if ($null -ne $compareResult) { To-Int (Get-ChildValue -Result $compareResult -Name "PERF_SKIPPED") } else { 0 }
                missing_failures = if ($null -ne $compareResult) { To-Int (Get-ChildValue -Result $compareResult -Name "MISSING_FAILURES") } else { 0 }
                summary_json = if ($null -ne $compareResult) { Get-ChildValue -Result $compareResult -Name "SUMMARY_JSON" } else { "" }
                summary_md = if ($null -ne $compareResult) { Get-ChildValue -Result $compareResult -Name "SUMMARY_MD" } else { "" }
                diff_dir = if ($null -ne $compareResult) { Get-ChildValue -Result $compareResult -Name "DIFF_DIR" } else { "" }
                stdout = if ($null -ne $compareResult) { $compareResult.stdout } else { "" }
                stderr = if ($null -ne $compareResult) { $compareResult.stderr } else { "" }
            }
        })
    }
}

$passedBackendCount = @($backendResults | Where-Object { $_.pass }).Count
$failedBackendCount = @($backendResults | Where-Object { -not $_.pass }).Count
$overallPass = ($buildPassed -and $failedBackendCount -eq 0)
$status = if ($overallPass) { "CompareAgainstBaselinePassed" } else { "CompareAgainstBaselineFailed" }

$summaryObject = [ordered]@{
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    suite = $suitePath
    baseline_manifest = $baselineManifestPath
    baseline_label = [string]$baselineManifestJson.label
    output_root = $comparisonRoot
    backends = $Backends
    build = $buildInfo
    backend_total = $backendResults.Count
    backend_pass_total = $passedBackendCount
    backend_fail_total = $failedBackendCount
    pass = $overallPass
    status = $status
    results = $backendResults
}

$summaryJsonPath = Join-Path $comparisonRoot "summary.json"
$summaryMdPath = Join-Path $comparisonRoot "summary.md"
$summaryObject | ConvertTo-Json -Depth 16 | Out-File -LiteralPath $summaryJsonPath -Encoding utf8

$md = New-Object System.Collections.Generic.List[string]
$md.Add("# A5 Baseline Compare Summary")
$md.Add("")
$md.Add("- Status: $status")
$md.Add("- Baseline Manifest: $baselineManifestPath")
$md.Add("- Baseline Label: $([string]$baselineManifestJson.label)")
$md.Add("- Suite: $suitePath")
$md.Add("- Backends: " + ($Backends -join ", "))
$md.Add("- Build: $buildStatus")
$md.Add("- Passed Backends: $passedBackendCount/$($backendResults.Count)")
$md.Add("")
$md.Add("| Backend | Pass | Baseline | Current | Compare | Perf Skipped | Notes |")
$md.Add("|---|---|---|---|---|---|---|")
foreach ($backendResult in $backendResults) {
    $notes = @()
    if ($backendResult.compare.failures -gt 0) {
        $notes += "compare-failures=$($backendResult.compare.failures)"
    }
    if (-not $backendResult.current.suite_success) {
        $notes += "current-suite-failed"
    }
    $md.Add("| $($backendResult.backend) | $(if($backendResult.pass){'PASS'}else{'FAIL'}) | $($backendResult.baseline.status) | $($backendResult.current.status) | $($backendResult.compare.status) | $($backendResult.compare.perf_skipped) | $($notes -join '; ') |")
}
$md | Out-File -LiteralPath $summaryMdPath -Encoding utf8

Write-Host "STATUS=$status"
Write-Host "BASELINE_MANIFEST=$baselineManifestPath"
Write-Host "OUTPUT_ROOT=$comparisonRoot"
Write-Host "SUMMARY_JSON=$summaryJsonPath"
Write-Host "SUMMARY_MD=$summaryMdPath"
Write-Host "BACKENDS=$($Backends -join ',')"
Write-Host "PASSED_BACKENDS=$passedBackendCount"
Write-Host "FAILED_BACKENDS=$failedBackendCount"
Write-Host "BUILD_STATUS=$buildStatus"

foreach ($backendResult in $backendResults) {
    $key = $backendResult.backend.ToUpperInvariant()
    Write-Host ("BACKEND_{0}_STATUS={1}" -f $key, $(if ($backendResult.pass) { "Passed" } else { "Failed" }))
    Write-Host ("BACKEND_{0}_BASELINE_RUN={1}" -f $key, $backendResult.baseline.run_dir)
    Write-Host ("BACKEND_{0}_CURRENT_RUN={1}" -f $key, $backendResult.current.run_dir)
    Write-Host ("BACKEND_{0}_COMPARE_JSON={1}" -f $key, $backendResult.compare.summary_json)
    Write-Host ("BACKEND_{0}_PERF_SKIPPED={1}" -f $key, $backendResult.compare.perf_skipped)
}

if ($overallPass) {
    exit 0
}

exit 1
