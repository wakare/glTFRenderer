param(
    [string]$NoLightScene = ".verify\\lightingbaker_fixture\\quad_uv1_bounce_nolight.gltf",
    [string]$ExePath = "glTFRenderer\\x64\\Debug\\LightingBaker.exe",
    [string]$OutputBase = "run_artifacts\\lightingbaker_emissive_direct_sampling_validate",
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

function New-EmissiveCeilingFixture {
    param(
        [string]$TemplateScenePath,
        [string]$SessionRoot,
        [string]$FixtureFileName,
        [double]$EmissiveIntensity = 4.0
    )

    $fixtureRoot = Join-Path $SessionRoot "generated_fixture"
    New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null

    $scene = Read-JsonFile -Path $TemplateScenePath
    $templateSceneDirectory = Split-Path -Parent $TemplateScenePath
    $materials = @($scene.materials)
    $meshes = @($scene.meshes)
    if ($materials.Count -lt 1 -or $meshes.Count -lt 2) {
        throw "Expected at least one material and two meshes in template scene."
    }

    $emissiveMaterial = ($materials[0] | ConvertTo-Json -Depth 32 | ConvertFrom-Json)
    if ($null -ne (Get-OptionalProperty -Object $emissiveMaterial -Name "name")) {
        $emissiveMaterial.name = "CeilingEmissive"
    }
    else {
        $emissiveMaterial | Add-Member -NotePropertyName "name" -NotePropertyValue "CeilingEmissive"
    }

    if ($null -ne (Get-OptionalProperty -Object $emissiveMaterial -Name "emissiveFactor")) {
        $emissiveMaterial.emissiveFactor = @($EmissiveIntensity, $EmissiveIntensity, $EmissiveIntensity)
    }
    else {
        $emissiveMaterial | Add-Member -NotePropertyName "emissiveFactor" -NotePropertyValue @($EmissiveIntensity, $EmissiveIntensity, $EmissiveIntensity)
    }
    if ($null -eq (Get-OptionalProperty -Object $emissiveMaterial -Name "doubleSided")) {
        $emissiveMaterial | Add-Member -NotePropertyName "doubleSided" -NotePropertyValue $false
    }

    $materials += $emissiveMaterial
    $scene.materials = @($materials)
    $ceilingMaterialIndex = $materials.Count - 1
    $meshes[1].primitives[0].material = $ceilingMaterialIndex
    $scene.meshes = @($meshes)

    foreach ($buffer in @((Get-OptionalProperty -Object $scene -Name "buffers"))) {
        $bufferUri = Get-OptionalProperty -Object $buffer -Name "uri"
        if ([string]::IsNullOrWhiteSpace([string]$bufferUri)) {
            continue
        }

        $sourcePath = Join-Path $templateSceneDirectory $bufferUri
        $destinationPath = Join-Path $fixtureRoot $bufferUri
        $destinationDirectory = Split-Path -Parent $destinationPath
        if (-not [string]::IsNullOrWhiteSpace($destinationDirectory)) {
            New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
        }
        Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
    }

    foreach ($image in @((Get-OptionalProperty -Object $scene -Name "images"))) {
        $imageUri = Get-OptionalProperty -Object $image -Name "uri"
        if ([string]::IsNullOrWhiteSpace([string]$imageUri)) {
            continue
        }

        $sourcePath = Join-Path $templateSceneDirectory $imageUri
        $destinationPath = Join-Path $fixtureRoot $imageUri
        $destinationDirectory = Split-Path -Parent $destinationPath
        if (-not [string]::IsNullOrWhiteSpace($destinationDirectory)) {
            New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
        }
        Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
    }

    $fixtureScenePath = Join-Path $fixtureRoot $FixtureFileName
    $scene | ConvertTo-Json -Depth 32 | Out-File -LiteralPath $fixtureScenePath -Encoding utf8

    return [pscustomobject]@{
        scene_path = $fixtureScenePath
        fixture_root = $fixtureRoot
        emissive_material_index = $ceilingMaterialIndex
        emissive_intensity = $EmissiveIntensity
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
        [int]$MaxBounces,
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
        "--resolution", "32",
        "--samples-per-iteration", "256",
        "--target-samples", "256",
        "--max-bounces", ([string]$MaxBounces),
        "--sky-intensity", "0",
        "--sky-ground-mix", "0",
        "--direct-light-samples", ([string]$DirectLightSamples),
        "--environment-light-samples", "0",
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
        $process.WaitForExit()
    }
    $stopwatch.Stop()
    $process.Refresh()

    $exitCode = if ($timedOut) { -1 } else { [int]$process.ExitCode }
    $manifestPath = Join-Path $bakeOutputRoot "manifest.json"
    $sceneSummaryPath = Join-Path $bakeOutputRoot "debug\\raytracing_scene_summary.json"
    $runtimeSummaryPath = Join-Path $bakeOutputRoot "debug\\raytracing_runtime_summary.json"
    $dispatchSummaryPath = Join-Path $bakeOutputRoot "debug\\raytracing_dispatch_summary.json"
    $resumePath = Join-Path $bakeOutputRoot "cache\\resume.json"
    $requiredArtifacts = @(
        $manifestPath,
        $sceneSummaryPath,
        $runtimeSummaryPath,
        $dispatchSummaryPath,
        $resumePath,
        (Join-Path $bakeOutputRoot "atlases\\indirect_00.preview.png")
    )
    $missingArtifacts = @($requiredArtifacts | Where-Object { -not (Test-Path -LiteralPath $_) })

    $sceneEmissiveTriangleCount = 0
    $sceneEmissivePrimitiveCount = 0
    $sceneEmissiveSelectionPdfs = @()
    if (Test-Path -LiteralPath $sceneSummaryPath) {
        $sceneSummary = Read-JsonFile -Path $sceneSummaryPath
        $sceneEmissiveTriangleCount = [int](Get-OptionalIntProperty -Object $sceneSummary -Name "emissive_triangle_count")
        $sceneEmissivePrimitiveCount = [int](Get-OptionalIntProperty -Object $sceneSummary -Name "emissive_primitive_count")
        $sceneEmissiveSelectionPdfs = @(
            @(Get-OptionalProperty -Object $sceneSummary -Name "emissive_triangles") | ForEach-Object {
                [double](Get-OptionalDoubleProperty -Object $_ -Name "selection_pdf")
            }
        )
    }

    $runtimeUploadedEmissiveTriangleCount = 0
    $runtimeEmissiveTriangleBufferCreated = $false
    if (Test-Path -LiteralPath $runtimeSummaryPath) {
        $runtimeSummary = Read-JsonFile -Path $runtimeSummaryPath
        $runtimeUploadedEmissiveTriangleCount =
            [int](Get-OptionalIntProperty -Object $runtimeSummary -Name "uploaded_emissive_triangle_count")
        $runtimeEmissiveTriangleBufferCreated =
            [bool](Get-OptionalProperty -Object $runtimeSummary -Name "emissive_triangle_buffer_created")
    }

    $dispatchDirectLightSampleCount = 0
    $dispatchEmissiveTriangleCount = 0
    $dispatchEmissiveTriangleRootAllocationFound = $false
    $dispatchEmissiveTriangleBufferBound = $false
    $outputLuminanceSum = 0.0
    $outputRgbSum = 0.0
    $nonzeroRgbTexelCount = 0
    if (Test-Path -LiteralPath $dispatchSummaryPath) {
        $dispatchSummary = Read-JsonFile -Path $dispatchSummaryPath
        $dispatchDirectLightSampleCount = [int](Get-OptionalIntProperty -Object $dispatchSummary -Name "direct_light_sample_count")
        $dispatchEmissiveTriangleCount = [int](Get-OptionalIntProperty -Object $dispatchSummary -Name "emissive_triangle_count")
        $dispatchEmissiveTriangleRootAllocationFound =
            [bool](Get-OptionalProperty -Object $dispatchSummary -Name "emissive_triangle_root_allocation_found")
        $dispatchEmissiveTriangleBufferBound =
            [bool](Get-OptionalProperty -Object $dispatchSummary -Name "emissive_triangle_buffer_bound")
        $outputLuminanceSum = [double](Get-OptionalDoubleProperty -Object $dispatchSummary -Name "output_luminance_sum")
        $outputRgbSum = [double](Get-OptionalDoubleProperty -Object $dispatchSummary -Name "output_rgb_sum")
        $nonzeroRgbTexelCount = [int](Get-OptionalIntProperty -Object $dispatchSummary -Name "output_nonzero_rgb_texel_count")
    }

    $manifestDirectLightSamples = 0
    if (Test-Path -LiteralPath $manifestPath) {
        $manifest = Read-JsonFile -Path $manifestPath
        $integrator = Get-OptionalProperty -Object $manifest -Name "integrator"
        $manifestDirectLightSamples = [int](Get-OptionalIntProperty -Object $integrator -Name "direct_light_samples")
    }

    $resumeDirectLightSamples = 0
    if (Test-Path -LiteralPath $resumePath) {
        $resumeJson = Read-JsonFile -Path $resumePath
        $job = Get-OptionalProperty -Object $resumeJson -Name "job"
        $resumeDirectLightSamples = [int](Get-OptionalIntProperty -Object $job -Name "direct_light_samples")
    }

    return [pscustomobject]@{
        case_name = $CaseName
        scene_path = Convert-ToRepoRelativePath -Path $ScenePath -RepoRoot $RepoRoot
        output_root = Convert-ToRepoRelativePath -Path $bakeOutputRoot -RepoRoot $RepoRoot
        stdout = Convert-ToRepoRelativePath -Path $stdoutPath -RepoRoot $RepoRoot
        stderr = Convert-ToRepoRelativePath -Path $stderrPath -RepoRoot $RepoRoot
        duration_ms = [int]$stopwatch.ElapsedMilliseconds
        exit_code = $exitCode
        timed_out = $timedOut
        max_bounces = $MaxBounces
        requested_direct_light_samples = $DirectLightSamples
        scene_emissive_triangle_count = $sceneEmissiveTriangleCount
        scene_emissive_primitive_count = $sceneEmissivePrimitiveCount
        scene_emissive_selection_pdfs = @($sceneEmissiveSelectionPdfs)
        runtime_uploaded_emissive_triangle_count = $runtimeUploadedEmissiveTriangleCount
        runtime_emissive_triangle_buffer_created = $runtimeEmissiveTriangleBufferCreated
        dispatch_direct_light_sample_count = $dispatchDirectLightSampleCount
        dispatch_emissive_triangle_count = $dispatchEmissiveTriangleCount
        dispatch_emissive_triangle_root_allocation_found = $dispatchEmissiveTriangleRootAllocationFound
        dispatch_emissive_triangle_buffer_bound = $dispatchEmissiveTriangleBufferBound
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
        $Failures.Add("$($CaseResult.case_name): missing required artifacts: $([string]::Join(', ', $CaseResult.missing_artifacts)).")
    }

    if ($CaseResult.dispatch_direct_light_sample_count -ne $CaseResult.requested_direct_light_samples) {
        $Failures.Add("$($CaseResult.case_name): dispatch direct_light_sample_count=$($CaseResult.dispatch_direct_light_sample_count) expected $($CaseResult.requested_direct_light_samples).")
    }

    if ($CaseResult.manifest_direct_light_samples -ne $CaseResult.requested_direct_light_samples) {
        $Failures.Add("$($CaseResult.case_name): manifest integrator.direct_light_samples=$($CaseResult.manifest_direct_light_samples) expected $($CaseResult.requested_direct_light_samples).")
    }

    if ($CaseResult.resume_direct_light_samples -ne $CaseResult.requested_direct_light_samples) {
        $Failures.Add("$($CaseResult.case_name): resume job.direct_light_samples=$($CaseResult.resume_direct_light_samples) expected $($CaseResult.requested_direct_light_samples).")
    }

    if (-not $CaseResult.dispatch_emissive_triangle_root_allocation_found) {
        $Failures.Add("$($CaseResult.case_name): emissive triangle root allocation was not found.")
    }

    if (-not $CaseResult.dispatch_emissive_triangle_buffer_bound) {
        $Failures.Add("$($CaseResult.case_name): emissive triangle buffer was not bound.")
    }

    if (-not $CaseResult.runtime_emissive_triangle_buffer_created) {
        $Failures.Add("$($CaseResult.case_name): runtime emissive triangle buffer was not created.")
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
$sessionRoot = Join-Path $outputBasePath "emissive_direct_$timestamp"
$sessionLogDir = Join-Path $logDirPath "lightingbaker_emissive_direct_validate_$timestamp"
New-Item -ItemType Directory -Path $sessionRoot -Force | Out-Null
New-Item -ItemType Directory -Path $sessionLogDir -Force | Out-Null

$fixture = New-EmissiveCeilingFixture `
    -TemplateScenePath $noLightScenePath `
    -SessionRoot $sessionRoot `
    -FixtureFileName "quad_uv1_bounce_emissive_direct.gltf"

$oneBounceBaseline = Invoke-LightingBakerCase `
    -CaseName "one_bounce_baseline" `
    -ScenePath $fixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -MaxBounces 1 `
    -DirectLightSamples 0

$oneBounceEmissiveDirect = Invoke-LightingBakerCase `
    -CaseName "one_bounce_emissive_direct" `
    -ScenePath $fixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -MaxBounces 1 `
    -DirectLightSamples 1

$twoBounceBaseline = Invoke-LightingBakerCase `
    -CaseName "two_bounce_baseline" `
    -ScenePath $fixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -MaxBounces 2 `
    -DirectLightSamples 0

$twoBounceEmissiveDirect = Invoke-LightingBakerCase `
    -CaseName "two_bounce_emissive_direct" `
    -ScenePath $fixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -MaxBounces 2 `
    -DirectLightSamples 1

$threeBounceBaseline = Invoke-LightingBakerCase `
    -CaseName "three_bounce_baseline" `
    -ScenePath $fixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec `
    -MaxBounces 3 `
    -DirectLightSamples 0

$failures = New-Object 'System.Collections.Generic.List[string]'
foreach ($caseResult in @($oneBounceBaseline, $oneBounceEmissiveDirect, $twoBounceBaseline, $twoBounceEmissiveDirect, $threeBounceBaseline)) {
    Test-CaseBaseSuccess -CaseResult $caseResult -Failures $failures
}

foreach ($caseResult in @($oneBounceBaseline, $oneBounceEmissiveDirect, $twoBounceBaseline, $twoBounceEmissiveDirect, $threeBounceBaseline)) {
    if ($caseResult.scene_emissive_triangle_count -ne 2) {
        $failures.Add("$($caseResult.case_name): scene_emissive_triangle_count=$($caseResult.scene_emissive_triangle_count) expected 2.")
    }

    if ($caseResult.scene_emissive_primitive_count -ne 1) {
        $failures.Add("$($caseResult.case_name): scene_emissive_primitive_count=$($caseResult.scene_emissive_primitive_count) expected 1.")
    }

    if ($caseResult.runtime_uploaded_emissive_triangle_count -ne 2) {
        $failures.Add("$($caseResult.case_name): runtime uploaded_emissive_triangle_count=$($caseResult.runtime_uploaded_emissive_triangle_count) expected 2.")
    }

    if ($caseResult.dispatch_emissive_triangle_count -ne 2) {
        $failures.Add("$($caseResult.case_name): dispatch emissive_triangle_count=$($caseResult.dispatch_emissive_triangle_count) expected 2.")
    }

    if ($caseResult.scene_emissive_selection_pdfs.Count -ne 2) {
        $failures.Add("$($caseResult.case_name): emissive triangle selection pdf count=$($caseResult.scene_emissive_selection_pdfs.Count) expected 2.")
    }
    else {
        $pdf0 = [double]$caseResult.scene_emissive_selection_pdfs[0]
        $pdf1 = [double]$caseResult.scene_emissive_selection_pdfs[1]
        if ([Math]::Abs($pdf0 - 0.5) -gt 1e-6 -or [Math]::Abs($pdf1 - 0.5) -gt 1e-6) {
            $failures.Add("$($caseResult.case_name): emissive triangle selection pdfs=[$pdf0, $pdf1] expected [0.5, 0.5].")
        }
    }
}

if ($oneBounceBaseline.output_nonzero_rgb_texel_count -le 0 -or
    $oneBounceEmissiveDirect.output_nonzero_rgb_texel_count -le 0 -or
    $twoBounceBaseline.output_nonzero_rgb_texel_count -le 0 -or
    $twoBounceEmissiveDirect.output_nonzero_rgb_texel_count -le 0 -or
    $threeBounceBaseline.output_nonzero_rgb_texel_count -le 0) {
    $failures.Add("all emissive direct-sampling cases must produce nonzero rgb texels.")
}

$oneBounceGainRatio = if ($oneBounceBaseline.output_luminance_sum -gt 1e-6) {
    [double]($oneBounceEmissiveDirect.output_luminance_sum / $oneBounceBaseline.output_luminance_sum)
}
else {
    [double]0.0
}
if ($oneBounceGainRatio -le 1.01) {
    $failures.Add("one_bounce_emissive_direct: luminance gain ratio=$oneBounceGainRatio expected > 1.01.")
}

if ($twoBounceBaseline.output_luminance_sum -le $oneBounceBaseline.output_luminance_sum) {
    $failures.Add("two_bounce_baseline: output_luminance_sum=$($twoBounceBaseline.output_luminance_sum) expected > one_bounce_baseline=$($oneBounceBaseline.output_luminance_sum).")
}

if ($threeBounceBaseline.output_luminance_sum -le $twoBounceBaseline.output_luminance_sum) {
    $failures.Add("three_bounce_baseline: output_luminance_sum=$($threeBounceBaseline.output_luminance_sum) expected > two_bounce_baseline=$($twoBounceBaseline.output_luminance_sum).")
}

if ($twoBounceEmissiveDirect.output_luminance_sum -le $twoBounceBaseline.output_luminance_sum) {
    $failures.Add("two_bounce_emissive_direct: output_luminance_sum=$($twoBounceEmissiveDirect.output_luminance_sum) expected > two_bounce_baseline=$($twoBounceBaseline.output_luminance_sum).")
}

$luminanceRelativeError = if ($twoBounceBaseline.output_luminance_sum -gt 1e-6) {
    [double]([Math]::Abs($oneBounceEmissiveDirect.output_luminance_sum - $twoBounceBaseline.output_luminance_sum) / $twoBounceBaseline.output_luminance_sum)
}
else {
    [double]0.0
}
if ($luminanceRelativeError -gt 0.3) {
    $failures.Add("one_bounce_emissive_direct: luminance relative error to two_bounce_baseline=$luminanceRelativeError expected <= 0.3.")
}

$rgbRelativeError = if ([Math]::Abs($twoBounceBaseline.output_rgb_sum) -gt 1e-6) {
    [double]([Math]::Abs($oneBounceEmissiveDirect.output_rgb_sum - $twoBounceBaseline.output_rgb_sum) / [Math]::Abs($twoBounceBaseline.output_rgb_sum))
}
else {
    [double]0.0
}
if ($rgbRelativeError -gt 0.3) {
    $failures.Add("one_bounce_emissive_direct: rgb relative error to two_bounce_baseline=$rgbRelativeError expected <= 0.3.")
}

$twoBounceMisLuminanceRelativeError = if ($twoBounceBaseline.output_luminance_sum -gt 1e-6) {
    [double]([Math]::Abs($twoBounceEmissiveDirect.output_luminance_sum - $twoBounceBaseline.output_luminance_sum) / $twoBounceBaseline.output_luminance_sum)
}
else {
    [double]0.0
}
$twoBounceMisLuminanceRelativeError = if ($threeBounceBaseline.output_luminance_sum -gt 1e-6) {
    [double]([Math]::Abs($twoBounceEmissiveDirect.output_luminance_sum - $threeBounceBaseline.output_luminance_sum) / $threeBounceBaseline.output_luminance_sum)
}
else {
    [double]0.0
}
if ($twoBounceMisLuminanceRelativeError -gt 0.2) {
    $failures.Add("two_bounce_emissive_direct: luminance relative error to three_bounce_baseline=$twoBounceMisLuminanceRelativeError expected <= 0.2.")
}

$twoBounceMisRgbRelativeError = if ([Math]::Abs($threeBounceBaseline.output_rgb_sum) -gt 1e-6) {
    [double]([Math]::Abs($twoBounceEmissiveDirect.output_rgb_sum - $threeBounceBaseline.output_rgb_sum) / [Math]::Abs($threeBounceBaseline.output_rgb_sum))
}
else {
    [double]0.0
}
if ($twoBounceMisRgbRelativeError -gt 0.2) {
    $failures.Add("two_bounce_emissive_direct: rgb relative error to three_bounce_baseline=$twoBounceMisRgbRelativeError expected <= 0.2.")
}

$status = "ValidationFailed"
$pass = $false
if ($failures.Count -eq 0) {
    $status = "ValidationPassed"
    $pass = $true
}
elseif ($oneBounceBaseline.timed_out -or
        $oneBounceEmissiveDirect.timed_out -or
        $twoBounceBaseline.timed_out -or
        $twoBounceEmissiveDirect.timed_out -or
        $threeBounceBaseline.timed_out) {
    $status = "ValidationTimeout"
}

$summaryObject = [pscustomobject]@{
    status = $status
    passed = $pass
    session_root = Convert-ToRepoRelativePath -Path $sessionRoot -RepoRoot $repoRoot
    generated_fixtures = [pscustomobject]@{
        emissive_scene = Convert-ToRepoRelativePath -Path $fixture.scene_path -RepoRoot $repoRoot
        fixture_root = Convert-ToRepoRelativePath -Path $fixture.fixture_root -RepoRoot $repoRoot
        emissive_material_index = $fixture.emissive_material_index
        emissive_intensity = $fixture.emissive_intensity
    }
    metrics = [pscustomobject]@{
        one_bounce_gain_ratio = $oneBounceGainRatio
        one_bounce_vs_two_bounce_luminance_relative_error = $luminanceRelativeError
        one_bounce_vs_two_bounce_rgb_relative_error = $rgbRelativeError
        two_bounce_direct_vs_three_bounce_baseline_luminance_relative_error = $twoBounceMisLuminanceRelativeError
        two_bounce_direct_vs_three_bounce_baseline_rgb_relative_error = $twoBounceMisRgbRelativeError
    }
    results = @(
        $oneBounceBaseline,
        $oneBounceEmissiveDirect,
        $twoBounceBaseline,
        $twoBounceEmissiveDirect,
        $threeBounceBaseline
    )
    failures = @($failures)
}

$summaryJsonPath = Join-Path $sessionRoot "summary.json"
$summaryObject | ConvertTo-Json -Depth 16 | Out-File -LiteralPath $summaryJsonPath -Encoding utf8

Write-Host "STATUS=$status"
Write-Host "SUMMARY_JSON=$(Convert-ToRepoRelativePath -Path $summaryJsonPath -RepoRoot $repoRoot)"
Write-Host "ONE_BOUNCE_BASELINE_OUTPUT=$($oneBounceBaseline.output_root)"
Write-Host "ONE_BOUNCE_EMISSIVE_DIRECT_OUTPUT=$($oneBounceEmissiveDirect.output_root)"
Write-Host "TWO_BOUNCE_BASELINE_OUTPUT=$($twoBounceBaseline.output_root)"
Write-Host "TWO_BOUNCE_EMISSIVE_DIRECT_OUTPUT=$($twoBounceEmissiveDirect.output_root)"
Write-Host "THREE_BOUNCE_BASELINE_OUTPUT=$($threeBounceBaseline.output_root)"
