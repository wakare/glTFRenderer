param(
    [string]$DxManifestPath,
    [string]$VkManifestPath,
    [string]$OutputDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build_logs\perf_compare')
)

$ErrorActionPreference = 'Stop'

function Resolve-SummaryDocument {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    $resolvedPath = (Resolve-Path $PathValue).Path
    $json = Get-Content $resolvedPath -Raw | ConvertFrom-Json

    if ($json.PSObject.Properties.Name -contains 'summary_path') {
        $summaryPath = $json.summary_path
        if (-not (Test-Path $summaryPath)) {
            throw "Summary path '$summaryPath' does not exist."
        }

        return [ordered]@{
            manifest_path = $resolvedPath
            summary_path = (Resolve-Path $summaryPath).Path
            summary = (Get-Content $summaryPath -Raw | ConvertFrom-Json)
        }
    }

    return [ordered]@{
        manifest_path = $null
        summary_path = $resolvedPath
        summary = $json
    }
}

function Resolve-CaseArtifactPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$SummaryPath,
        [string]$ArtifactPath
    )

    if (-not $ArtifactPath) {
        return $null
    }
    if ([System.IO.Path]::IsPathRooted($ArtifactPath)) {
        return $ArtifactPath
    }

    $summaryDir = Split-Path $SummaryPath -Parent
    $candidate = Join-Path $summaryDir $ArtifactPath
    if (Test-Path $candidate) {
        return (Resolve-Path $candidate).Path
    }

    $repoCandidate = Join-Path $RepoRoot $ArtifactPath
    if (Test-Path $repoCandidate) {
        return (Resolve-Path $repoCandidate).Path
    }

    return $ArtifactPath
}

function Get-NumberOrNull {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$JsonObject,
        [Parameter(Mandatory = $true)]
        [string]$PropertyName
    )

    if (-not ($JsonObject.PSObject.Properties.Name -contains $PropertyName)) {
        return $null
    }

    $value = $JsonObject.$PropertyName
    if ($null -eq $value) {
        return $null
    }

    return [double]$value
}

function Compare-Metric {
    param(
        [string]$MetricKey,
        [double]$DxValue,
        [double]$VkValue
    )

    $deltaMs = $DxValue - $VkValue
    $deltaPct = $null
    if ([Math]::Abs($VkValue) -gt 1e-6) {
        $deltaPct = ($deltaMs / $VkValue) * 100.0
    }

    return [ordered]@{
        metric = $MetricKey
        dx12 = [Math]::Round($DxValue, 4)
        vulkan = [Math]::Round($VkValue, 4)
        delta_ms = [Math]::Round($deltaMs, 4)
        delta_pct_vs_vulkan = if ($null -ne $deltaPct) { [Math]::Round($deltaPct, 2) } else { $null }
    }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$dxDoc = Resolve-SummaryDocument -PathValue $DxManifestPath
$vkDoc = Resolve-SummaryDocument -PathValue $VkManifestPath

$metrics = @(
    'cpu_total_avg_ms',
    'gpu_total_avg_ms',
    'frame_total_avg_ms',
    'execute_passes_avg_ms',
    'non_pass_cpu_avg_ms',
    'frame_wait_total_avg_ms',
    'wait_previous_frame_avg_ms',
    'acquire_command_list_avg_ms',
    'acquire_swapchain_avg_ms',
    'execution_planning_avg_ms',
    'blit_to_swapchain_avg_ms',
    'submit_command_list_avg_ms',
    'present_call_avg_ms',
    'prepare_frame_avg_ms',
    'finalize_submission_avg_ms',
    'frosted_cpu_avg_ms',
    'frosted_gpu_avg_ms'
)

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$jsonReportPath = Join-Path $OutputDir "comparison_$stamp.json"
$mdReportPath = Join-Path $OutputDir "comparison_$stamp.md"

$dxCases = @{}
foreach ($caseItem in $dxDoc.summary.cases) {
    $dxCases[$caseItem.id] = $caseItem
}

$vkCases = @{}
foreach ($caseItem in $vkDoc.summary.cases) {
    $vkCases[$caseItem.id] = $caseItem
}

$commonCaseIds = @($dxCases.Keys | Where-Object { $vkCases.ContainsKey($_) } | Sort-Object)
$caseComparisons = @()
$aggregateSums = @{}
$aggregateCounts = @{}
foreach ($metric in $metrics) {
    $aggregateSums[$metric] = [ordered]@{
        dx12 = 0.0
        vulkan = 0.0
    }
    $aggregateCounts[$metric] = 0
}

foreach ($caseId in $commonCaseIds) {
    $dxCase = $dxCases[$caseId]
    $vkCase = $vkCases[$caseId]
    $dxPerfPath = Resolve-CaseArtifactPath -RepoRoot $repoRoot -SummaryPath $dxDoc.summary_path -ArtifactPath $dxCase.perf_json_path
    $vkPerfPath = Resolve-CaseArtifactPath -RepoRoot $repoRoot -SummaryPath $vkDoc.summary_path -ArtifactPath $vkCase.perf_json_path

    $dxPerf = if ($dxPerfPath -and (Test-Path $dxPerfPath)) { Get-Content $dxPerfPath -Raw | ConvertFrom-Json } else { $null }
    $vkPerf = if ($vkPerfPath -and (Test-Path $vkPerfPath)) { Get-Content $vkPerfPath -Raw | ConvertFrom-Json } else { $null }

    $metricComparisons = @()
    if ($dxPerf -and $vkPerf) {
        foreach ($metric in $metrics) {
            $dxValue = Get-NumberOrNull -JsonObject $dxPerf -PropertyName $metric
            $vkValue = Get-NumberOrNull -JsonObject $vkPerf -PropertyName $metric
            if ($null -eq $dxValue -or $null -eq $vkValue) {
                continue
            }

            $metricComparison = Compare-Metric -MetricKey $metric -DxValue $dxValue -VkValue $vkValue
            $metricComparisons += [pscustomobject]$metricComparison

            $aggregateSums[$metric].dx12 += $dxValue
            $aggregateSums[$metric].vulkan += $vkValue
            $aggregateCounts[$metric] += 1
        }
    }

    $caseComparisons += [pscustomobject]@{
        case_id = $caseId
        dx12_perf_path = $dxPerfPath
        vulkan_perf_path = $vkPerfPath
        metrics = $metricComparisons
    }
}

$aggregateMetrics = @()
foreach ($metric in $metrics) {
    if ($aggregateCounts[$metric] -le 0) {
        continue
    }

    $aggregateMetrics += [pscustomobject](Compare-Metric `
        -MetricKey $metric `
        -DxValue ($aggregateSums[$metric].dx12 / $aggregateCounts[$metric]) `
        -VkValue ($aggregateSums[$metric].vulkan / $aggregateCounts[$metric]))
}

$largestDeltas = $aggregateMetrics |
    Sort-Object { [Math]::Abs($_.delta_ms) } -Descending |
    Select-Object -First 5

$heuristics = @()
$waitMetric = $aggregateMetrics | Where-Object { $_.metric -eq 'frame_wait_total_avg_ms' } | Select-Object -First 1
$gpuMetric = $aggregateMetrics | Where-Object { $_.metric -eq 'gpu_total_avg_ms' } | Select-Object -First 1
$passMetric = $aggregateMetrics | Where-Object { $_.metric -eq 'execute_passes_avg_ms' } | Select-Object -First 1

if ($waitMetric -and $passMetric -and ([Math]::Abs($waitMetric.delta_ms) -gt 0.2) -and ([Math]::Abs($waitMetric.delta_ms) -gt [Math]::Abs($passMetric.delta_ms))) {
    if ($waitMetric.delta_ms -gt 0) {
        $heuristics += 'DX12 is slower mainly in frame wait time, which points to framework-side synchronization or present pacing overhead.'
    }
    elseif ($waitMetric.delta_ms -lt 0) {
        $heuristics += 'Vulkan is slower mainly in frame wait time, which points to synchronization or present pacing overhead outside the pass body.'
    }
}

if ($gpuMetric -and ([Math]::Abs($gpuMetric.delta_ms) -gt 0.2)) {
    if ($gpuMetric.delta_ms -gt 0) {
        $heuristics += 'DX12 also shows higher GPU time, so the gap is not purely a CPU-side present/wait artifact.'
    }
    elseif ($gpuMetric.delta_ms -lt 0) {
        $heuristics += 'Vulkan shows higher GPU time, so the backend or shader path still deserves inspection.'
    }
}

$report = [ordered]@{
    generated_at = (Get-Date).ToString('s')
    dx12_summary_path = $dxDoc.summary_path
    vulkan_summary_path = $vkDoc.summary_path
    suite_name_dx12 = $dxDoc.summary.suite_name
    suite_name_vulkan = $vkDoc.summary.suite_name
    dx12_present_mode = $dxDoc.summary.present_mode
    vulkan_present_mode = $vkDoc.summary.present_mode
    common_case_count = $commonCaseIds.Count
    aggregate_metrics = $aggregateMetrics
    largest_deltas = $largestDeltas
    heuristics = $heuristics
    cases = $caseComparisons
}

$report | ConvertTo-Json -Depth 8 | Out-File -FilePath $jsonReportPath -Encoding utf8

$mdLines = New-Object System.Collections.Generic.List[string]
$mdLines.Add('# RendererDemo DX12 vs Vulkan Comparison')
$mdLines.Add('')
$mdLines.Add(('- DX12 summary: `{0}`' -f $dxDoc.summary_path))
$mdLines.Add(('- Vulkan summary: `{0}`' -f $vkDoc.summary_path))
$mdLines.Add(('- Common cases: {0}' -f $commonCaseIds.Count))
$mdLines.Add(('- Present mode: DX12=`{0}`, Vulkan=`{1}`' -f $dxDoc.summary.present_mode, $vkDoc.summary.present_mode))
$mdLines.Add('')

if ($heuristics.Count -gt 0) {
    $mdLines.Add('## Heuristics')
    foreach ($heuristic in $heuristics) {
        $mdLines.Add(('- {0}' -f $heuristic))
    }
    $mdLines.Add('')
}

$mdLines.Add('## Aggregate Metrics')
$mdLines.Add('')
$mdLines.Add('| Metric | DX12 (ms) | Vulkan (ms) | Delta (ms) | Delta % vs Vulkan |')
$mdLines.Add('| --- | ---: | ---: | ---: | ---: |')
foreach ($metric in $aggregateMetrics) {
    $deltaPctText = if ($null -ne $metric.delta_pct_vs_vulkan) { '{0:N2}%' -f $metric.delta_pct_vs_vulkan } else { 'n/a' }
    $dxText = '{0:N4}' -f $metric.dx12
    $vkText = '{0:N4}' -f $metric.vulkan
    $deltaText = '{0:N4}' -f $metric.delta_ms
    $mdLines.Add(('| {0} | {1} | {2} | {3} | {4} |' -f $metric.metric, $dxText, $vkText, $deltaText, $deltaPctText))
}
$mdLines.Add('')

foreach ($caseComparison in $caseComparisons) {
    $mdLines.Add(('## Case `{0}`' -f $caseComparison.case_id))
    $mdLines.Add('')
    $mdLines.Add('| Metric | DX12 (ms) | Vulkan (ms) | Delta (ms) | Delta % vs Vulkan |')
    $mdLines.Add('| --- | ---: | ---: | ---: | ---: |')
    foreach ($metric in $caseComparison.metrics) {
        $deltaPctText = if ($null -ne $metric.delta_pct_vs_vulkan) { '{0:N2}%' -f $metric.delta_pct_vs_vulkan } else { 'n/a' }
        $dxText = '{0:N4}' -f $metric.dx12
        $vkText = '{0:N4}' -f $metric.vulkan
        $deltaText = '{0:N4}' -f $metric.delta_ms
        $mdLines.Add(('| {0} | {1} | {2} | {3} | {4} |' -f $metric.metric, $dxText, $vkText, $deltaText, $deltaPctText))
    }
    $mdLines.Add('')
}

$mdLines | Out-File -FilePath $mdReportPath -Encoding utf8

Write-Output "STATUS=SUCCESS"
Write-Output "JSON_REPORT=$jsonReportPath"
Write-Output "MD_REPORT=$mdReportPath"
