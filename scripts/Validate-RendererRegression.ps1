param(
    [Parameter(Mandatory = $true)]
    [string]$Suite,
    [string[]]$Backends = @("dx", "vk"),
    [string]$OutputBase = ".tmp\\regression_validate",
    [string]$BuildScript = "scripts\\Build-RendererDemo-Verify.ps1",
    [string]$CaptureScript = "scripts\\Capture-RendererRegression.ps1",
    [string]$CompareScript = "scripts\\Compare-RendererRegression.ps1",
    [string]$Profile = "scripts\\RegressionCompareProfile.default.json",
    [string]$DemoApp = "DemoAppModelViewerFrostedGlass",
    [switch]$SkipBuild,
    [switch]$RenderDocCapture,
    [switch]$RenderDocRequired,
    [switch]$PIXCapture,
    [switch]$PIXRequired,
    [int]$BuildTimeoutSec = 900,
    [int]$CaptureTimeoutSec = 900,
    [switch]$DisableVisualCompare,
    [switch]$DisablePerfCompare
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
        throw "At least one backend must be specified."
    }

    return @($normalized)
}

function Convert-ToRepoRelativePath {
    param(
        [string]$Path,
        [string]$RepoRoot
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $rootPath = [System.IO.Path]::GetFullPath($RepoRoot)
    if (-not $rootPath.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $rootPath += [System.IO.Path]::DirectorySeparatorChar
    }

    if ($fullPath.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $fullPath.Substring($rootPath.Length)
    }

    return $fullPath
}

$repoRoot = (Resolve-Path -LiteralPath ".").Path
$suitePath = Resolve-RepoPath -Path $Suite -RepoRoot $repoRoot -RequireExists
$outputBasePath = Resolve-RepoPath -Path $OutputBase -RepoRoot $repoRoot
$buildScriptPath = Resolve-RepoPath -Path $BuildScript -RepoRoot $repoRoot -RequireExists
$captureScriptPath = Resolve-RepoPath -Path $CaptureScript -RepoRoot $repoRoot -RequireExists
$compareScriptPath = Resolve-RepoPath -Path $CompareScript -RepoRoot $repoRoot -RequireExists
$profilePath = Resolve-RepoPath -Path $Profile -RepoRoot $repoRoot -RequireExists
$Backends = Normalize-Backends -Values $Backends

if ($RenderDocCapture -and $PIXCapture) {
    throw "RenderDoc and PIX capture automation cannot be enabled in the same validation run."
}
if (($PIXCapture -or $PIXRequired) -and @($Backends | Where-Object { $_ -ne "dx" }).Count -gt 0) {
    throw "PIX capture automation is only supported on DX12 validation runs. Use -Backends dx."
}

New-Item -ItemType Directory -Path $outputBasePath -Force | Out-Null

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$validationRoot = Join-Path $outputBasePath ("rv_" + $stamp)
New-Item -ItemType Directory -Path $validationRoot -Force | Out-Null

$buildSummary = $null
$backendResults = New-Object System.Collections.Generic.List[object]
$buildPassed = $true

if (-not $SkipBuild) {
    $buildRoot = Join-Path $validationRoot "build"
    New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
    $buildLogDir = Convert-ToRepoRelativePath -Path (Join-Path $buildRoot "logs") -RepoRoot $repoRoot
    $buildVerifyRoot = Convert-ToRepoRelativePath -Path (Join-Path $buildRoot ".verify") -RepoRoot $repoRoot

    $buildResult = Invoke-ChildScript `
        -ScriptPath $buildScriptPath `
        -Arguments @(
            "-LogDir", $buildLogDir,
            "-VerifyRoot", $buildVerifyRoot,
            "-TimeoutSec", [string]$BuildTimeoutSec
        ) `
        -StdoutPath (Join-Path $buildRoot "build.stdout.log") `
        -StderrPath (Join-Path $buildRoot "build.stderr.log") `
        -TimeoutSec ($BuildTimeoutSec + 30)

    $buildStatus = Get-ChildValue -Result $buildResult -Name "STATUS"
    $buildPassed = ($buildResult.exit_code -eq 0 -and $buildStatus -eq "BuildSucceeded")
    $buildSummary = [ordered]@{
        skipped = $false
        pass = $buildPassed
        status = $buildStatus
        exit_code = $buildResult.exit_code
        warnings = To-Int (Get-ChildValue -Result $buildResult -Name "WARNINGS")
        errors = To-Int (Get-ChildValue -Result $buildResult -Name "ERRORS")
        duration_ms = To-Int (Get-ChildValue -Result $buildResult -Name "DURATION_MS")
        stdout = $buildResult.stdout
        stderr = $buildResult.stderr
        txt = Get-ChildValue -Result $buildResult -Name "TXT"
        binlog = Get-ChildValue -Result $buildResult -Name "BINLOG"
    }
}
else {
    $buildSummary = [ordered]@{
        skipped = $true
        pass = $true
        status = "BuildSkipped"
    }
}

if ($buildPassed) {
    foreach ($backend in $Backends) {
        $backendRoot = Join-Path $validationRoot $backend
        New-Item -ItemType Directory -Path $backendRoot -Force | Out-Null

        $sharedCaptureArgs = @(
            "-Suite", $suitePath,
            "-Backend", $backend,
            "-DemoApp", $DemoApp,
            "-TimeoutSec", [string]$CaptureTimeoutSec
        )
        if ($RenderDocCapture) {
            $sharedCaptureArgs += "-RenderDocCapture"
        }
        if ($RenderDocRequired) {
            $sharedCaptureArgs += "-RenderDocRequired"
        }
        if ($PIXCapture) {
            $sharedCaptureArgs += "-PIXCapture"
        }
        if ($PIXRequired) {
            $sharedCaptureArgs += "-PIXRequired"
        }

        $baselineResult = Invoke-ChildScript `
            -ScriptPath $captureScriptPath `
            -Arguments ($sharedCaptureArgs + @("-OutputBase", (Join-Path $backendRoot "a"))) `
            -StdoutPath (Join-Path $backendRoot "baseline.capture.stdout.log") `
            -StderrPath (Join-Path $backendRoot "baseline.capture.stderr.log") `
            -TimeoutSec ($CaptureTimeoutSec + 30)

        $currentResult = Invoke-ChildScript `
            -ScriptPath $captureScriptPath `
            -Arguments ($sharedCaptureArgs + @("-OutputBase", (Join-Path $backendRoot "b"))) `
            -StdoutPath (Join-Path $backendRoot "current.capture.stdout.log") `
            -StderrPath (Join-Path $backendRoot "current.capture.stderr.log") `
            -TimeoutSec ($CaptureTimeoutSec + 30)

        $baselineRunSucceeded = ($baselineResult.exit_code -eq 0 -and (Get-ChildValue -Result $baselineResult -Name "STATUS") -eq "RunSucceeded" -and (To-Bool (Get-ChildValue -Result $baselineResult -Name "SUITE_SUCCESS")))
        $currentRunSucceeded = ($currentResult.exit_code -eq 0 -and (Get-ChildValue -Result $currentResult -Name "STATUS") -eq "RunSucceeded" -and (To-Bool (Get-ChildValue -Result $currentResult -Name "SUITE_SUCCESS")))

        $compareResult = $null
        $comparePassed = $false
        if ($baselineRunSucceeded -and $currentRunSucceeded) {
            $compareArgs = @(
                "-Baseline", (Get-ChildValue -Result $baselineResult -Name "RUN_DIR"),
                "-Current", (Get-ChildValue -Result $currentResult -Name "RUN_DIR"),
                "-ReportOut", (Join-Path $backendRoot "c"),
                "-Profile", $profilePath
            )
            if ($DisableVisualCompare) {
                $compareArgs += "-DisableVisualCompare"
            }
            if ($DisablePerfCompare) {
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

        $backendPass = ($baselineRunSucceeded -and $currentRunSucceeded -and $comparePassed)
        $backendResults.Add([PSCustomObject][ordered]@{
            backend = $backend
            pass = $backendPass
            baseline = [ordered]@{
                status = Get-ChildValue -Result $baselineResult -Name "STATUS"
                exit_code = $baselineResult.exit_code
                run_dir = Get-ChildValue -Result $baselineResult -Name "RUN_DIR"
                summary = Get-ChildValue -Result $baselineResult -Name "SUMMARY"
                suite_success = To-Bool (Get-ChildValue -Result $baselineResult -Name "SUITE_SUCCESS")
                renderdoc_enabled = To-Bool (Get-ChildValue -Result $baselineResult -Name "RENDERDOC_ENABLED")
                renderdoc_available = To-Bool (Get-ChildValue -Result $baselineResult -Name "RENDERDOC_AVAILABLE")
                renderdoc_success_count = To-Int (Get-ChildValue -Result $baselineResult -Name "RENDERDOC_SUCCESS_COUNT")
                renderdoc_retained_count = To-Int (Get-ChildValue -Result $baselineResult -Name "RENDERDOC_RETAINED_COUNT")
                rdc_count = To-Int (Get-ChildValue -Result $baselineResult -Name "RDC_COUNT")
                pix_enabled = To-Bool (Get-ChildValue -Result $baselineResult -Name "PIX_ENABLED")
                pix_available = To-Bool (Get-ChildValue -Result $baselineResult -Name "PIX_AVAILABLE")
                pix_success_count = To-Int (Get-ChildValue -Result $baselineResult -Name "PIX_SUCCESS_COUNT")
                pix_retained_count = To-Int (Get-ChildValue -Result $baselineResult -Name "PIX_RETAINED_COUNT")
                wpix_count = To-Int (Get-ChildValue -Result $baselineResult -Name "WPIX_COUNT")
                stdout = $baselineResult.stdout
                stderr = $baselineResult.stderr
            }
            current = [ordered]@{
                status = Get-ChildValue -Result $currentResult -Name "STATUS"
                exit_code = $currentResult.exit_code
                run_dir = Get-ChildValue -Result $currentResult -Name "RUN_DIR"
                summary = Get-ChildValue -Result $currentResult -Name "SUMMARY"
                suite_success = To-Bool (Get-ChildValue -Result $currentResult -Name "SUITE_SUCCESS")
                renderdoc_enabled = To-Bool (Get-ChildValue -Result $currentResult -Name "RENDERDOC_ENABLED")
                renderdoc_available = To-Bool (Get-ChildValue -Result $currentResult -Name "RENDERDOC_AVAILABLE")
                renderdoc_success_count = To-Int (Get-ChildValue -Result $currentResult -Name "RENDERDOC_SUCCESS_COUNT")
                renderdoc_retained_count = To-Int (Get-ChildValue -Result $currentResult -Name "RENDERDOC_RETAINED_COUNT")
                rdc_count = To-Int (Get-ChildValue -Result $currentResult -Name "RDC_COUNT")
                pix_enabled = To-Bool (Get-ChildValue -Result $currentResult -Name "PIX_ENABLED")
                pix_available = To-Bool (Get-ChildValue -Result $currentResult -Name "PIX_AVAILABLE")
                pix_success_count = To-Int (Get-ChildValue -Result $currentResult -Name "PIX_SUCCESS_COUNT")
                pix_retained_count = To-Int (Get-ChildValue -Result $currentResult -Name "PIX_RETAINED_COUNT")
                wpix_count = To-Int (Get-ChildValue -Result $currentResult -Name "WPIX_COUNT")
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
$overallStatus = if ($overallPass) { "ValidationPassed" } elseif (-not $buildPassed) { "BuildFailed" } else { "ValidationFailed" }

$summaryObject = [ordered]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    suite = $suitePath
    output_root = $validationRoot
    backends = $Backends
    renderdoc_capture = [bool]$RenderDocCapture
    renderdoc_required = [bool]$RenderDocRequired
    pix_capture = [bool]$PIXCapture
    pix_required = [bool]$PIXRequired
    build = $buildSummary
    backend_total = $backendResults.Count
    backend_pass_total = $passedBackendCount
    backend_fail_total = $failedBackendCount
    pass = $overallPass
    status = $overallStatus
    results = $backendResults
}

$summaryJsonPath = Join-Path $validationRoot "summary.json"
$summaryMdPath = Join-Path $validationRoot "summary.md"
$summaryObject | ConvertTo-Json -Depth 16 | Out-File -LiteralPath $summaryJsonPath -Encoding utf8

$md = New-Object System.Collections.Generic.List[string]
$md.Add("# Regression Validation Summary")
$md.Add("")
$md.Add("- Status: $overallStatus")
$md.Add("- Suite: $suitePath")
$md.Add("- Backends: " + ($Backends -join ", "))
$md.Add("- RenderDoc Capture: " + ($(if ($RenderDocCapture) { "YES" } else { "NO" })))
$md.Add("- RenderDoc Required: " + ($(if ($RenderDocRequired) { "YES" } else { "NO" })))
$md.Add("- PIX Capture: " + ($(if ($PIXCapture) { "YES" } else { "NO" })))
$md.Add("- PIX Required: " + ($(if ($PIXRequired) { "YES" } else { "NO" })))
$md.Add("- Build: $($buildSummary.status)")
$md.Add("- Passed Backends: $passedBackendCount/$($backendResults.Count)")
$md.Add("")
$md.Add("| Backend | Pass | Baseline | Current | Compare | Perf Skipped | Notes |")
$md.Add("|---|---|---|---|---|---|---|")
foreach ($backendResult in $backendResults) {
    $notes = @()
    $renderDocSuccessCount =
        $backendResult.baseline.renderdoc_success_count +
        $backendResult.current.renderdoc_success_count
    $renderDocRetainedCount =
        $backendResult.baseline.renderdoc_retained_count +
        $backendResult.current.renderdoc_retained_count
    if ($renderDocSuccessCount -eq 0) {
        $renderDocSuccessCount = $backendResult.baseline.rdc_count + $backendResult.current.rdc_count
    }
    if ($renderDocRetainedCount -eq 0) {
        $renderDocRetainedCount = $backendResult.baseline.rdc_count + $backendResult.current.rdc_count
    }
    if ($renderDocSuccessCount -gt 0) {
        $notes += "rdc"
    }
    if ($renderDocSuccessCount -gt 0 -and $renderDocRetainedCount -eq 0) {
        $notes += "rdc-pruned"
    }
    $pixSuccessCount =
        $backendResult.baseline.pix_success_count +
        $backendResult.current.pix_success_count
    $pixRetainedCount =
        $backendResult.baseline.pix_retained_count +
        $backendResult.current.pix_retained_count
    if ($pixSuccessCount -eq 0) {
        $pixSuccessCount = $backendResult.baseline.wpix_count + $backendResult.current.wpix_count
    }
    if ($pixRetainedCount -eq 0) {
        $pixRetainedCount = $backendResult.baseline.wpix_count + $backendResult.current.wpix_count
    }
    if ($pixSuccessCount -gt 0) {
        $notes += "wpix"
    }
    if ($pixSuccessCount -gt 0 -and $pixRetainedCount -eq 0) {
        $notes += "wpix-pruned"
    }
    if ($backendResult.compare.perf_skipped -gt 0) {
        $notes += "perf-skipped"
    }
    if ($backendResult.compare.failures -gt 0) {
        $notes += "compare-failures=$($backendResult.compare.failures)"
    }
    if (-not $backendResult.baseline.suite_success -or -not $backendResult.current.suite_success) {
        $notes += "suite-failed"
    }

    $md.Add("| $($backendResult.backend) | $(if($backendResult.pass){'PASS'}else{'FAIL'}) | $($backendResult.baseline.status) | $($backendResult.current.status) | $($backendResult.compare.status) | $($backendResult.compare.perf_skipped) | $($notes -join '; ') |")
}

$md | Out-File -LiteralPath $summaryMdPath -Encoding utf8

Write-Host "STATUS=$overallStatus"
Write-Host "SUITE=$suitePath"
Write-Host "OUTPUT_ROOT=$validationRoot"
Write-Host "SUMMARY_JSON=$summaryJsonPath"
Write-Host "SUMMARY_MD=$summaryMdPath"
Write-Host "BACKENDS=$($Backends -join ',')"
Write-Host "PIX_CAPTURE=$([bool]$PIXCapture)"
Write-Host "PIX_REQUIRED=$([bool]$PIXRequired)"
Write-Host "PASSED_BACKENDS=$passedBackendCount"
Write-Host "FAILED_BACKENDS=$failedBackendCount"
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
