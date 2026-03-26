param(
    [string]$NoLightScene = ".verify\\lightingbaker_fixture\\quad_uv1_bounce_nolight.gltf",
    [string]$ExePath = "glTFRenderer\\x64\\Debug\\LightingBaker.exe",
    [string]$OutputBase = "run_artifacts\\lightingbaker_environment_mis_validate",
    [string]$LogDir = "build_logs",
    [int]$TimeoutSec = 300
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-RepoPath {
    param(
        [string]$Path,
        [string]$RepoRoot,
        [switch]$RequireExists
    )

    $candidate = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $RepoRoot $Path }
    if ($RequireExists -and -not (Test-Path -LiteralPath $candidate)) {
        throw "Path does not exist: $candidate"
    }

    return $candidate
}

function Convert-ToRepoRelativePath {
    param(
        [string]$Path,
        [string]$RepoRoot
    )

    $absolutePath = [System.IO.Path]::GetFullPath($Path)
    $absoluteRoot = [System.IO.Path]::GetFullPath($RepoRoot)
    if ($absolutePath.StartsWith($absoluteRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $absolutePath.Substring($absoluteRoot.Length).TrimStart('\', '/')
    }

    return $absolutePath
}

function Get-OptionalProperty {
    param(
        [object]$Object,
        [string]$Name
    )

    if ($null -eq $Object) {
        return $null
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $null
    }

    return $property.Value
}

function Get-OptionalDoubleProperty {
    param(
        [object]$Object,
        [string]$Name
    )

    $value = Get-OptionalProperty -Object $Object -Name $Name
    if ($null -eq $value) {
        return 0.0
    }

    return [double]$value
}

function Get-OptionalIntProperty {
    param(
        [object]$Object,
        [string]$Name
    )

    $value = Get-OptionalProperty -Object $Object -Name $Name
    if ($null -eq $value) {
        return 0
    }

    return [int]$value
}

function Read-JsonFile {
    param([string]$Path)

    return [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}

function Invoke-LightingBakerCase {
    param(
        [string]$CaseName,
        [string]$ScenePath,
        [string]$ExeFullPath,
        [string]$RuntimeRoot,
        [string]$SessionRoot,
        [string]$LogDirPath,
        [string]$RepoRoot,
        [int]$TimeoutSec,
        [int]$MaxBounces,
        [int]$EnvironmentLightSamples
    )

    $caseRoot = Join-Path $SessionRoot $CaseName
    $bakeOutputRoot = Join-Path $caseRoot "bake_output"
    New-Item -ItemType Directory -Path $caseRoot -Force | Out-Null

    $stdoutPath = Join-Path $LogDirPath "$CaseName.stdout.log"
    $stderrPath = Join-Path $LogDirPath "$CaseName.stderr.log"

    $arguments = @(
        "--scene", $ScenePath,
        "--output", $bakeOutputRoot,
        "--resolution", "64",
        "--samples-per-iteration", "96",
        "--target-samples", "96",
        "--max-bounces", ([string]$MaxBounces),
        "--sky-intensity", "1",
        "--sky-ground-mix", "0.35",
        "--direct-light-samples", "1",
        "--environment-light-samples", ([string]$EnvironmentLightSamples),
        "--no-progressive"
    )

    $process = Start-Process -FilePath $ExeFullPath `
        -ArgumentList $arguments `
        -WorkingDirectory $RuntimeRoot `
        -NoNewWindow `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -PassThru

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $timedOut = $false
    if (-not $process.WaitForExit($TimeoutSec * 1000)) {
        $timedOut = $true
        try {
            $process.Kill($true)
        }
        catch {}
    }
    $stopwatch.Stop()
    $process.Refresh()

    $exitCode = if ($timedOut) { -1 } else { [int]$process.ExitCode }
    $dispatchSummaryPath = Join-Path $bakeOutputRoot "debug\\raytracing_dispatch_summary.json"
    $resumePath = Join-Path $bakeOutputRoot "cache\\resume.json"
    $manifestPath = Join-Path $bakeOutputRoot "manifest.json"
    $requiredArtifacts = @(
        $dispatchSummaryPath,
        $resumePath,
        $manifestPath,
        (Join-Path $bakeOutputRoot "atlases\\indirect_00.preview.png")
    )
    $missingArtifacts = @($requiredArtifacts | Where-Object { -not (Test-Path -LiteralPath $_) })

    $environmentLightSampleCount = 0
    $outputLuminanceSum = 0.0
    $outputRgbSum = 0.0
    $nonzeroRgbTexelCount = 0
    $resumeEnvironmentLightSamples = 0
    $manifestEnvironmentLightSamples = 0
    if (Test-Path -LiteralPath $dispatchSummaryPath) {
        $dispatchSummary = Read-JsonFile -Path $dispatchSummaryPath
        $environmentLightSampleCount = [int](Get-OptionalIntProperty -Object $dispatchSummary -Name "environment_light_sample_count")
        $outputLuminanceSum = [double](Get-OptionalDoubleProperty -Object $dispatchSummary -Name "output_luminance_sum")
        $outputRgbSum = [double](Get-OptionalDoubleProperty -Object $dispatchSummary -Name "output_rgb_sum")
        $nonzeroRgbTexelCount = [int](Get-OptionalIntProperty -Object $dispatchSummary -Name "output_nonzero_rgb_texel_count")
    }

    if (Test-Path -LiteralPath $resumePath) {
        $resumeJson = Read-JsonFile -Path $resumePath
        $resumeJob = Get-OptionalProperty -Object $resumeJson -Name "job"
        if ($null -ne $resumeJob) {
            $resumeEnvironmentLightSamples = [int](Get-OptionalIntProperty -Object $resumeJob -Name "environment_light_samples")
        }
    }

    if (Test-Path -LiteralPath $manifestPath) {
        $manifestJson = Read-JsonFile -Path $manifestPath
        $integrator = Get-OptionalProperty -Object $manifestJson -Name "integrator"
        if ($null -ne $integrator) {
            $manifestEnvironmentLightSamples = [int](Get-OptionalIntProperty -Object $integrator -Name "environment_light_samples")
        }
    }

    return [pscustomobject]@{
        case_name = $CaseName
        output_root = Convert-ToRepoRelativePath -Path $bakeOutputRoot -RepoRoot $RepoRoot
        stdout = Convert-ToRepoRelativePath -Path $stdoutPath -RepoRoot $RepoRoot
        stderr = Convert-ToRepoRelativePath -Path $stderrPath -RepoRoot $RepoRoot
        duration_ms = [int]$stopwatch.ElapsedMilliseconds
        exit_code = $exitCode
        timed_out = $timedOut
        max_bounces = $MaxBounces
        requested_environment_light_samples = $EnvironmentLightSamples
        environment_light_sample_count = $environmentLightSampleCount
        resume_environment_light_samples = $resumeEnvironmentLightSamples
        manifest_environment_light_samples = $manifestEnvironmentLightSamples
        output_luminance_sum = $outputLuminanceSum
        output_rgb_sum = $outputRgbSum
        output_nonzero_rgb_texel_count = $nonzeroRgbTexelCount
        missing_artifacts = @($missingArtifacts | ForEach-Object { Convert-ToRepoRelativePath -Path $_ -RepoRoot $RepoRoot })
    }
}

function Test-CaseBaseSuccess {
    param(
        [object]$CaseResult,
        [System.Collections.Generic.List[string]]$Failures
    )

    if ($CaseResult.timed_out) {
        $Failures.Add("$($CaseResult.case_name): process timed out.")
    }

    if ($CaseResult.exit_code -ne 0) {
        $Failures.Add("$($CaseResult.case_name): exit_code=$($CaseResult.exit_code) expected 0.")
    }

    if ($CaseResult.missing_artifacts.Count -ne 0) {
        $Failures.Add("$($CaseResult.case_name): missing artifacts: $($CaseResult.missing_artifacts -join ', ').")
    }

    if ($CaseResult.environment_light_sample_count -ne $CaseResult.requested_environment_light_samples) {
        $Failures.Add("$($CaseResult.case_name): dispatch environment_light_sample_count=$($CaseResult.environment_light_sample_count) expected $($CaseResult.requested_environment_light_samples).")
    }

    if ($CaseResult.resume_environment_light_samples -ne $CaseResult.requested_environment_light_samples) {
        $Failures.Add("$($CaseResult.case_name): resume environment_light_samples=$($CaseResult.resume_environment_light_samples) expected $($CaseResult.requested_environment_light_samples).")
    }

    if ($CaseResult.manifest_environment_light_samples -ne $CaseResult.requested_environment_light_samples) {
        $Failures.Add("$($CaseResult.case_name): manifest environment_light_samples=$($CaseResult.manifest_environment_light_samples) expected $($CaseResult.requested_environment_light_samples).")
    }
}

$repoRoot = (Resolve-Path -LiteralPath ".").Path
$runtimeRoot = Resolve-RepoPath -Path "glTFRenderer" -RepoRoot $repoRoot -RequireExists
$noLightScenePath = Resolve-RepoPath -Path $NoLightScene -RepoRoot $repoRoot -RequireExists
$exeFullPath = Resolve-RepoPath -Path $ExePath -RepoRoot $repoRoot -RequireExists
$outputBasePath = Resolve-RepoPath -Path $OutputBase -RepoRoot $repoRoot
$logDirPath = Resolve-RepoPath -Path $LogDir -RepoRoot $repoRoot
New-Item -ItemType Directory -Path $outputBasePath -Force | Out-Null
New-Item -ItemType Directory -Path $logDirPath -Force | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$sessionRoot = Join-Path $outputBasePath "environment_mis_$timestamp"
$sessionLogDir = Join-Path $logDirPath "lightingbaker_environment_mis_validate_$timestamp"
New-Item -ItemType Directory -Path $sessionRoot -Force | Out-Null
New-Item -ItemType Directory -Path $sessionLogDir -Force | Out-Null

$oneBounceBaseline = Invoke-LightingBakerCase `
    -CaseName "one_bounce_baseline" `
    -ScenePath $noLightScenePath `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -MaxBounces 1 `
    -EnvironmentLightSamples 0

$oneBounceEnvironment = Invoke-LightingBakerCase `
    -CaseName "one_bounce_environment" `
    -ScenePath $noLightScenePath `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -MaxBounces 1 `
    -EnvironmentLightSamples 2

$twoBounceBaseline = Invoke-LightingBakerCase `
    -CaseName "two_bounce_baseline" `
    -ScenePath $noLightScenePath `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -MaxBounces 2 `
    -EnvironmentLightSamples 0

$twoBounceEnvironment = Invoke-LightingBakerCase `
    -CaseName "two_bounce_environment" `
    -ScenePath $noLightScenePath `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -MaxBounces 2 `
    -EnvironmentLightSamples 2

$threeBounceBaseline = Invoke-LightingBakerCase `
    -CaseName "three_bounce_baseline" `
    -ScenePath $noLightScenePath `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -MaxBounces 3 `
    -EnvironmentLightSamples 0

$failures = New-Object 'System.Collections.Generic.List[string]'
foreach ($caseResult in @($oneBounceBaseline, $oneBounceEnvironment, $twoBounceBaseline, $twoBounceEnvironment, $threeBounceBaseline)) {
    Test-CaseBaseSuccess -CaseResult $caseResult -Failures $failures
}

if ($oneBounceBaseline.output_nonzero_rgb_texel_count -le 0 -or $oneBounceEnvironment.output_nonzero_rgb_texel_count -le 0) {
    $failures.Add("one-bounce cases must both produce nonzero rgb texels.")
}

if ($twoBounceBaseline.output_nonzero_rgb_texel_count -le 0 -or
    $twoBounceEnvironment.output_nonzero_rgb_texel_count -le 0 -or
    $threeBounceBaseline.output_nonzero_rgb_texel_count -le 0) {
    $failures.Add("two- and three-bounce cases must all produce nonzero rgb texels.")
}

$oneBounceGainRatio = if ($oneBounceBaseline.output_luminance_sum -gt 1e-6) {
    [double]($oneBounceEnvironment.output_luminance_sum / $oneBounceBaseline.output_luminance_sum)
}
else {
    [double]0.0
}
if ($oneBounceGainRatio -le 1.05) {
    $failures.Add("one_bounce_environment: luminance gain ratio=$oneBounceGainRatio expected > 1.05.")
}

$oneBounceVsTwoBounceLuminanceRelErr = if ($twoBounceBaseline.output_luminance_sum -gt 1e-6) {
    [double]([Math]::Abs($oneBounceEnvironment.output_luminance_sum - $twoBounceBaseline.output_luminance_sum) / $twoBounceBaseline.output_luminance_sum)
}
else {
    [double]0.0
}
if ($oneBounceVsTwoBounceLuminanceRelErr -gt 0.15) {
    $failures.Add("one_bounce_environment: luminance relative error to two_bounce_baseline=$oneBounceVsTwoBounceLuminanceRelErr expected <= 0.15.")
}

$oneBounceVsTwoBounceRgbRelErr = if ([Math]::Abs($twoBounceBaseline.output_rgb_sum) -gt 1e-6) {
    [double]([Math]::Abs($oneBounceEnvironment.output_rgb_sum - $twoBounceBaseline.output_rgb_sum) / [Math]::Abs($twoBounceBaseline.output_rgb_sum))
}
else {
    [double]0.0
}
if ($oneBounceVsTwoBounceRgbRelErr -gt 0.15) {
    $failures.Add("one_bounce_environment: rgb relative error to two_bounce_baseline=$oneBounceVsTwoBounceRgbRelErr expected <= 0.15.")
}

$twoBounceVsThreeBounceLuminanceRelErr = if ($threeBounceBaseline.output_luminance_sum -gt 1e-6) {
    [double]([Math]::Abs($twoBounceEnvironment.output_luminance_sum - $threeBounceBaseline.output_luminance_sum) / $threeBounceBaseline.output_luminance_sum)
}
else {
    [double]0.0
}
if ($twoBounceVsThreeBounceLuminanceRelErr -gt 0.15) {
    $failures.Add("two_bounce_environment: luminance relative error to three_bounce_baseline=$twoBounceVsThreeBounceLuminanceRelErr expected <= 0.15.")
}

$twoBounceVsThreeBounceRgbRelErr = if ([Math]::Abs($threeBounceBaseline.output_rgb_sum) -gt 1e-6) {
    [double]([Math]::Abs($twoBounceEnvironment.output_rgb_sum - $threeBounceBaseline.output_rgb_sum) / [Math]::Abs($threeBounceBaseline.output_rgb_sum))
}
else {
    [double]0.0
}
if ($twoBounceVsThreeBounceRgbRelErr -gt 0.15) {
    $failures.Add("two_bounce_environment: rgb relative error to three_bounce_baseline=$twoBounceVsThreeBounceRgbRelErr expected <= 0.15.")
}

$status = "ValidationFailed"
$pass = $false
if ($failures.Count -eq 0) {
    $status = "ValidationPassed"
    $pass = $true
}
elseif ($oneBounceBaseline.timed_out -or
        $oneBounceEnvironment.timed_out -or
        $twoBounceBaseline.timed_out -or
        $twoBounceEnvironment.timed_out -or
        $threeBounceBaseline.timed_out) {
    $status = "ValidationTimeout"
}

$summaryObject = [pscustomobject]@{
    status = $status
    passed = $pass
    session_root = Convert-ToRepoRelativePath -Path $sessionRoot -RepoRoot $repoRoot
    metrics = [pscustomobject]@{
        one_bounce_gain_ratio = $oneBounceGainRatio
        one_bounce_vs_two_bounce_luminance_relative_error = $oneBounceVsTwoBounceLuminanceRelErr
        one_bounce_vs_two_bounce_rgb_relative_error = $oneBounceVsTwoBounceRgbRelErr
        two_bounce_vs_three_bounce_luminance_relative_error = $twoBounceVsThreeBounceLuminanceRelErr
        two_bounce_vs_three_bounce_rgb_relative_error = $twoBounceVsThreeBounceRgbRelErr
    }
    results = @(
        $oneBounceBaseline,
        $oneBounceEnvironment,
        $twoBounceBaseline,
        $twoBounceEnvironment,
        $threeBounceBaseline
    )
    failures = @($failures)
}

$summaryJsonPath = Join-Path $sessionRoot "summary.json"
$summaryObject | ConvertTo-Json -Depth 16 | Out-File -LiteralPath $summaryJsonPath -Encoding utf8

Write-Host "STATUS=$status"
Write-Host "SUMMARY_JSON=$(Convert-ToRepoRelativePath -Path $summaryJsonPath -RepoRoot $repoRoot)"
Write-Host "ONE_BOUNCE_BASELINE_OUTPUT=$($oneBounceBaseline.output_root)"
Write-Host "ONE_BOUNCE_ENVIRONMENT_OUTPUT=$($oneBounceEnvironment.output_root)"
Write-Host "TWO_BOUNCE_BASELINE_OUTPUT=$($twoBounceBaseline.output_root)"
Write-Host "TWO_BOUNCE_ENVIRONMENT_OUTPUT=$($twoBounceEnvironment.output_root)"
Write-Host "THREE_BOUNCE_BASELINE_OUTPUT=$($threeBounceBaseline.output_root)"
