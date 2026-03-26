param(
    [string]$NoLightScene = ".verify\\lightingbaker_fixture\\quad_uv1_bounce_nolight.gltf",
    [string]$PointLightScene = ".verify\\lightingbaker_fixture\\quad_uv1_bounce_pointlight.gltf",
    [string]$ExePath = "glTFRenderer\\x64\\Debug\\LightingBaker.exe",
    [string]$OutputBase = "run_artifacts\\lightingbaker_direct_light_sampling_validate",
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

    if ([System.IO.Path]::IsPathRooted($Path)) {
        $candidate = $Path
    }
    else {
        $candidate = Join-Path $RepoRoot $Path
    }

    $candidate = [System.IO.Path]::GetFullPath($candidate)
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

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $repoRootFull = [System.IO.Path]::GetFullPath($RepoRoot)
    if ($fullPath.StartsWith($repoRootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $fullPath.Substring($repoRootFull.Length).TrimStart('\', '/')
    }

    return $fullPath
}

function Stop-ProcessTree {
    param([int]$ParentProcessId)

    try {
        $children = Get-CimInstance Win32_Process -Filter "ParentProcessId = $ParentProcessId"
    }
    catch {
        $children = @()
    }

    foreach ($child in $children) {
        Stop-ProcessTree -ParentProcessId $child.ProcessId
    }

    try {
        Stop-Process -Id $ParentProcessId -Force -ErrorAction SilentlyContinue
    }
    catch {}
}

function Set-ObjectProperty {
    param(
        [object]$Object,
        [string]$Name,
        [object]$Value
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        $Object | Add-Member -NotePropertyName $Name -NotePropertyValue $Value
    }
    else {
        $property.Value = $Value
    }
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

function Get-OptionalIntProperty {
    param(
        [object]$Object,
        [string]$Name
    )

    $value = Get-OptionalProperty -Object $Object -Name $Name
    if ($null -eq $value) {
        return $null
    }

    return [int]$value
}

function Get-OptionalDoubleProperty {
    param(
        [object]$Object,
        [string]$Name
    )

    $value = Get-OptionalProperty -Object $Object -Name $Name
    if ($null -eq $value) {
        return $null
    }

    return [double]$value
}

function Read-JsonFile {
    param([string]$Path)

    return [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}

function Test-FileBytesEqual {
    param(
        [string]$LeftPath,
        [string]$RightPath
    )

    if (-not (Test-Path -LiteralPath $LeftPath) -or -not (Test-Path -LiteralPath $RightPath)) {
        return $false
    }

    $leftBytes = [System.IO.File]::ReadAllBytes($LeftPath)
    $rightBytes = [System.IO.File]::ReadAllBytes($RightPath)
    if ($leftBytes.Length -ne $rightBytes.Length) {
        return $false
    }

    for ($index = 0; $index -lt $leftBytes.Length; ++$index) {
        if ($leftBytes[$index] -ne $rightBytes[$index]) {
            return $false
        }
    }

    return $true
}

function New-DuplicatePointLightFixture {
    param(
        [string]$TemplateScenePath,
        [string]$SessionRoot,
        [string]$FixtureFileName,
        [double]$PrimaryIntensityScale = 1.0,
        [double]$DuplicateIntensityScale = 1.0
    )

    $fixtureRoot = Join-Path $SessionRoot "fixture"
    New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null

    $templateSceneDir = Split-Path -Parent $TemplateScenePath
    Get-ChildItem -LiteralPath $templateSceneDir -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $fixtureRoot $_.Name) -Force
    }

    $scene = Read-JsonFile -Path $TemplateScenePath
    $sceneIndex = Get-OptionalIntProperty -Object $scene -Name "scene"
    if ($null -eq $sceneIndex) {
        $sceneIndex = 0
    }

    $extensions = Get-OptionalProperty -Object $scene -Name "extensions"
    if ($null -eq $extensions) {
        throw "Template scene does not define the glTF root extensions object."
    }

    $punctual = Get-OptionalProperty -Object $extensions -Name "KHR_lights_punctual"
    if ($null -eq $punctual) {
        throw "Template scene is missing KHR_lights_punctual."
    }

    $lights = @($(Get-OptionalProperty -Object $punctual -Name "lights"))
    if ($lights.Count -eq 0) {
        throw "Template scene does not contain any punctual lights."
    }

    $baseIntensity = Get-OptionalDoubleProperty -Object $lights[0] -Name "intensity"
    if ($null -eq $baseIntensity) {
        $baseIntensity = 1.0
    }

    $baseLight = ($lights[0] | ConvertTo-Json -Depth 64 | ConvertFrom-Json)
    $baseLightName = Get-OptionalProperty -Object $baseLight -Name "name"
    if ([string]::IsNullOrWhiteSpace([string]$baseLightName)) {
        Set-ObjectProperty -Object $baseLight -Name "name" -Value "BouncePointDuplicate"
    }
    else {
        Set-ObjectProperty -Object $baseLight -Name "name" -Value ([string]$baseLightName + "_Duplicate")
    }

    Set-ObjectProperty -Object $lights[0] -Name "intensity" -Value ([double]($baseIntensity * $PrimaryIntensityScale))
    Set-ObjectProperty -Object $baseLight -Name "intensity" -Value ([double]($baseIntensity * $DuplicateIntensityScale))

    $lights += $baseLight
    Set-ObjectProperty -Object $punctual -Name "lights" -Value $lights

    $nodes = @($(Get-OptionalProperty -Object $scene -Name "nodes"))
    if ($nodes.Count -eq 0) {
        throw "Template scene does not contain any nodes."
    }

    $baseLightNodeIndex = -1
    for ($index = 0; $index -lt $nodes.Count; ++$index) {
        $nodeExtensions = Get-OptionalProperty -Object $nodes[$index] -Name "extensions"
        $nodePunctual = Get-OptionalProperty -Object $nodeExtensions -Name "KHR_lights_punctual"
        $nodeLightIndex = Get-OptionalIntProperty -Object $nodePunctual -Name "light"
        if ($null -ne $nodeLightIndex -and $nodeLightIndex -eq 0) {
            $baseLightNodeIndex = $index
            break
        }
    }

    if ($baseLightNodeIndex -lt 0) {
        throw "Template scene does not contain a node that references punctual light 0."
    }

    $duplicateNode = ($nodes[$baseLightNodeIndex] | ConvertTo-Json -Depth 64 | ConvertFrom-Json)
    $duplicateNodeName = Get-OptionalProperty -Object $duplicateNode -Name "name"
    if ([string]::IsNullOrWhiteSpace([string]$duplicateNodeName)) {
        Set-ObjectProperty -Object $duplicateNode -Name "name" -Value "BouncePointLightDuplicate"
    }
    else {
        Set-ObjectProperty -Object $duplicateNode -Name "name" -Value ([string]$duplicateNodeName + "_Duplicate")
    }

    $duplicateExtensions = Get-OptionalProperty -Object $duplicateNode -Name "extensions"
    $duplicatePunctual = Get-OptionalProperty -Object $duplicateExtensions -Name "KHR_lights_punctual"
    if ($null -eq $duplicatePunctual) {
        throw "Duplicate node is missing KHR_lights_punctual."
    }

    Set-ObjectProperty -Object $duplicatePunctual -Name "light" -Value ($lights.Count - 1)
    $nodes += $duplicateNode
    Set-ObjectProperty -Object $scene -Name "nodes" -Value $nodes
    $newNodeIndex = $nodes.Count - 1

    $scenes = @($(Get-OptionalProperty -Object $scene -Name "scenes"))
    if ($sceneIndex -lt 0 -or $sceneIndex -ge $scenes.Count) {
        throw "Template scene index is out of range."
    }

    $sceneEntry = $scenes[$sceneIndex]
    $sceneNodes = @($(Get-OptionalProperty -Object $sceneEntry -Name "nodes"))
    $sceneNodes += $newNodeIndex
    Set-ObjectProperty -Object $sceneEntry -Name "nodes" -Value $sceneNodes

    $fixtureScenePath = Join-Path $fixtureRoot $FixtureFileName
    $sceneJson = $scene | ConvertTo-Json -Depth 100
    [System.IO.File]::WriteAllText($fixtureScenePath, $sceneJson, [System.Text.UTF8Encoding]::new($false))

    return [pscustomobject]@{
        scene_path = $fixtureScenePath
        fixture_root = $fixtureRoot
        base_light_node_index = $baseLightNodeIndex
        duplicated_light_index = $lights.Count - 1
        duplicated_light_node_index = $newNodeIndex
        primary_intensity = [double]($baseIntensity * $PrimaryIntensityScale)
        duplicated_intensity = [double]($baseIntensity * $DuplicateIntensityScale)
    }
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
        [int]$DirectLightSamples
    )

    $caseRoot = Join-Path $SessionRoot $CaseName
    $bakeOutputRoot = Join-Path $caseRoot "bake_output"
    New-Item -ItemType Directory -Path $caseRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $bakeOutputRoot -Force | Out-Null

    $stdoutPath = Join-Path $LogDirPath "$CaseName.stdout.log"
    $stderrPath = Join-Path $LogDirPath "$CaseName.stderr.log"
    $arguments = @(
        "--scene", $ScenePath,
        "--output", $bakeOutputRoot,
        "--samples-per-iteration", "1",
        "--target-samples", "1",
        "--max-bounces", "1",
        "--sky-intensity", "0",
        "--direct-light-samples", ([string]$DirectLightSamples)
    )

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $ExeFullPath `
        -ArgumentList $arguments `
        -WorkingDirectory $RuntimeRoot `
        -NoNewWindow `
        -PassThru `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath

    $timedOut = $false
    if (-not $process.WaitForExit($TimeoutSec * 1000)) {
        $timedOut = $true
        Stop-ProcessTree -ParentProcessId $process.Id
        $process.WaitForExit()
    }

    $stopwatch.Stop()
    $exitCode = if ($timedOut) { -1 } else { [int]$process.ExitCode }

    $manifestPath = Join-Path $bakeOutputRoot "manifest.json"
    $sceneSummaryPath = Join-Path $bakeOutputRoot "debug\\raytracing_scene_summary.json"
    $dispatchSummaryPath = Join-Path $bakeOutputRoot "debug\\raytracing_dispatch_summary.json"
    $resumePath = Join-Path $bakeOutputRoot "cache\\resume.json"
    $atlasMasterPath = Join-Path $bakeOutputRoot "atlases\\indirect_00.master.rgba16f.bin"
    $requiredArtifacts = @(
        $manifestPath,
        $sceneSummaryPath,
        $dispatchSummaryPath,
        $resumePath,
        $atlasMasterPath,
        (Join-Path $bakeOutputRoot "debug\\import_summary.json"),
        (Join-Path $bakeOutputRoot "debug\\raytracing_runtime_summary.json"),
        (Join-Path $bakeOutputRoot "atlases\\indirect_00.preview.png")
    )

    $missingArtifacts = @($requiredArtifacts | Where-Object { -not (Test-Path -LiteralPath $_) })

    $sceneLightCount = $null
    $sceneLightSelectionPdfs = @()
    if (Test-Path -LiteralPath $sceneSummaryPath) {
        $sceneSummary = Read-JsonFile -Path $sceneSummaryPath
        $sceneLightCount = Get-OptionalIntProperty -Object $sceneSummary -Name "scene_light_count"
        $sceneLightSelectionPdfs = @(
            @(Get-OptionalProperty -Object $sceneSummary -Name "scene_lights") | ForEach-Object {
                $selectionPdf = Get-OptionalDoubleProperty -Object $_ -Name "selection_pdf"
                if ($null -eq $selectionPdf) { 0.0 } else { [double]$selectionPdf }
            }
        )
    }

    $dispatchDirectLightSamples = $null
    $outputRgbSum = 0.0
    $outputLuminanceSum = 0.0
    $nonzeroRgbTexelCount = 0
    if (Test-Path -LiteralPath $dispatchSummaryPath) {
        $dispatchSummary = Read-JsonFile -Path $dispatchSummaryPath
        $dispatchDirectLightSamples = Get-OptionalIntProperty -Object $dispatchSummary -Name "direct_light_sample_count"
        $nonzeroRgbTexelCount = Get-OptionalIntProperty -Object $dispatchSummary -Name "output_nonzero_rgb_texel_count"
        if ($null -eq $nonzeroRgbTexelCount) {
            $nonzeroRgbTexelCount = 0
        }

        $rgbSumValue = Get-OptionalDoubleProperty -Object $dispatchSummary -Name "output_rgb_sum"
        if ($null -ne $rgbSumValue) {
            $outputRgbSum = $rgbSumValue
        }

        $luminanceValue = Get-OptionalDoubleProperty -Object $dispatchSummary -Name "output_luminance_sum"
        if ($null -ne $luminanceValue) {
            $outputLuminanceSum = $luminanceValue
        }
    }

    $manifestDirectLightSamples = $null
    if (Test-Path -LiteralPath $manifestPath) {
        $manifest = Read-JsonFile -Path $manifestPath
        $integrator = Get-OptionalProperty -Object $manifest -Name "integrator"
        $manifestDirectLightSamples = Get-OptionalIntProperty -Object $integrator -Name "direct_light_samples"
    }

    $resumeDirectLightSamples = $null
    if (Test-Path -LiteralPath $resumePath) {
        $resumeMetadata = Read-JsonFile -Path $resumePath
        $job = Get-OptionalProperty -Object $resumeMetadata -Name "job"
        $resumeDirectLightSamples = Get-OptionalIntProperty -Object $job -Name "direct_light_samples"
    }

    return [pscustomobject]@{
        case_name = $CaseName
        scene_path = Convert-ToRepoRelativePath -Path $ScenePath -RepoRoot $RepoRoot
        output_root = Convert-ToRepoRelativePath -Path $bakeOutputRoot -RepoRoot $RepoRoot
        atlas_master_path = Convert-ToRepoRelativePath -Path $atlasMasterPath -RepoRoot $RepoRoot
        stdout = Convert-ToRepoRelativePath -Path $stdoutPath -RepoRoot $RepoRoot
        stderr = Convert-ToRepoRelativePath -Path $stderrPath -RepoRoot $RepoRoot
        duration_ms = [int]$stopwatch.ElapsedMilliseconds
        exit_code = $exitCode
        timed_out = $timedOut
        requested_direct_light_samples = $DirectLightSamples
        scene_light_count = $sceneLightCount
        scene_light_selection_pdfs = @($sceneLightSelectionPdfs)
        dispatch_direct_light_sample_count = $dispatchDirectLightSamples
        manifest_direct_light_samples = $manifestDirectLightSamples
        resume_direct_light_samples = $resumeDirectLightSamples
        output_nonzero_rgb_texel_count = $nonzeroRgbTexelCount
        output_rgb_sum = $outputRgbSum
        output_luminance_sum = $outputLuminanceSum
        missing_artifacts = @($missingArtifacts | ForEach-Object { Convert-ToRepoRelativePath -Path $_ -RepoRoot $RepoRoot })
    }
}

function Test-CaseBaseSuccess {
    param(
        [pscustomobject]$CaseResult,
        [System.Collections.Generic.List[string]]$Failures
    )

    if ($CaseResult.timed_out) {
        $Failures.Add("$($CaseResult.case_name): LightingBaker timed out.")
    }

    if ($CaseResult.exit_code -ne 0) {
        $Failures.Add("$($CaseResult.case_name): LightingBaker exited with code $($CaseResult.exit_code).")
    }

    if ($CaseResult.missing_artifacts.Count -gt 0) {
        $Failures.Add("$($CaseResult.case_name): Missing required artifacts: $([string]::Join(', ', $CaseResult.missing_artifacts)).")
    }

    if ($CaseResult.dispatch_direct_light_sample_count -ne $CaseResult.requested_direct_light_samples) {
        $Failures.Add("$($CaseResult.case_name): Dispatch summary direct_light_sample_count=$($CaseResult.dispatch_direct_light_sample_count) expected $($CaseResult.requested_direct_light_samples).")
    }

    if ($CaseResult.manifest_direct_light_samples -ne $CaseResult.requested_direct_light_samples) {
        $Failures.Add("$($CaseResult.case_name): Manifest integrator.direct_light_samples=$($CaseResult.manifest_direct_light_samples) expected $($CaseResult.requested_direct_light_samples).")
    }

    if ($CaseResult.resume_direct_light_samples -ne $CaseResult.requested_direct_light_samples) {
        $Failures.Add("$($CaseResult.case_name): Resume job.direct_light_samples=$($CaseResult.resume_direct_light_samples) expected $($CaseResult.requested_direct_light_samples).")
    }
}

function Test-SelectionPdfs {
    param(
        [pscustomobject]$CaseResult,
        [double[]]$ExpectedPdfs,
        [System.Collections.Generic.List[string]]$Failures
    )

    if ($CaseResult.scene_light_selection_pdfs.Count -ne $ExpectedPdfs.Count) {
        $Failures.Add("$($CaseResult.case_name): selection_pdf count=$($CaseResult.scene_light_selection_pdfs.Count) expected $($ExpectedPdfs.Count).")
        return
    }

    for ($index = 0; $index -lt $ExpectedPdfs.Count; ++$index) {
        if ([Math]::Abs([double]$CaseResult.scene_light_selection_pdfs[$index] - $ExpectedPdfs[$index]) -gt 1e-6) {
            $Failures.Add("$($CaseResult.case_name): selection_pdf[$index]=$($CaseResult.scene_light_selection_pdfs[$index]) expected $($ExpectedPdfs[$index]).")
        }
    }
}

$repoRoot = (Resolve-Path -LiteralPath ".").Path
$runtimeRoot = Resolve-RepoPath -Path "glTFRenderer" -RepoRoot $repoRoot -RequireExists
$noLightScenePath = Resolve-RepoPath -Path $NoLightScene -RepoRoot $repoRoot -RequireExists
$pointLightScenePath = Resolve-RepoPath -Path $PointLightScene -RepoRoot $repoRoot -RequireExists
$exeFullPath = Resolve-RepoPath -Path $ExePath -RepoRoot $repoRoot -RequireExists
$outputBasePath = Resolve-RepoPath -Path $OutputBase -RepoRoot $repoRoot
$logDirPath = Resolve-RepoPath -Path $LogDir -RepoRoot $repoRoot
New-Item -ItemType Directory -Path $outputBasePath -Force | Out-Null
New-Item -ItemType Directory -Path $logDirPath -Force | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$sessionRoot = Join-Path $outputBasePath "direct_light_$timestamp"
$sessionLogDir = Join-Path $logDirPath "lightingbaker_direct_light_validate_$timestamp"
New-Item -ItemType Directory -Path $sessionRoot -Force | Out-Null
New-Item -ItemType Directory -Path $sessionLogDir -Force | Out-Null

$identicalFixture = New-DuplicatePointLightFixture `
    -TemplateScenePath $pointLightScenePath `
    -SessionRoot $sessionRoot `
    -FixtureFileName "quad_uv1_bounce_two_identical_pointlights.gltf" `
    -PrimaryIntensityScale 1.0 `
    -DuplicateIntensityScale 1.0

$weightedFixture = New-DuplicatePointLightFixture `
    -TemplateScenePath $pointLightScenePath `
    -SessionRoot $sessionRoot `
    -FixtureFileName "quad_uv1_bounce_two_weighted_pointlights.gltf" `
    -PrimaryIntensityScale 0.75 `
    -DuplicateIntensityScale 0.25

$noLightResult = Invoke-LightingBakerCase `
    -CaseName "no_light" `
    -ScenePath $noLightScenePath `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -DirectLightSamples 1

$oneLightResult = Invoke-LightingBakerCase `
    -CaseName "one_light" `
    -ScenePath $pointLightScenePath `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -DirectLightSamples 1

$twoLightSampledResult = Invoke-LightingBakerCase `
    -CaseName "two_light_sampled" `
    -ScenePath $identicalFixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -DirectLightSamples 1

$twoLightExactResult = Invoke-LightingBakerCase `
    -CaseName "two_light_exact" `
    -ScenePath $identicalFixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -DirectLightSamples 2

$twoLightWeightedSampledResult = Invoke-LightingBakerCase `
    -CaseName "two_light_weighted_sampled" `
    -ScenePath $weightedFixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -DirectLightSamples 1

$twoLightWeightedExactResult = Invoke-LightingBakerCase `
    -CaseName "two_light_weighted_exact" `
    -ScenePath $weightedFixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -DirectLightSamples 2

$failures = New-Object 'System.Collections.Generic.List[string]'
Test-CaseBaseSuccess -CaseResult $noLightResult -Failures $failures
Test-CaseBaseSuccess -CaseResult $oneLightResult -Failures $failures
Test-CaseBaseSuccess -CaseResult $twoLightSampledResult -Failures $failures
Test-CaseBaseSuccess -CaseResult $twoLightExactResult -Failures $failures
Test-CaseBaseSuccess -CaseResult $twoLightWeightedSampledResult -Failures $failures
Test-CaseBaseSuccess -CaseResult $twoLightWeightedExactResult -Failures $failures

if ($noLightResult.scene_light_count -ne 0) {
    $failures.Add("no_light: scene_light_count=$($noLightResult.scene_light_count) expected 0.")
}

if ($oneLightResult.scene_light_count -ne 1) {
    $failures.Add("one_light: scene_light_count=$($oneLightResult.scene_light_count) expected 1.")
}

if ($twoLightSampledResult.scene_light_count -ne 2) {
    $failures.Add("two_light_sampled: scene_light_count=$($twoLightSampledResult.scene_light_count) expected 2.")
}

if ($twoLightExactResult.scene_light_count -ne 2) {
    $failures.Add("two_light_exact: scene_light_count=$($twoLightExactResult.scene_light_count) expected 2.")
}

if ($twoLightWeightedSampledResult.scene_light_count -ne 2) {
    $failures.Add("two_light_weighted_sampled: scene_light_count=$($twoLightWeightedSampledResult.scene_light_count) expected 2.")
}

if ($twoLightWeightedExactResult.scene_light_count -ne 2) {
    $failures.Add("two_light_weighted_exact: scene_light_count=$($twoLightWeightedExactResult.scene_light_count) expected 2.")
}

Test-SelectionPdfs -CaseResult $twoLightSampledResult -ExpectedPdfs @(0.5, 0.5) -Failures $failures
Test-SelectionPdfs -CaseResult $twoLightExactResult -ExpectedPdfs @(0.5, 0.5) -Failures $failures
Test-SelectionPdfs -CaseResult $twoLightWeightedSampledResult -ExpectedPdfs @(0.75, 0.25) -Failures $failures
Test-SelectionPdfs -CaseResult $twoLightWeightedExactResult -ExpectedPdfs @(0.75, 0.25) -Failures $failures

$zeroTolerance = 1e-6
if ([Math]::Abs($noLightResult.output_rgb_sum) -gt $zeroTolerance) {
    $failures.Add("no_light: output_rgb_sum=$($noLightResult.output_rgb_sum) expected 0.")
}

if ([Math]::Abs($noLightResult.output_luminance_sum) -gt $zeroTolerance) {
    $failures.Add("no_light: output_luminance_sum=$($noLightResult.output_luminance_sum) expected 0.")
}

if ($oneLightResult.output_luminance_sum -le $zeroTolerance) {
    $failures.Add("one_light: output_luminance_sum=$($oneLightResult.output_luminance_sum) expected > 0.")
}

$ratioSampled = $null
$ratioExact = $null
$sampledVsExactRelativeError = $null
$weightedRatioSampled = $null
$weightedRatioExact = $null
$weightedSampledVsExactRelativeError = $null
$weightedRgbRelativeError = $null
if ($oneLightResult.output_luminance_sum -gt $zeroTolerance) {
    $ratioSampled = $twoLightSampledResult.output_luminance_sum / $oneLightResult.output_luminance_sum
    $ratioExact = $twoLightExactResult.output_luminance_sum / $oneLightResult.output_luminance_sum
    $weightedRatioSampled = $twoLightWeightedSampledResult.output_luminance_sum / $oneLightResult.output_luminance_sum
    $weightedRatioExact = $twoLightWeightedExactResult.output_luminance_sum / $oneLightResult.output_luminance_sum
}

if ($null -eq $ratioSampled -or $ratioSampled -lt 1.8 -or $ratioSampled -gt 2.2) {
    $failures.Add("two_light_sampled: luminance ratio to one_light is $ratioSampled expected within [1.8, 2.2].")
}

if ($null -eq $ratioExact -or $ratioExact -lt 1.8 -or $ratioExact -gt 2.2) {
    $failures.Add("two_light_exact: luminance ratio to one_light is $ratioExact expected within [1.8, 2.2].")
}

if ($twoLightExactResult.output_luminance_sum -gt $zeroTolerance) {
    $sampledVsExactRelativeError =
        [Math]::Abs($twoLightSampledResult.output_luminance_sum - $twoLightExactResult.output_luminance_sum) /
        $twoLightExactResult.output_luminance_sum
    if ($sampledVsExactRelativeError -gt 0.02) {
        $failures.Add("two_light_sampled vs two_light_exact: relative luminance error is $sampledVsExactRelativeError expected <= 0.02.")
    }
}
else {
    $failures.Add("two_light_exact: output_luminance_sum=$($twoLightExactResult.output_luminance_sum) expected > 0.")
}

if ($twoLightWeightedExactResult.output_luminance_sum -gt $zeroTolerance) {
    $weightedSampledVsExactRelativeError =
        [Math]::Abs($twoLightWeightedSampledResult.output_luminance_sum - $twoLightWeightedExactResult.output_luminance_sum) /
        $twoLightWeightedExactResult.output_luminance_sum
    if ($weightedSampledVsExactRelativeError -gt 1e-6) {
        $failures.Add("two_light_weighted_sampled vs two_light_weighted_exact: relative luminance error is $weightedSampledVsExactRelativeError expected <= 1e-6.")
    }

    $weightedRgbRelativeError =
        [Math]::Abs($twoLightWeightedSampledResult.output_rgb_sum - $twoLightWeightedExactResult.output_rgb_sum) /
        $twoLightWeightedExactResult.output_rgb_sum
    if ($weightedRgbRelativeError -gt 1e-6) {
        $failures.Add("two_light_weighted_sampled vs two_light_weighted_exact: relative rgb-sum error is $weightedRgbRelativeError expected <= 1e-6.")
    }
}
else {
    $failures.Add("two_light_weighted_exact: output_luminance_sum=$($twoLightWeightedExactResult.output_luminance_sum) expected > 0.")
}

if ($null -eq $weightedRatioSampled -or [Math]::Abs($weightedRatioSampled - 1.0) -gt 1e-6) {
    $failures.Add("two_light_weighted_sampled: luminance ratio to one_light is $weightedRatioSampled expected 1.0.")
}

if ($null -eq $weightedRatioExact -or [Math]::Abs($weightedRatioExact - 1.0) -gt 1e-6) {
    $failures.Add("two_light_weighted_exact: luminance ratio to one_light is $weightedRatioExact expected 1.0.")
}

$weightedMasterMatches = Test-FileBytesEqual `
    -LeftPath (Resolve-RepoPath -Path $twoLightWeightedSampledResult.atlas_master_path -RepoRoot $repoRoot -RequireExists) `
    -RightPath (Resolve-RepoPath -Path $twoLightWeightedExactResult.atlas_master_path -RepoRoot $repoRoot -RequireExists)

$status = "ValidationFailed"
$pass = $false
if ($failures.Count -eq 0) {
    $status = "ValidationPassed"
    $pass = $true
}
elseif ($noLightResult.timed_out -or
        $oneLightResult.timed_out -or
        $twoLightSampledResult.timed_out -or
        $twoLightExactResult.timed_out -or
        $twoLightWeightedSampledResult.timed_out -or
        $twoLightWeightedExactResult.timed_out) {
    $status = "ValidationTimeout"
}

$summaryObject = [pscustomobject]@{
    status = $status
    passed = $pass
    session_root = Convert-ToRepoRelativePath -Path $sessionRoot -RepoRoot $repoRoot
    generated_fixtures = [pscustomobject]@{
        identical_scene = Convert-ToRepoRelativePath -Path $identicalFixture.scene_path -RepoRoot $repoRoot
        weighted_scene = Convert-ToRepoRelativePath -Path $weightedFixture.scene_path -RepoRoot $repoRoot
        fixture_root = Convert-ToRepoRelativePath -Path $identicalFixture.fixture_root -RepoRoot $repoRoot
    }
    results = @(
        $noLightResult,
        $oneLightResult,
        $twoLightSampledResult,
        $twoLightExactResult,
        $twoLightWeightedSampledResult,
        $twoLightWeightedExactResult
    )
    metrics = [pscustomobject]@{
        one_light_luminance_sum = $oneLightResult.output_luminance_sum
        two_light_sampled_luminance_sum = $twoLightSampledResult.output_luminance_sum
        two_light_exact_luminance_sum = $twoLightExactResult.output_luminance_sum
        two_light_sampled_to_one_light_ratio = $ratioSampled
        two_light_exact_to_one_light_ratio = $ratioExact
        two_light_sampled_vs_exact_relative_error = $sampledVsExactRelativeError
        two_light_weighted_sampled_luminance_sum = $twoLightWeightedSampledResult.output_luminance_sum
        two_light_weighted_exact_luminance_sum = $twoLightWeightedExactResult.output_luminance_sum
        two_light_weighted_sampled_to_one_light_ratio = $weightedRatioSampled
        two_light_weighted_exact_to_one_light_ratio = $weightedRatioExact
        two_light_weighted_sampled_vs_exact_relative_error = $weightedSampledVsExactRelativeError
        two_light_weighted_rgb_relative_error = $weightedRgbRelativeError
        two_light_weighted_master_bytes_match = $weightedMasterMatches
    }
    failures = @($failures)
}

$summaryJsonPath = Join-Path $sessionRoot "summary.json"
$summaryObject | ConvertTo-Json -Depth 16 | Out-File -LiteralPath $summaryJsonPath -Encoding utf8

Write-Host "STATUS=$status"
Write-Host "SUMMARY_JSON=$(Convert-ToRepoRelativePath -Path $summaryJsonPath -RepoRoot $repoRoot)"
Write-Host "NO_LIGHT_OUTPUT=$($noLightResult.output_root)"
Write-Host "ONE_LIGHT_OUTPUT=$($oneLightResult.output_root)"
Write-Host "TWO_LIGHT_SAMPLED_OUTPUT=$($twoLightSampledResult.output_root)"
Write-Host "TWO_LIGHT_EXACT_OUTPUT=$($twoLightExactResult.output_root)"
Write-Host "TWO_LIGHT_WEIGHTED_SAMPLED_OUTPUT=$($twoLightWeightedSampledResult.output_root)"
Write-Host "TWO_LIGHT_WEIGHTED_EXACT_OUTPUT=$($twoLightWeightedExactResult.output_root)"
