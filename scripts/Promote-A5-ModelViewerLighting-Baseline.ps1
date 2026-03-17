param(
    [Parameter(Mandatory = $true)]
    [string]$Validation,
    [string]$Label = "",
    [string]$BaselineRoot = "build_logs\\regression_baselines\\a5_model_viewer_lighting",
    [ValidateSet("baseline", "current")]
    [string]$RunKind = "baseline"
)

function Resolve-RepoPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $PathValue))
}

function Resolve-ValidationSummaryPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$InputPath
    )

    $resolved = Resolve-RepoPath -RepoRoot $RepoRoot -PathValue $InputPath
    if (-not (Test-Path -LiteralPath $resolved)) {
        throw "Validation path not found: $resolved"
    }

    $item = Get-Item -LiteralPath $resolved
    if ($item.PSIsContainer) {
        $summaryPath = Join-Path $item.FullName "summary.json"
        if (-not (Test-Path -LiteralPath $summaryPath)) {
            throw "summary.json not found under validation directory: $resolved"
        }

        return $summaryPath
    }

    if ([string]::Equals($item.Name, "summary.json", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $item.FullName
    }

    throw "Validation input must be a validation directory or summary.json file: $resolved"
}

function Copy-FileIfPresent {
    param(
        [string]$SourcePath,
        [string]$DestinationPath
    )

    if ([string]::IsNullOrWhiteSpace($SourcePath)) {
        return $false
    }

    if (-not (Test-Path -LiteralPath $SourcePath)) {
        return $false
    }

    Copy-Item -LiteralPath $SourcePath -Destination $DestinationPath -Force
    return $true
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$summaryPath = Resolve-ValidationSummaryPath -RepoRoot $repoRoot -InputPath $Validation
$summaryDir = Split-Path -Parent $summaryPath
$summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json

if ([string]$summary.status -ne "ValidationPassed") {
    throw "Validation summary is not passing: $summaryPath"
}

if ([string]::IsNullOrWhiteSpace($Label)) {
    $Label = Get-Date -Format "yyyyMMdd_HHmmss"
}

$baselineRootPath = Resolve-RepoPath -RepoRoot $repoRoot -PathValue $BaselineRoot
$promotionRoot = Join-Path $baselineRootPath $Label
$null = New-Item -ItemType Directory -Path $promotionRoot -Force

$manifest = [ordered]@{
    created_utc = [DateTime]::UtcNow.ToString("o")
    label = $Label
    suite = [string]$summary.suite
    source_validation_summary = $summaryPath
    source_validation_root = $summaryDir
    run_kind = $RunKind
    status = "BaselinePromoted"
    backends = @()
}

$validationSummaryJsonCopy = Join-Path $promotionRoot "validation.summary.json"
$validationSummaryMdCopy = Join-Path $promotionRoot "validation.summary.md"
Copy-Item -LiteralPath $summaryPath -Destination $validationSummaryJsonCopy -Force
[void](Copy-FileIfPresent -SourcePath (Join-Path $summaryDir "summary.md") -DestinationPath $validationSummaryMdCopy)

foreach ($result in $summary.results) {
    $backend = [string]$result.backend
    $sourceEntry = if ($RunKind -eq "baseline") { $result.baseline } else { $result.current }
    $sourceRunDir = [string]$sourceEntry.run_dir
    $sourceSummary = [string]$sourceEntry.summary

    if ([string]::IsNullOrWhiteSpace($sourceRunDir) -or -not (Test-Path -LiteralPath $sourceRunDir)) {
        throw "Run directory missing for backend '$backend': $sourceRunDir"
    }

    $backendRoot = Join-Path $promotionRoot $backend
    $null = New-Item -ItemType Directory -Path $backendRoot -Force

    $runDirName = Split-Path -Leaf $sourceRunDir
    $destRunDir = Join-Path $backendRoot $runDirName
    Copy-Item -LiteralPath $sourceRunDir -Destination $backendRoot -Recurse -Force

    $manifest.backends += [ordered]@{
        backend = $backend
        run_dir = $destRunDir
        summary = Join-Path $destRunDir "suite_result.json"
        source_run_dir = $sourceRunDir
        source_summary = $sourceSummary
        compare_status = [string]$result.compare.status
    }
}

$manifestPath = Join-Path $promotionRoot "baseline_manifest.json"
$latestPath = Join-Path $baselineRootPath "latest.json"

$manifestJson = $manifest | ConvertTo-Json -Depth 8
[System.IO.File]::WriteAllText($manifestPath, $manifestJson, [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText($latestPath, $manifestJson, [System.Text.UTF8Encoding]::new($false))

Write-Host "STATUS=BaselinePromoted"
Write-Host "LABEL=$Label"
Write-Host "RUN_KIND=$RunKind"
Write-Host "VALIDATION_SUMMARY=$summaryPath"
Write-Host "BASELINE_ROOT=$baselineRootPath"
Write-Host "PROMOTION_ROOT=$promotionRoot"
Write-Host "MANIFEST=$manifestPath"
Write-Host "LATEST=$latestPath"
