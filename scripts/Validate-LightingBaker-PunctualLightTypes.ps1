param(
    [string]$NoLightScene = ".verify\\lightingbaker_fixture\\quad_uv1_bounce_nolight.gltf",
    [string]$PointLightScene = ".verify\\lightingbaker_fixture\\quad_uv1_bounce_pointlight.gltf",
    [string]$ExePath = "glTFRenderer\\x64\\Debug\\LightingBaker.exe",
    [string]$OutputBase = "run_artifacts\\lightingbaker_punctual_light_types_validate",
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

function New-LightTypeFixture {
    param(
        [string]$TemplateScenePath,
        [string]$SessionRoot,
        [string]$LightType,
        [string]$FixtureFileName
    )

    $fixtureRoot = Join-Path $SessionRoot "fixture"
    New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null

    $templateSceneDir = Split-Path -Parent $TemplateScenePath
    Get-ChildItem -LiteralPath $templateSceneDir -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $fixtureRoot $_.Name) -Force
    }

    $scene = Read-JsonFile -Path $TemplateScenePath
    $extensions = Get-OptionalProperty -Object $scene -Name "extensions"
    if ($null -eq $extensions) {
        throw "Template scene does not define root extensions."
    }

    $punctual = Get-OptionalProperty -Object $extensions -Name "KHR_lights_punctual"
    if ($null -eq $punctual) {
        throw "Template scene is missing KHR_lights_punctual."
    }

    $lights = @($(Get-OptionalProperty -Object $punctual -Name "lights"))
    if ($lights.Count -eq 0) {
        throw "Template scene does not contain any punctual lights."
    }

    $nodes = @($(Get-OptionalProperty -Object $scene -Name "nodes"))
    if ($nodes.Count -eq 0) {
        throw "Template scene does not contain any nodes."
    }

    $light = $lights[0]
    $lightNode = $null
    foreach ($node in $nodes) {
        $nodeExtensions = Get-OptionalProperty -Object $node -Name "extensions"
        if ($null -eq $nodeExtensions) {
            continue
        }

        $nodePunctual = Get-OptionalProperty -Object $nodeExtensions -Name "KHR_lights_punctual"
        if ($null -eq $nodePunctual) {
            continue
        }

        $nodeLightIndex = Get-OptionalProperty -Object $nodePunctual -Name "light"
        if ($nodeLightIndex -eq 0) {
            $lightNode = $node
            break
        }
    }

    if ($null -eq $lightNode) {
        throw "Template scene does not reference punctual light 0 from any node."
    }

    Set-ObjectProperty -Object $light -Name "type" -Value $LightType
    switch ($LightType) {
        "directional" {
            $rangeProperty = $light.PSObject.Properties["range"]
            if ($null -ne $rangeProperty) {
                $light.PSObject.Properties.Remove("range")
            }

            $spotProperty = $light.PSObject.Properties["spot"]
            if ($null -ne $spotProperty) {
                $light.PSObject.Properties.Remove("spot")
            }

            Set-ObjectProperty -Object $light -Name "intensity" -Value 20.0
            Set-ObjectProperty -Object $lightNode -Name "rotation" -Value @(
                0.0,
                0.6427876096865394,
                0.0,
                0.7660444431189780
            )
        }
        "spot" {
            Set-ObjectProperty -Object $light -Name "intensity" -Value 400.0
            Set-ObjectProperty -Object $light -Name "range" -Value 4.0
            Set-ObjectProperty -Object $light -Name "spot" -Value ([pscustomobject]@{
                innerConeAngle = 0.2
                outerConeAngle = 0.7
            })
        }
        "point" {
            $spotProperty = $light.PSObject.Properties["spot"]
            if ($null -ne $spotProperty) {
                $light.PSObject.Properties.Remove("spot")
            }
        }
        default {
            throw "Unsupported light type: $LightType"
        }
    }

    $fixtureScenePath = Join-Path $fixtureRoot $FixtureFileName
    $sceneJson = $scene | ConvertTo-Json -Depth 100
    [System.IO.File]::WriteAllText($fixtureScenePath, $sceneJson, [System.Text.UTF8Encoding]::new($false))

    return [pscustomobject]@{
        scene_path = $fixtureScenePath
        light_type = $LightType
        fixture_root = $fixtureRoot
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
        [int]$TimeoutSec
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
        "--direct-light-samples", "1"
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

    $sceneSummaryPath = Join-Path $bakeOutputRoot "debug\\raytracing_scene_summary.json"
    $dispatchSummaryPath = Join-Path $bakeOutputRoot "debug\\raytracing_dispatch_summary.json"
    $requiredArtifacts = @(
        (Join-Path $bakeOutputRoot "manifest.json"),
        $sceneSummaryPath,
        $dispatchSummaryPath,
        (Join-Path $bakeOutputRoot "cache\\resume.json"),
        (Join-Path $bakeOutputRoot "atlases\\indirect_00.preview.png")
    )
    $missingArtifacts = @($requiredArtifacts | Where-Object { -not (Test-Path -LiteralPath $_) })

    $sceneLightCount = 0
    $directionalLightCount = 0
    $pointLightCount = 0
    $spotLightCount = 0
    $selectionPdfs = @()
    if (Test-Path -LiteralPath $sceneSummaryPath) {
        $sceneSummary = Read-JsonFile -Path $sceneSummaryPath
        $sceneLightCount = [int](Get-OptionalIntProperty -Object $sceneSummary -Name "scene_light_count")
        $directionalLightCount = [int](Get-OptionalIntProperty -Object $sceneSummary -Name "directional_light_count")
        $pointLightCount = [int](Get-OptionalIntProperty -Object $sceneSummary -Name "point_light_count")
        $spotLightCount = [int](Get-OptionalIntProperty -Object $sceneSummary -Name "spot_light_count")
        $selectionPdfs = @(
            @(Get-OptionalProperty -Object $sceneSummary -Name "scene_lights") | ForEach-Object {
                $selectionPdf = Get-OptionalDoubleProperty -Object $_ -Name "selection_pdf"
                if ($null -eq $selectionPdf) { 0.0 } else { [double]$selectionPdf }
            }
        )
    }

    $outputLuminanceSum = 0.0
    $nonzeroRgbTexelCount = 0
    if (Test-Path -LiteralPath $dispatchSummaryPath) {
        $dispatchSummary = Read-JsonFile -Path $dispatchSummaryPath
        $luminanceValue = Get-OptionalDoubleProperty -Object $dispatchSummary -Name "output_luminance_sum"
        if ($null -ne $luminanceValue) {
            $outputLuminanceSum = $luminanceValue
        }

        $nonzeroValue = Get-OptionalIntProperty -Object $dispatchSummary -Name "output_nonzero_rgb_texel_count"
        if ($null -ne $nonzeroValue) {
            $nonzeroRgbTexelCount = $nonzeroValue
        }
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
        scene_light_count = $sceneLightCount
        directional_light_count = $directionalLightCount
        point_light_count = $pointLightCount
        spot_light_count = $spotLightCount
        scene_light_selection_pdfs = @($selectionPdfs)
        output_luminance_sum = $outputLuminanceSum
        output_nonzero_rgb_texel_count = $nonzeroRgbTexelCount
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
$sessionRoot = Join-Path $outputBasePath "punctual_types_$timestamp"
$sessionLogDir = Join-Path $logDirPath "lightingbaker_punctual_types_validate_$timestamp"
New-Item -ItemType Directory -Path $sessionRoot -Force | Out-Null
New-Item -ItemType Directory -Path $sessionLogDir -Force | Out-Null

$spotFixture = New-LightTypeFixture `
    -TemplateScenePath $pointLightScenePath `
    -SessionRoot $sessionRoot `
    -LightType "spot" `
    -FixtureFileName "quad_uv1_bounce_spotlight.gltf"

$directionalFixture = New-LightTypeFixture `
    -TemplateScenePath $pointLightScenePath `
    -SessionRoot $sessionRoot `
    -LightType "directional" `
    -FixtureFileName "quad_uv1_bounce_directional.gltf"

$noLightResult = Invoke-LightingBakerCase `
    -CaseName "no_light" `
    -ScenePath $noLightScenePath `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec

$pointResult = Invoke-LightingBakerCase `
    -CaseName "point" `
    -ScenePath $pointLightScenePath `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec

$spotResult = Invoke-LightingBakerCase `
    -CaseName "spot" `
    -ScenePath $spotFixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec

$directionalResult = Invoke-LightingBakerCase `
    -CaseName "directional" `
    -ScenePath $directionalFixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec

$failures = New-Object 'System.Collections.Generic.List[string]'
Test-CaseBaseSuccess -CaseResult $noLightResult -Failures $failures
Test-CaseBaseSuccess -CaseResult $pointResult -Failures $failures
Test-CaseBaseSuccess -CaseResult $spotResult -Failures $failures
Test-CaseBaseSuccess -CaseResult $directionalResult -Failures $failures

if ($noLightResult.scene_light_count -ne 0) {
    $failures.Add("no_light: scene_light_count=$($noLightResult.scene_light_count) expected 0.")
}

if ([Math]::Abs($noLightResult.output_luminance_sum) -gt 1e-6) {
    $failures.Add("no_light: output_luminance_sum=$($noLightResult.output_luminance_sum) expected 0.")
}

if ($pointResult.scene_light_count -ne 1 -or $pointResult.point_light_count -ne 1 -or $pointResult.directional_light_count -ne 0 -or $pointResult.spot_light_count -ne 0) {
    $failures.Add("point: unexpected light counts scene=$($pointResult.scene_light_count) directional=$($pointResult.directional_light_count) point=$($pointResult.point_light_count) spot=$($pointResult.spot_light_count).")
}

if ($spotResult.scene_light_count -ne 1 -or $spotResult.spot_light_count -ne 1 -or $spotResult.directional_light_count -ne 0 -or $spotResult.point_light_count -ne 0) {
    $failures.Add("spot: unexpected light counts scene=$($spotResult.scene_light_count) directional=$($spotResult.directional_light_count) point=$($spotResult.point_light_count) spot=$($spotResult.spot_light_count).")
}

if ($directionalResult.scene_light_count -ne 1 -or $directionalResult.directional_light_count -ne 1 -or $directionalResult.point_light_count -ne 0 -or $directionalResult.spot_light_count -ne 0) {
    $failures.Add("directional: unexpected light counts scene=$($directionalResult.scene_light_count) directional=$($directionalResult.directional_light_count) point=$($directionalResult.point_light_count) spot=$($directionalResult.spot_light_count).")
}

Test-SelectionPdfs -CaseResult $pointResult -ExpectedPdfs @(1.0) -Failures $failures
Test-SelectionPdfs -CaseResult $spotResult -ExpectedPdfs @(1.0) -Failures $failures
Test-SelectionPdfs -CaseResult $directionalResult -ExpectedPdfs @(1.0) -Failures $failures

foreach ($caseResult in @($pointResult, $spotResult, $directionalResult)) {
    if ($caseResult.output_luminance_sum -le 1e-6) {
        $failures.Add("$($caseResult.case_name): output_luminance_sum=$($caseResult.output_luminance_sum) expected > 0.")
    }

    if ($caseResult.output_nonzero_rgb_texel_count -le 0) {
        $failures.Add("$($caseResult.case_name): output_nonzero_rgb_texel_count=$($caseResult.output_nonzero_rgb_texel_count) expected > 0.")
    }
}

$status = "ValidationFailed"
$pass = $false
if ($failures.Count -eq 0) {
    $status = "ValidationPassed"
    $pass = $true
}
elseif ($noLightResult.timed_out -or $pointResult.timed_out -or $spotResult.timed_out -or $directionalResult.timed_out) {
    $status = "ValidationTimeout"
}

$summaryObject = [pscustomobject]@{
    status = $status
    passed = $pass
    session_root = Convert-ToRepoRelativePath -Path $sessionRoot -RepoRoot $repoRoot
    generated_fixtures = [pscustomobject]@{
        spot_scene = Convert-ToRepoRelativePath -Path $spotFixture.scene_path -RepoRoot $repoRoot
        directional_scene = Convert-ToRepoRelativePath -Path $directionalFixture.scene_path -RepoRoot $repoRoot
        fixture_root = Convert-ToRepoRelativePath -Path $spotFixture.fixture_root -RepoRoot $repoRoot
    }
    results = @(
        $noLightResult,
        $pointResult,
        $spotResult,
        $directionalResult
    )
    failures = @($failures)
}

$summaryJsonPath = Join-Path $sessionRoot "summary.json"
$summaryObject | ConvertTo-Json -Depth 16 | Out-File -LiteralPath $summaryJsonPath -Encoding utf8

Write-Host "STATUS=$status"
Write-Host "SUMMARY_JSON=$(Convert-ToRepoRelativePath -Path $summaryJsonPath -RepoRoot $repoRoot)"
Write-Host "NO_LIGHT_OUTPUT=$($noLightResult.output_root)"
Write-Host "POINT_OUTPUT=$($pointResult.output_root)"
Write-Host "SPOT_OUTPUT=$($spotResult.output_root)"
Write-Host "DIRECTIONAL_OUTPUT=$($directionalResult.output_root)"
