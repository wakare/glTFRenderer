param(
    [Parameter(Mandatory = $true)]
    [string]$Baseline,
    [Parameter(Mandatory = $true)]
    [string]$Current,
    [string]$ReportOut = "build_logs/regression_compare",
    [string]$Profile = "",
    [switch]$DisableVisualCompare,
    [switch]$DisablePerfCompare,
    [switch]$FailOnMissingCase
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

function Resolve-SummaryPath {
    param(
        [string]$InputPath
    )

    if (-not (Test-Path -LiteralPath $InputPath)) {
        throw "Path not found: $InputPath"
    }

    $item = Get-Item -LiteralPath $InputPath
    if ($item.PSIsContainer) {
        $summaryPath = Join-Path $item.FullName "suite_result.json"
        if (-not (Test-Path -LiteralPath $summaryPath)) {
            throw "suite_result.json not found under directory: $InputPath"
        }
        return (Resolve-Path -LiteralPath $summaryPath).Path
    }

    if ($item.Extension -ne ".json") {
        throw "Input must be a run directory or suite_result.json file: $InputPath"
    }
    return (Resolve-Path -LiteralPath $item.FullName).Path
}

function Resolve-PathAgainstRoot {
    param(
        [string]$RawPath,
        [string]$RootDirectory
    )

    if ([string]::IsNullOrWhiteSpace($RawPath)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($RawPath)) {
        return $RawPath
    }

    return (Join-Path $RootDirectory $RawPath)
}

function Load-RunResult {
    param(
        [string]$SummaryPath
    )

    $rootDir = Split-Path -Parent $SummaryPath
    $jsonText = Get-Content -LiteralPath $SummaryPath -Raw
    $summary = $jsonText | ConvertFrom-Json
    if (-not $summary.cases) {
        throw "Invalid suite_result.json: missing cases array ($SummaryPath)"
    }

    $caseMap = @{}
    foreach ($caseItem in $summary.cases) {
        $id = [string]$caseItem.id
        if ([string]::IsNullOrWhiteSpace($id)) {
            continue
        }

        $caseMap[$id] = [PSCustomObject]@{
            id = $id
            success = [bool]$caseItem.success
            screenshot_path = Resolve-PathAgainstRoot -RawPath ([string]$caseItem.screenshot_path) -RootDirectory $rootDir
            pass_csv_path = Resolve-PathAgainstRoot -RawPath ([string]$caseItem.pass_csv_path) -RootDirectory $rootDir
            perf_json_path = Resolve-PathAgainstRoot -RawPath ([string]$caseItem.perf_json_path) -RootDirectory $rootDir
            renderdoc_capture_success = [bool]$caseItem.renderdoc_capture_success
            renderdoc_capture_retained = [bool]$caseItem.renderdoc_capture_retained
            renderdoc_capture_keep_on_success = if ($null -ne $caseItem.renderdoc_capture_keep_on_success) { [bool]$caseItem.renderdoc_capture_keep_on_success } else { $true }
            renderdoc_capture_frame_index = [uint64]$caseItem.renderdoc_capture_frame_index
            renderdoc_capture_path = Resolve-PathAgainstRoot -RawPath ([string]$caseItem.renderdoc_capture_path) -RootDirectory $rootDir
            renderdoc_capture_error = [string]$caseItem.renderdoc_capture_error
            pix_capture_success = if ($null -ne $caseItem.pix_capture_success) { [bool]$caseItem.pix_capture_success } else { $false }
            pix_capture_retained = if ($null -ne $caseItem.pix_capture_retained) { [bool]$caseItem.pix_capture_retained } else { $false }
            pix_capture_keep_on_success = if ($null -ne $caseItem.pix_capture_keep_on_success) { [bool]$caseItem.pix_capture_keep_on_success } else { $true }
            pix_capture_frame_index = if ($null -ne $caseItem.pix_capture_frame_index) { [uint64]$caseItem.pix_capture_frame_index } else { 0 }
            pix_capture_path = Resolve-PathAgainstRoot -RawPath ([string]$caseItem.pix_capture_path) -RootDirectory $rootDir
            pix_capture_error = [string]$caseItem.pix_capture_error
            error = [string]$caseItem.error
        }
    }

    return [PSCustomObject]@{
        summary_path = $SummaryPath
        root_dir = $rootDir
        suite_name = [string]$summary.suite_name
        case_count = [int]$summary.case_count
        cases = $caseMap
    }
}

function Get-CompareProfile {
    param(
        [string]$ProfilePath
    )

    $profile = [ordered]@{
        visual = [ordered]@{
            mae_max = 0.03
            rmse_max = 0.05
            psnr_min = 26.0
        }
        perf = [ordered]@{
            cpu_total_increase_pct_max = 15.0
            gpu_total_increase_pct_max = 15.0
            frosted_cpu_increase_pct_max = 20.0
            frosted_gpu_increase_pct_max = 20.0
        }
    }

    if ([string]::IsNullOrWhiteSpace($ProfilePath)) {
        return $profile
    }
    if (-not (Test-Path -LiteralPath $ProfilePath)) {
        throw "Profile not found: $ProfilePath"
    }

    $custom = (Get-Content -LiteralPath $ProfilePath -Raw | ConvertFrom-Json)
    if ($custom.visual) {
        foreach ($k in @("mae_max", "rmse_max", "psnr_min")) {
            if ($null -ne $custom.visual.$k) {
                $profile.visual[$k] = [double]$custom.visual.$k
            }
        }
    }
    if ($custom.perf) {
        foreach ($k in @(
                "cpu_total_increase_pct_max",
                "gpu_total_increase_pct_max",
                "frosted_cpu_increase_pct_max",
                "frosted_gpu_increase_pct_max")) {
            if ($null -ne $custom.perf.$k) {
                $profile.perf[$k] = [double]$custom.perf.$k
            }
        }
    }

    return $profile
}

function Get-BitmapBytes {
    param(
        [System.Drawing.Bitmap]$Bitmap
    )

    $rect = New-Object System.Drawing.Rectangle(0, 0, $Bitmap.Width, $Bitmap.Height)
    $data = $Bitmap.LockBits(
        $rect,
        [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    )
    try {
        $stride = [math]::Abs($data.Stride)
        $bytes = New-Object byte[] ($stride * $Bitmap.Height)
        [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
        return [PSCustomObject]@{
            width = $Bitmap.Width
            height = $Bitmap.Height
            stride = $stride
            bytes = $bytes
        }
    }
    finally {
        $Bitmap.UnlockBits($data)
    }
}

function Save-DiffBitmap {
    param(
        [byte[]]$Bytes,
        [int]$Width,
        [int]$Height,
        [string]$OutPath
    )

    $bmp = New-Object System.Drawing.Bitmap($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $rect = New-Object System.Drawing.Rectangle(0, 0, $Width, $Height)
    $data = $bmp.LockBits(
        $rect,
        [System.Drawing.Imaging.ImageLockMode]::WriteOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    )
    try {
        [Runtime.InteropServices.Marshal]::Copy($Bytes, 0, $data.Scan0, $Bytes.Length)
    }
    finally {
        $bmp.UnlockBits($data)
    }

    $outDir = Split-Path -Parent $OutPath
    if (-not (Test-Path -LiteralPath $outDir)) {
        New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    }
    $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Compare-Images {
    param(
        [string]$BaselinePath,
        [string]$CurrentPath,
        [string]$DiffOutPath
    )

    $baselineBmp = New-Object System.Drawing.Bitmap($BaselinePath)
    $currentBmp = New-Object System.Drawing.Bitmap($CurrentPath)
    try {
        if ($baselineBmp.Width -ne $currentBmp.Width -or $baselineBmp.Height -ne $currentBmp.Height) {
            return [PSCustomObject]@{
                success = $false
                reason = "DimensionMismatch"
                baseline_width = $baselineBmp.Width
                baseline_height = $baselineBmp.Height
                current_width = $currentBmp.Width
                current_height = $currentBmp.Height
                mae = $null
                rmse = $null
                psnr = $null
                diff_path = ""
            }
        }

        $a = Get-BitmapBytes -Bitmap $baselineBmp
        $b = Get-BitmapBytes -Bitmap $currentBmp

        $pixelCount = [long]$a.width * [long]$a.height
        $channelCount = [double]($pixelCount * 3)
        $sumAbs = 0.0
        $sumSq = 0.0
        $diffBytes = New-Object byte[] $a.bytes.Length

        for ($y = 0; $y -lt $a.height; $y++) {
            $rowOffset = $y * $a.stride
            for ($x = 0; $x -lt $a.width; $x++) {
                $i = $rowOffset + ($x * 4)
                $db = [math]::Abs([int]$a.bytes[$i + 0] - [int]$b.bytes[$i + 0])
                $dg = [math]::Abs([int]$a.bytes[$i + 1] - [int]$b.bytes[$i + 1])
                $dr = [math]::Abs([int]$a.bytes[$i + 2] - [int]$b.bytes[$i + 2])

                $sumAbs += ($db + $dg + $dr) / 255.0
                $sumSq += (($db * $db) + ($dg * $dg) + ($dr * $dr)) / (255.0 * 255.0)

                $ampB = [Math]::Min(255, [int]($db * 4))
                $ampG = [Math]::Min(255, [int]($dg * 4))
                $ampR = [Math]::Min(255, [int]($dr * 4))
                $diffBytes[$i + 0] = [byte]$ampB
                $diffBytes[$i + 1] = [byte]$ampG
                $diffBytes[$i + 2] = [byte]$ampR
                $diffBytes[$i + 3] = 255
            }
        }

        $mae = $sumAbs / $channelCount
        $rmse = [math]::Sqrt($sumSq / $channelCount)
        $psnr = if ($rmse -le 1e-12) { [double]::PositiveInfinity } else { 20.0 * [math]::Log10(1.0 / $rmse) }

        Save-DiffBitmap -Bytes $diffBytes -Width $a.width -Height $a.height -OutPath $DiffOutPath

        return [PSCustomObject]@{
            success = $true
            reason = ""
            baseline_width = $a.width
            baseline_height = $a.height
            current_width = $b.width
            current_height = $b.height
            mae = [double]$mae
            rmse = [double]$rmse
            psnr = [double]$psnr
            diff_path = $DiffOutPath
        }
    }
    finally {
        $baselineBmp.Dispose()
        $currentBmp.Dispose()
    }
}

function Load-PerfJson {
    param(
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }
    return (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
}

function Compare-PerfMetrics {
    param(
        $BaselinePerf,
        $CurrentPerf,
        $PerfThresholds
    )

    $checks = @(
        @{ name = "cpu_total_avg_ms"; threshold = "cpu_total_increase_pct_max" },
        @{ name = "gpu_total_avg_ms"; threshold = "gpu_total_increase_pct_max" },
        @{ name = "frosted_cpu_avg_ms"; threshold = "frosted_cpu_increase_pct_max" },
        @{ name = "frosted_gpu_avg_ms"; threshold = "frosted_gpu_increase_pct_max" }
    )

    $resultRows = @()
    $hasFailure = $false

    foreach ($check in $checks) {
        $metricName = $check.name
        $thresholdName = $check.threshold
        $baselineValue = $BaselinePerf.$metricName
        $currentValue = $CurrentPerf.$metricName

        $row = [ordered]@{
            metric = $metricName
            baseline = $baselineValue
            current = $currentValue
            increase_pct = $null
            threshold_pct = [double]$PerfThresholds[$thresholdName]
            pass = $true
            reason = ""
        }

        if ($null -eq $baselineValue -or $null -eq $currentValue) {
            $row.pass = $true
            $row.reason = "Skipped(NullValue)"
            $resultRows += [PSCustomObject]$row
            continue
        }

        $baselineDouble = [double]$baselineValue
        $currentDouble = [double]$currentValue
        if ($baselineDouble -le 1e-12) {
            if ($currentDouble -le 1e-12) {
                $row.increase_pct = 0.0
            }
            else {
                $row.increase_pct = [double]::PositiveInfinity
            }
        }
        else {
            $row.increase_pct = (($currentDouble - $baselineDouble) / $baselineDouble) * 100.0
        }

        if ($row.increase_pct -gt [double]$row.threshold_pct) {
            $row.pass = $false
            $row.reason = "IncreaseExceeded"
            $hasFailure = $true
        }
        $resultRows += [PSCustomObject]$row
    }

    return [PSCustomObject]@{
        success = (-not $hasFailure)
        rows = $resultRows
    }
}

function To-PrettyPct {
    param([object]$Value)
    if ($null -eq $Value) { return "N/A" }
    if ($Value -is [double] -and [double]::IsPositiveInfinity($Value)) { return "INF" }
    return ("{0:N2}%" -f [double]$Value)
}

$baselineSummaryPath = Resolve-SummaryPath -InputPath $Baseline
$currentSummaryPath = Resolve-SummaryPath -InputPath $Current
$baselineRun = Load-RunResult -SummaryPath $baselineSummaryPath
$currentRun = Load-RunResult -SummaryPath $currentSummaryPath
$profileData = Get-CompareProfile -ProfilePath $Profile

$repoRoot = (Resolve-Path -LiteralPath ".").Path
$reportRoot = if ([System.IO.Path]::IsPathRooted($ReportOut)) { $ReportOut } else { Join-Path $repoRoot $ReportOut }
New-Item -ItemType Directory -Path $reportRoot -Force | Out-Null
$diffDir = Join-Path $reportRoot "diff"
New-Item -ItemType Directory -Path $diffDir -Force | Out-Null

$allCaseIds = New-Object System.Collections.Generic.HashSet[string]
foreach ($k in $baselineRun.cases.Keys) { [void]$allCaseIds.Add($k) }
foreach ($k in $currentRun.cases.Keys) { [void]$allCaseIds.Add($k) }

$caseResults = @()
$failureCount = 0
$visualFailureCount = 0
$perfFailureCount = 0
$perfSkippedCount = 0
$missingFailureCount = 0

foreach ($caseId in ($allCaseIds | Sort-Object)) {
    $baselineCase = $baselineRun.cases[$caseId]
    $currentCase = $currentRun.cases[$caseId]

    $caseRow = [ordered]@{
        id = $caseId
        baseline_exists = $null -ne $baselineCase
        current_exists = $null -ne $currentCase
        missing_failure = $false
        renderdoc = [ordered]@{
            baseline_path = if ($null -ne $baselineCase) { [string]$baselineCase.renderdoc_capture_path } else { "" }
            baseline_success = if ($null -ne $baselineCase) { [bool]$baselineCase.renderdoc_capture_success } else { $false }
            baseline_retained = if ($null -ne $baselineCase) { [bool]$baselineCase.renderdoc_capture_retained } else { $false }
            baseline_frame_index = if ($null -ne $baselineCase) { [uint64]$baselineCase.renderdoc_capture_frame_index } else { 0 }
            baseline_error = if ($null -ne $baselineCase) { [string]$baselineCase.renderdoc_capture_error } else { "" }
            current_path = if ($null -ne $currentCase) { [string]$currentCase.renderdoc_capture_path } else { "" }
            current_success = if ($null -ne $currentCase) { [bool]$currentCase.renderdoc_capture_success } else { $false }
            current_retained = if ($null -ne $currentCase) { [bool]$currentCase.renderdoc_capture_retained } else { $false }
            current_frame_index = if ($null -ne $currentCase) { [uint64]$currentCase.renderdoc_capture_frame_index } else { 0 }
            current_error = if ($null -ne $currentCase) { [string]$currentCase.renderdoc_capture_error } else { "" }
        }
        pix = [ordered]@{
            baseline_path = if ($null -ne $baselineCase) { [string]$baselineCase.pix_capture_path } else { "" }
            baseline_success = if ($null -ne $baselineCase) { [bool]$baselineCase.pix_capture_success } else { $false }
            baseline_retained = if ($null -ne $baselineCase) { [bool]$baselineCase.pix_capture_retained } else { $false }
            baseline_frame_index = if ($null -ne $baselineCase) { [uint64]$baselineCase.pix_capture_frame_index } else { 0 }
            baseline_error = if ($null -ne $baselineCase) { [string]$baselineCase.pix_capture_error } else { "" }
            current_path = if ($null -ne $currentCase) { [string]$currentCase.pix_capture_path } else { "" }
            current_success = if ($null -ne $currentCase) { [bool]$currentCase.pix_capture_success } else { $false }
            current_retained = if ($null -ne $currentCase) { [bool]$currentCase.pix_capture_retained } else { $false }
            current_frame_index = if ($null -ne $currentCase) { [uint64]$currentCase.pix_capture_frame_index } else { 0 }
            current_error = if ($null -ne $currentCase) { [string]$currentCase.pix_capture_error } else { "" }
        }
        visual = $null
        perf = $null
        pass = $true
    }

    if ($null -eq $baselineCase -or $null -eq $currentCase) {
        $caseRow.pass = (-not $FailOnMissingCase)
        $caseRow.missing_failure = $FailOnMissingCase.IsPresent
        if ($caseRow.missing_failure) {
            $missingFailureCount++
            $failureCount++
        }
        $caseResults += [PSCustomObject]$caseRow
        continue
    }

    if (-not $DisableVisualCompare) {
        if ([string]::IsNullOrWhiteSpace($baselineCase.screenshot_path) -or
            [string]::IsNullOrWhiteSpace($currentCase.screenshot_path) -or
            -not (Test-Path -LiteralPath $baselineCase.screenshot_path) -or
            -not (Test-Path -LiteralPath $currentCase.screenshot_path)) {
            $visual = [ordered]@{
                compared = $false
                pass = $false
                reason = "MissingScreenshot"
                mae = $null
                rmse = $null
                psnr = $null
                diff_path = ""
            }
            $caseRow.visual = [PSCustomObject]$visual
            $caseRow.pass = $false
            $visualFailureCount++
            $failureCount++
        }
        else {
            $diffPath = Join-Path $diffDir ("{0}_absdiff.png" -f $caseId)
            $imageResult = Compare-Images -BaselinePath $baselineCase.screenshot_path -CurrentPath $currentCase.screenshot_path -DiffOutPath $diffPath
            $visualPass = $false
            $reason = ""
            if (-not $imageResult.success) {
                $visualPass = $false
                $reason = [string]$imageResult.reason
            }
            else {
                $visualPass = ($imageResult.mae -le $profileData.visual.mae_max) -and
                              ($imageResult.rmse -le $profileData.visual.rmse_max) -and
                              ($imageResult.psnr -ge $profileData.visual.psnr_min)
                if (-not $visualPass) {
                    $reason = "MetricThresholdExceeded"
                }
            }

            $visual = [ordered]@{
                compared = $true
                pass = $visualPass
                reason = $reason
                mae = $imageResult.mae
                rmse = $imageResult.rmse
                psnr = $imageResult.psnr
                diff_path = $imageResult.diff_path
            }
            $caseRow.visual = [PSCustomObject]$visual
            if (-not $visualPass) {
                $caseRow.pass = $false
                $visualFailureCount++
                $failureCount++
            }
        }
    }

    $skipPerfForRenderDoc = ($null -ne $baselineCase -and [bool]$baselineCase.renderdoc_capture_success) -or
        ($null -ne $currentCase -and [bool]$currentCase.renderdoc_capture_success)
    $skipPerfForPIX = ($null -ne $baselineCase -and [bool]$baselineCase.pix_capture_success) -or
        ($null -ne $currentCase -and [bool]$currentCase.pix_capture_success)
    $perfSkipReason = ""
    if ($skipPerfForRenderDoc -and $skipPerfForPIX) {
        $perfSkipReason = "SkippedForGPUCapture"
    }
    elseif ($skipPerfForRenderDoc) {
        $perfSkipReason = "SkippedForRenderDocCapture"
    }
    elseif ($skipPerfForPIX) {
        $perfSkipReason = "SkippedForPIXCapture"
    }

    if (-not $DisablePerfCompare) {
        if (-not [string]::IsNullOrWhiteSpace($perfSkipReason)) {
            $perf = [ordered]@{
                compared = $false
                skipped = $true
                pass = $true
                reason = $perfSkipReason
                rows = @()
            }
            $caseRow.perf = [PSCustomObject]$perf
            $perfSkippedCount++
        }
        else {
            $baselinePerf = Load-PerfJson -Path $baselineCase.perf_json_path
            $currentPerf = Load-PerfJson -Path $currentCase.perf_json_path

            if ($null -eq $baselinePerf -or $null -eq $currentPerf) {
                $perf = [ordered]@{
                    compared = $false
                    skipped = $false
                    pass = $false
                    reason = "MissingPerfJson"
                    rows = @()
                }
                $caseRow.perf = [PSCustomObject]$perf
                $caseRow.pass = $false
                $perfFailureCount++
                $failureCount++
            }
            else {
                $perfResult = Compare-PerfMetrics -BaselinePerf $baselinePerf -CurrentPerf $currentPerf -PerfThresholds $profileData.perf
                $perf = [ordered]@{
                    compared = $true
                    skipped = $false
                    pass = [bool]$perfResult.success
                    reason = if ($perfResult.success) { "" } else { "PerfThresholdExceeded" }
                    rows = $perfResult.rows
                }
                $caseRow.perf = [PSCustomObject]$perf
                if (-not $perfResult.success) {
                    $caseRow.pass = $false
                    $perfFailureCount++
                    $failureCount++
                }
            }
        }
    }

    $caseResults += [PSCustomObject]$caseRow
}

$summaryObject = [ordered]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    baseline_summary = $baselineSummaryPath
    current_summary = $currentSummaryPath
    report_root = $reportRoot
    profile = $profileData
    case_total = $caseResults.Count
    failure_total = $failureCount
    visual_failure_total = $visualFailureCount
    perf_failure_total = $perfFailureCount
    perf_skipped_total = $perfSkippedCount
    missing_failure_total = $missingFailureCount
    pass = ($failureCount -eq 0)
    cases = $caseResults
}

$summaryJsonPath = Join-Path $reportRoot "summary.json"
$summaryMdPath = Join-Path $reportRoot "summary.md"

$summaryObject | ConvertTo-Json -Depth 16 | Out-File -LiteralPath $summaryJsonPath -Encoding utf8

$md = New-Object System.Collections.Generic.List[string]
$md.Add("# Regression Compare Summary")
$md.Add("")
$md.Add("- Baseline: $Baseline")
$md.Add("- Current: $Current")
$md.Add("- Pass: " + ($(if ($summaryObject.pass) { "YES" } else { "NO" })))
$md.Add("- Case Total: $($summaryObject.case_total)")
$md.Add("- Failures: $($summaryObject.failure_total) (visual=$visualFailureCount perf=$perfFailureCount perf-skipped=$perfSkippedCount missing=$missingFailureCount)")
$md.Add("")
$md.Add("| Case | Pass | Visual | Perf | Notes |")
$md.Add("|---|---|---|---|---|")
foreach ($case in $caseResults) {
    $visualStatus = if ($null -eq $case.visual) { "N/A" } elseif ($case.visual.pass) { "PASS" } else { "FAIL" }
    $perfStatus = if ($null -eq $case.perf) { "N/A" } elseif ($case.perf.skipped) { "SKIP" } elseif ($case.perf.pass) { "PASS" } else { "FAIL" }
    $notes = @()
    if ($case.visual -and -not $case.visual.pass) {
        $notes += ("visual: " + $case.visual.reason)
        if ($case.visual.compared) {
            $notes += ("mae=" + ("{0:N5}" -f [double]$case.visual.mae))
            $notes += ("rmse=" + ("{0:N5}" -f [double]$case.visual.rmse))
            $notes += ("psnr=" + ("{0:N2}" -f [double]$case.visual.psnr))
        }
    }
    if ($case.perf -and $case.perf.skipped) {
        if ($case.perf.reason -eq "SkippedForPIXCapture") {
            $notes += "perf: skipped-pix"
        }
        elseif ($case.perf.reason -eq "SkippedForGPUCapture") {
            $notes += "perf: skipped-gpu-capture"
        }
        else {
            $notes += "perf: skipped-renderdoc"
        }
    }
    elseif ($case.perf -and -not $case.perf.pass) {
        $failedMetrics = @($case.perf.rows | Where-Object { -not $_.pass } | ForEach-Object { "$($_.metric)=" + (To-PrettyPct -Value $_.increase_pct) })
        if ($failedMetrics.Count -gt 0) {
            $notes += ("perf: " + ($failedMetrics -join ", "))
        }
        else {
            $notes += "perf: " + $case.perf.reason
        }
    }
    if ($case.missing_failure) {
        $notes += "missing case"
    }
    if ($case.renderdoc.current_success -and -not [string]::IsNullOrWhiteSpace($case.renderdoc.current_path)) {
        $notes += "rdc"
    } elseif ($case.renderdoc.current_success -and -not $case.renderdoc.current_retained) {
        $notes += "rdc-pruned"
    } elseif (-not [string]::IsNullOrWhiteSpace($case.renderdoc.current_error)) {
        $notes += "rdc-error"
    }
    if ($case.pix.current_success -and -not [string]::IsNullOrWhiteSpace($case.pix.current_path)) {
        $notes += "wpix"
    } elseif ($case.pix.current_success -and -not $case.pix.current_retained) {
        $notes += "wpix-pruned"
    } elseif (-not [string]::IsNullOrWhiteSpace($case.pix.current_error)) {
        $notes += "wpix-error"
    }
    $md.Add("| $($case.id) | $(if($case.pass){'PASS'}else{'FAIL'}) | $visualStatus | $perfStatus | $($notes -join '; ') |")
}

$md | Out-File -LiteralPath $summaryMdPath -Encoding utf8

Write-Host ("STATUS={0}" -f ($(if ($summaryObject.pass) { "ComparePassed" } else { "CompareFailed" })))
Write-Host "CASES=$($summaryObject.case_total)"
Write-Host "FAILURES=$($summaryObject.failure_total)"
Write-Host "VISUAL_FAILURES=$visualFailureCount"
Write-Host "PERF_FAILURES=$perfFailureCount"
Write-Host "PERF_SKIPPED=$perfSkippedCount"
Write-Host "MISSING_FAILURES=$missingFailureCount"
Write-Host "SUMMARY_JSON=$summaryJsonPath"
Write-Host "SUMMARY_MD=$summaryMdPath"
Write-Host "DIFF_DIR=$diffDir"

if ($summaryObject.pass) {
    exit 0
}
exit 1
