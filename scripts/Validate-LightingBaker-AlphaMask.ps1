param(
    [string]$OpaqueScene = ".verify\\lightingbaker_fixture\\quad_uv1_alpha_occluder_opaque.gltf",
    [string]$FactorMaskScene = ".verify\\lightingbaker_fixture\\quad_uv1_alpha_occluder_mask.gltf",
    [string]$ExePath = "glTFRenderer\\x64\\Debug\\LightingBaker.exe",
    [string]$OutputBase = "run_artifacts\\lightingbaker_alpha_mask_validate",
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

function Set-ObjectProperty {
    param(
        [object]$Object,
        [string]$Name,
        [object]$Value
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        Add-Member -InputObject $Object -MemberType NoteProperty -Name $Name -Value $Value
    }
    else {
        $property.Value = $Value
    }
}

function Remove-ObjectProperty {
    param(
        [object]$Object,
        [string]$Name
    )

    if ($null -eq $Object) {
        return
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -ne $property) {
        $Object.PSObject.Properties.Remove($Name)
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

function Read-JsonFile {
    param([string]$Path)

    return [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}

function New-SolidTexture {
    param(
        [string]$TexturePath,
        [byte]$AlphaByte
    )

    Add-Type -AssemblyName System.Drawing
    $bitmap = New-Object System.Drawing.Bitmap 1, 1
    try {
        $bitmap.SetPixel(0, 0, [System.Drawing.Color]::FromArgb($AlphaByte, 255, 255, 255))
        $bitmap.Save($TexturePath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
}

function New-TransparentTexture {
    param([string]$TexturePath)

    New-SolidTexture -TexturePath $TexturePath -AlphaByte 0
}

function New-FixtureSceneClone {
    param(
        [string]$TemplateScenePath,
        [string]$SessionRoot,
        [string]$FixtureName
    )

    $fixtureRoot = Join-Path $SessionRoot $FixtureName
    New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null

    $sceneText = [System.IO.File]::ReadAllText($TemplateScenePath, [System.Text.Encoding]::UTF8)
    $scene = $sceneText | ConvertFrom-Json
    if ($null -eq $scene.materials -or $scene.materials.Count -lt 2) {
        throw "Expected at least two materials in fixture template: $TemplateScenePath"
    }
    if ($null -eq $scene.buffers -or $scene.buffers.Count -lt 1) {
        throw "Fixture template does not contain buffers: $TemplateScenePath"
    }

    $templateRoot = Split-Path -Parent $TemplateScenePath
    foreach ($buffer in @($scene.buffers)) {
        $bufferUri = Get-OptionalProperty -Object $buffer -Name "uri"
        if ([string]::IsNullOrWhiteSpace($bufferUri)) {
            throw "Fixture template buffer is missing a uri: $TemplateScenePath"
        }

        $sourceBufferPath = Join-Path $templateRoot $bufferUri
        if (-not (Test-Path -LiteralPath $sourceBufferPath)) {
            throw "Fixture template buffer not found: $sourceBufferPath"
        }

        $bufferFileName = [System.IO.Path]::GetFileName($bufferUri)
        Copy-Item -LiteralPath $sourceBufferPath -Destination (Join-Path $fixtureRoot $bufferFileName) -Force
        $buffer.uri = $bufferFileName
    }

    return [pscustomobject][ordered]@{
        fixture_root = $fixtureRoot
        scene = $scene
    }
}

function Write-FixtureScene {
    param(
        [object]$Scene,
        [string]$FixtureScenePath
    )

    $sceneJson = $Scene | ConvertTo-Json -Depth 100
    [System.IO.File]::WriteAllText($FixtureScenePath, $sceneJson, [System.Text.UTF8Encoding]::new($false))
}

function Configure-MaskedMaterialTexture {
    param(
        [object]$Scene,
        [string]$FixtureRoot,
        [string]$TextureFileName,
        [byte]$TextureAlphaByte,
        [double]$AlphaCutoff,
        [string]$AlphaMode
    )

    $texturePath = Join-Path $FixtureRoot $TextureFileName
    New-SolidTexture -TexturePath $texturePath -AlphaByte $TextureAlphaByte

    Set-ObjectProperty -Object $Scene -Name "images" -Value @(
        [pscustomobject]@{
            uri = $textureFileName
        }
    )
    Set-ObjectProperty -Object $Scene -Name "samplers" -Value @(
        [pscustomobject]@{
            magFilter = 9729
            minFilter = 9729
            wrapS = 33071
            wrapT = 33071
        }
    )
    Set-ObjectProperty -Object $Scene -Name "textures" -Value @(
        [pscustomobject]@{
            sampler = 0
            source = 0
        }
    )

    $maskedMaterial = $Scene.materials[1]
    if ($null -eq $maskedMaterial.pbrMetallicRoughness) {
        throw "Masked fixture material is missing pbrMetallicRoughness: $FixtureRoot"
    }

    $maskedMaterial.pbrMetallicRoughness.baseColorFactor = @(1.0, 1.0, 1.0, 1.0)
    Set-ObjectProperty -Object $maskedMaterial.pbrMetallicRoughness -Name "baseColorTexture" -Value (
        [pscustomobject]@{
            index = 0
            texCoord = 1
        }
    )
    Set-ObjectProperty -Object $maskedMaterial -Name "alphaMode" -Value $AlphaMode
    Set-ObjectProperty -Object $maskedMaterial -Name "alphaCutoff" -Value $AlphaCutoff

    return $texturePath
}

function Configure-MaskedMaterialFactorOnly {
    param(
        [object]$Scene,
        [double]$FactorAlpha,
        [double]$AlphaCutoff,
        [string]$AlphaMode
    )

    Remove-ObjectProperty -Object $Scene -Name "images"
    Remove-ObjectProperty -Object $Scene -Name "samplers"
    Remove-ObjectProperty -Object $Scene -Name "textures"

    $maskedMaterial = $Scene.materials[1]
    if ($null -eq $maskedMaterial.pbrMetallicRoughness) {
        throw "Masked fixture material is missing pbrMetallicRoughness."
    }

    $maskedMaterial.pbrMetallicRoughness.baseColorFactor = @(1.0, 1.0, 1.0, $FactorAlpha)
    Remove-ObjectProperty -Object $maskedMaterial.pbrMetallicRoughness -Name "baseColorTexture"
    Set-ObjectProperty -Object $maskedMaterial -Name "alphaMode" -Value $AlphaMode
    Set-ObjectProperty -Object $maskedMaterial -Name "alphaCutoff" -Value $AlphaCutoff
}

function New-TextureMaskFixture {
    param(
        [string]$TemplateScenePath,
        [string]$SessionRoot
    )

    $fixture = New-FixtureSceneClone -TemplateScenePath $TemplateScenePath -SessionRoot $SessionRoot -FixtureName "fixture_texture_mask"
    $texturePath = Configure-MaskedMaterialTexture `
        -Scene $fixture.scene `
        -FixtureRoot $fixture.fixture_root `
        -TextureFileName "alpha_mask_transparent.png" `
        -TextureAlphaByte 0 `
        -AlphaCutoff 0.5 `
        -AlphaMode "MASK"

    $fixtureScenePath = Join-Path $fixture.fixture_root "quad_uv1_alpha_occluder_texture_mask.gltf"
    Write-FixtureScene -Scene $fixture.scene -FixtureScenePath $fixtureScenePath

    return [pscustomobject][ordered]@{
        fixture_root = $fixture.fixture_root
        scene_path = $fixtureScenePath
        texture_path = $texturePath
    }
}

function New-FactorCutoffBoundaryFixture {
    param(
        [string]$TemplateScenePath,
        [string]$SessionRoot
    )

    $fixture = New-FixtureSceneClone -TemplateScenePath $TemplateScenePath -SessionRoot $SessionRoot -FixtureName "fixture_factor_cutoff_boundary"
    Configure-MaskedMaterialFactorOnly -Scene $fixture.scene -FactorAlpha 0.5 -AlphaCutoff 0.5 -AlphaMode "MASK"

    $fixtureScenePath = Join-Path $fixture.fixture_root "quad_uv1_alpha_occluder_factor_cutoff_boundary.gltf"
    Write-FixtureScene -Scene $fixture.scene -FixtureScenePath $fixtureScenePath

    return [pscustomobject][ordered]@{
        fixture_root = $fixture.fixture_root
        scene_path = $fixtureScenePath
        texture_path = $null
    }
}

function New-TextureCutoffBoundaryFixture {
    param(
        [string]$TemplateScenePath,
        [string]$SessionRoot
    )

    $fixture = New-FixtureSceneClone -TemplateScenePath $TemplateScenePath -SessionRoot $SessionRoot -FixtureName "fixture_texture_cutoff_boundary"
    $alphaByte = [byte]128
    $alphaCutoff = [double]$alphaByte / 255.0
    $texturePath = Configure-MaskedMaterialTexture `
        -Scene $fixture.scene `
        -FixtureRoot $fixture.fixture_root `
        -TextureFileName "alpha_mask_cutoff_boundary.png" `
        -TextureAlphaByte $alphaByte `
        -AlphaCutoff $alphaCutoff `
        -AlphaMode "MASK"

    $fixtureScenePath = Join-Path $fixture.fixture_root "quad_uv1_alpha_occluder_texture_cutoff_boundary.gltf"
    Write-FixtureScene -Scene $fixture.scene -FixtureScenePath $fixtureScenePath

    return [pscustomobject][ordered]@{
        fixture_root = $fixture.fixture_root
        scene_path = $fixtureScenePath
        texture_path = $texturePath
        alpha_cutoff = $alphaCutoff
    }
}

function New-BlendTextureFallbackFixture {
    param(
        [string]$TemplateScenePath,
        [string]$SessionRoot
    )

    $fixture = New-FixtureSceneClone -TemplateScenePath $TemplateScenePath -SessionRoot $SessionRoot -FixtureName "fixture_blend_texture_fallback"
    $texturePath = Configure-MaskedMaterialTexture `
        -Scene $fixture.scene `
        -FixtureRoot $fixture.fixture_root `
        -TextureFileName "alpha_blend_transparent.png" `
        -TextureAlphaByte 0 `
        -AlphaCutoff 0.5 `
        -AlphaMode "BLEND"

    $fixtureScenePath = Join-Path $fixture.fixture_root "quad_uv1_alpha_occluder_blend_texture.gltf"
    Write-FixtureScene -Scene $fixture.scene -FixtureScenePath $fixtureScenePath

    return [pscustomobject][ordered]@{
        fixture_root = $fixture.fixture_root
        scene_path = $fixtureScenePath
        texture_path = $texturePath
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

    $stdoutPath = Join-Path $LogDirPath ($CaseName + ".stdout.log")
    $stderrPath = Join-Path $LogDirPath ($CaseName + ".stderr.log")

    $arguments = @(
        "--scene", $ScenePath,
        "--output", $bakeOutputRoot,
        "--samples-per-iteration", "1",
        "--target-samples", "1"
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
    $finished = $process.WaitForExit($TimeoutSec * 1000)
    $stopwatch.Stop()
    if (-not $finished) {
        $timedOut = $true
        Stop-ProcessTree -ParentProcessId $process.Id
        $exitCode = 124
    }
    else {
        $exitCode = [int]$process.ExitCode
    }

    $sceneSummaryPath = Join-Path $bakeOutputRoot "debug\\raytracing_scene_summary.json"
    $runtimeSummaryPath = Join-Path $bakeOutputRoot "debug\\raytracing_runtime_summary.json"
    $dispatchSummaryPath = Join-Path $bakeOutputRoot "debug\\raytracing_dispatch_summary.json"

    $requiredArtifacts = @(
        (Join-Path $bakeOutputRoot "manifest.json"),
        (Join-Path $bakeOutputRoot "debug\\import_summary.json"),
        $sceneSummaryPath,
        $runtimeSummaryPath,
        $dispatchSummaryPath,
        (Join-Path $bakeOutputRoot "atlases\\indirect_00.rgba16f.bin"),
        (Join-Path $bakeOutputRoot "atlases\\indirect_00.master.rgba16f.bin"),
        (Join-Path $bakeOutputRoot "atlases\\indirect_00.preview.png"),
        (Join-Path $bakeOutputRoot "debug\\coverage_00.png"),
        (Join-Path $bakeOutputRoot "debug\\chart_id_00.png")
    )
    $missingArtifacts = @($requiredArtifacts | Where-Object { -not (Test-Path -LiteralPath $_) })

    $sceneSummary = $null
    $runtimeSummary = $null
    $dispatchSummary = $null
    if (Test-Path -LiteralPath $sceneSummaryPath) {
        $sceneSummary = Read-JsonFile -Path $sceneSummaryPath
    }
    if (Test-Path -LiteralPath $runtimeSummaryPath) {
        $runtimeSummary = Read-JsonFile -Path $runtimeSummaryPath
    }
    if (Test-Path -LiteralPath $dispatchSummaryPath) {
        $dispatchSummary = Read-JsonFile -Path $dispatchSummaryPath
    }

    return [pscustomobject][ordered]@{
        case_name = $CaseName
        exit_code = $exitCode
        timed_out = $timedOut
        duration_ms = [int]$stopwatch.ElapsedMilliseconds
        source_scene = Convert-ToRepoRelativePath -Path $ScenePath -RepoRoot $RepoRoot
        output_root = Convert-ToRepoRelativePath -Path $bakeOutputRoot -RepoRoot $RepoRoot
        stdout = Convert-ToRepoRelativePath -Path $stdoutPath -RepoRoot $RepoRoot
        stderr = Convert-ToRepoRelativePath -Path $stderrPath -RepoRoot $RepoRoot
        missing_artifacts = @($missingArtifacts | ForEach-Object { Convert-ToRepoRelativePath -Path $_ -RepoRoot $RepoRoot })
        scene_validation_status = Get-OptionalProperty -Object $sceneSummary -Name "validation_status"
        runtime_validation_status = Get-OptionalProperty -Object $runtimeSummary -Name "validation_status"
        dispatch_validation_status = Get-OptionalProperty -Object $dispatchSummary -Name "validation_status"
        alpha_masked_instance_count = Get-OptionalIntProperty -Object $sceneSummary -Name "alpha_masked_instance_count"
        alpha_blended_instance_count = Get-OptionalIntProperty -Object $sceneSummary -Name "alpha_blended_instance_count"
        fully_transparent_masked_primitive_count = Get-OptionalIntProperty -Object $sceneSummary -Name "fully_transparent_masked_primitive_count"
        skipped_primitive_count = Get-OptionalIntProperty -Object $sceneSummary -Name "skipped_primitive_count"
        scene_warning_codes = @(
            @($sceneSummary.warnings) | ForEach-Object {
                $warningCode = Get-OptionalProperty -Object $_ -Name "code"
                if (-not [string]::IsNullOrWhiteSpace($warningCode)) {
                    [string]$warningCode
                }
            }
        )
        material_texture_count = Get-OptionalIntProperty -Object $sceneSummary -Name "material_texture_count"
        uploaded_material_texture_count = Get-OptionalIntProperty -Object $runtimeSummary -Name "uploaded_material_texture_count"
        bound_material_texture_count = Get-OptionalIntProperty -Object $runtimeSummary -Name "bound_material_texture_count"
        output_nonzero_rgb_texel_count = Get-OptionalIntProperty -Object $dispatchSummary -Name "output_nonzero_rgb_texel_count"
        output_nonzero_alpha_texel_count = Get-OptionalIntProperty -Object $dispatchSummary -Name "output_nonzero_alpha_texel_count"
    }
}

function Test-CaseBaseSuccess {
    param(
        [pscustomobject]$CaseResult,
        [System.Collections.Generic.List[string]]$Failures
    )

    if ($CaseResult.timed_out) {
        $Failures.Add("$($CaseResult.case_name): timed out")
    }
    if ($CaseResult.exit_code -ne 0) {
        $Failures.Add("$($CaseResult.case_name): exit code $($CaseResult.exit_code)")
    }
    if ($CaseResult.missing_artifacts.Count -ne 0) {
        $Failures.Add("$($CaseResult.case_name): missing artifacts -> $([string]::Join(', ', $CaseResult.missing_artifacts))")
    }
    if ($CaseResult.scene_validation_status -ne "passed") {
        $Failures.Add("$($CaseResult.case_name): scene validation status $($CaseResult.scene_validation_status)")
    }
    if ($CaseResult.runtime_validation_status -ne "passed") {
        $Failures.Add("$($CaseResult.case_name): runtime validation status $($CaseResult.runtime_validation_status)")
    }
    if ($CaseResult.dispatch_validation_status -ne "passed") {
        $Failures.Add("$($CaseResult.case_name): dispatch validation status $($CaseResult.dispatch_validation_status)")
    }
}

$repoRoot = (Resolve-Path -LiteralPath ".").Path
$runtimeRoot = Resolve-RepoPath -Path "glTFRenderer" -RepoRoot $repoRoot -RequireExists
$opaqueScenePath = Resolve-RepoPath -Path $OpaqueScene -RepoRoot $repoRoot -RequireExists
$factorMaskScenePath = Resolve-RepoPath -Path $FactorMaskScene -RepoRoot $repoRoot -RequireExists
$exeFullPath = Resolve-RepoPath -Path $ExePath -RepoRoot $repoRoot -RequireExists
$outputBasePath = Resolve-RepoPath -Path $OutputBase -RepoRoot $repoRoot
$logDirPath = Resolve-RepoPath -Path $LogDir -RepoRoot $repoRoot

New-Item -ItemType Directory -Path $outputBasePath -Force | Out-Null
New-Item -ItemType Directory -Path $logDirPath -Force | Out-Null

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$sessionRoot = Join-Path $outputBasePath ("alpha_mask_" + $stamp)
$sessionLogDir = Join-Path $logDirPath ("lightingbaker_alpha_mask_" + $stamp)
New-Item -ItemType Directory -Path $sessionRoot -Force | Out-Null
New-Item -ItemType Directory -Path $sessionLogDir -Force | Out-Null

$textureMaskFixture = New-TextureMaskFixture -TemplateScenePath $opaqueScenePath -SessionRoot $sessionRoot
$factorCutoffBoundaryFixture = New-FactorCutoffBoundaryFixture -TemplateScenePath $opaqueScenePath -SessionRoot $sessionRoot
$textureCutoffBoundaryFixture = New-TextureCutoffBoundaryFixture -TemplateScenePath $opaqueScenePath -SessionRoot $sessionRoot
$blendTextureFallbackFixture = New-BlendTextureFallbackFixture -TemplateScenePath $opaqueScenePath -SessionRoot $sessionRoot

$opaqueResult = Invoke-LightingBakerCase `
    -CaseName "opaque" `
    -ScenePath $opaqueScenePath `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec

$factorMaskResult = Invoke-LightingBakerCase `
    -CaseName "factor_mask" `
    -ScenePath $factorMaskScenePath `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec

$factorCutoffBoundaryResult = Invoke-LightingBakerCase `
    -CaseName "factor_cutoff_boundary" `
    -ScenePath $factorCutoffBoundaryFixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec

$textureMaskResult = Invoke-LightingBakerCase `
    -CaseName "texture_mask" `
    -ScenePath $textureMaskFixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec

$textureCutoffBoundaryResult = Invoke-LightingBakerCase `
    -CaseName "texture_cutoff_boundary" `
    -ScenePath $textureCutoffBoundaryFixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec

$blendTextureFallbackResult = Invoke-LightingBakerCase `
    -CaseName "blend_texture_fallback" `
    -ScenePath $blendTextureFallbackFixture.scene_path `
    -ExeFullPath $exeFullPath `
    -RuntimeRoot $runtimeRoot `
    -SessionRoot $sessionRoot `
    -LogDirPath $sessionLogDir `
    -RepoRoot $repoRoot `
    -TimeoutSec $TimeoutSec

$failures = [System.Collections.Generic.List[string]]::new()
Test-CaseBaseSuccess -CaseResult $opaqueResult -Failures $failures
Test-CaseBaseSuccess -CaseResult $factorMaskResult -Failures $failures
Test-CaseBaseSuccess -CaseResult $factorCutoffBoundaryResult -Failures $failures
Test-CaseBaseSuccess -CaseResult $textureMaskResult -Failures $failures
Test-CaseBaseSuccess -CaseResult $textureCutoffBoundaryResult -Failures $failures
Test-CaseBaseSuccess -CaseResult $blendTextureFallbackResult -Failures $failures

if ($opaqueResult.alpha_masked_instance_count -ne 0) {
    $failures.Add("opaque: expected alpha_masked_instance_count=0 but saw $($opaqueResult.alpha_masked_instance_count)")
}
if ($opaqueResult.alpha_blended_instance_count -ne 0) {
    $failures.Add("opaque: expected alpha_blended_instance_count=0 but saw $($opaqueResult.alpha_blended_instance_count)")
}
if ($opaqueResult.fully_transparent_masked_primitive_count -ne 0) {
    $failures.Add("opaque: expected fully_transparent_masked_primitive_count=0 but saw $($opaqueResult.fully_transparent_masked_primitive_count)")
}
if ($opaqueResult.skipped_primitive_count -ne 0) {
    $failures.Add("opaque: expected skipped_primitive_count=0 but saw $($opaqueResult.skipped_primitive_count)")
}

if ($factorMaskResult.alpha_masked_instance_count -ne 0) {
    $failures.Add("factor_mask: expected alpha_masked_instance_count=0 but saw $($factorMaskResult.alpha_masked_instance_count)")
}
if ($factorMaskResult.fully_transparent_masked_primitive_count -ne 2) {
    $failures.Add("factor_mask: expected fully_transparent_masked_primitive_count=2 but saw $($factorMaskResult.fully_transparent_masked_primitive_count)")
}
if ($factorMaskResult.skipped_primitive_count -ne 2) {
    $failures.Add("factor_mask: expected skipped_primitive_count=2 but saw $($factorMaskResult.skipped_primitive_count)")
}
if ($factorMaskResult.material_texture_count -ne 0) {
    $failures.Add("factor_mask: expected material_texture_count=0 but saw $($factorMaskResult.material_texture_count)")
}
if ($factorMaskResult.uploaded_material_texture_count -ne 0) {
    $failures.Add("factor_mask: expected uploaded_material_texture_count=0 but saw $($factorMaskResult.uploaded_material_texture_count)")
}

if ($factorCutoffBoundaryResult.alpha_masked_instance_count -ne 2) {
    $failures.Add(
        "factor_cutoff_boundary: expected alpha_masked_instance_count=2 but saw " +
        $factorCutoffBoundaryResult.alpha_masked_instance_count
    )
}
if ($factorCutoffBoundaryResult.alpha_blended_instance_count -ne 0) {
    $failures.Add(
        "factor_cutoff_boundary: expected alpha_blended_instance_count=0 but saw " +
        $factorCutoffBoundaryResult.alpha_blended_instance_count
    )
}
if ($factorCutoffBoundaryResult.fully_transparent_masked_primitive_count -ne 0) {
    $failures.Add(
        "factor_cutoff_boundary: expected fully_transparent_masked_primitive_count=0 but saw " +
        $factorCutoffBoundaryResult.fully_transparent_masked_primitive_count
    )
}
if ($factorCutoffBoundaryResult.skipped_primitive_count -ne 0) {
    $failures.Add(
        "factor_cutoff_boundary: expected skipped_primitive_count=0 but saw " +
        $factorCutoffBoundaryResult.skipped_primitive_count
    )
}
if ($factorCutoffBoundaryResult.material_texture_count -ne 0) {
    $failures.Add(
        "factor_cutoff_boundary: expected material_texture_count=0 but saw " +
        $factorCutoffBoundaryResult.material_texture_count
    )
}
if ($factorCutoffBoundaryResult.uploaded_material_texture_count -ne 0) {
    $failures.Add(
        "factor_cutoff_boundary: expected uploaded_material_texture_count=0 but saw " +
        $factorCutoffBoundaryResult.uploaded_material_texture_count
    )
}
if ($factorCutoffBoundaryResult.output_nonzero_rgb_texel_count -ne $opaqueResult.output_nonzero_rgb_texel_count) {
    $failures.Add(
        "factor_cutoff_boundary: expected output_nonzero_rgb_texel_count ($($factorCutoffBoundaryResult.output_nonzero_rgb_texel_count)) " +
        "to match opaque ($($opaqueResult.output_nonzero_rgb_texel_count))"
    )
}

if ($textureMaskResult.alpha_masked_instance_count -ne 2) {
    $failures.Add("texture_mask: expected alpha_masked_instance_count=2 but saw $($textureMaskResult.alpha_masked_instance_count)")
}
if ($textureMaskResult.alpha_blended_instance_count -ne 0) {
    $failures.Add("texture_mask: expected alpha_blended_instance_count=0 but saw $($textureMaskResult.alpha_blended_instance_count)")
}
if ($textureMaskResult.fully_transparent_masked_primitive_count -ne 0) {
    $failures.Add("texture_mask: expected fully_transparent_masked_primitive_count=0 but saw $($textureMaskResult.fully_transparent_masked_primitive_count)")
}
if ($textureMaskResult.skipped_primitive_count -ne 0) {
    $failures.Add("texture_mask: expected skipped_primitive_count=0 but saw $($textureMaskResult.skipped_primitive_count)")
}
if ($textureMaskResult.material_texture_count -ne 1) {
    $failures.Add("texture_mask: expected material_texture_count=1 but saw $($textureMaskResult.material_texture_count)")
}
if ($textureMaskResult.uploaded_material_texture_count -ne 1) {
    $failures.Add("texture_mask: expected uploaded_material_texture_count=1 but saw $($textureMaskResult.uploaded_material_texture_count)")
}
if ($textureMaskResult.output_nonzero_rgb_texel_count -le $opaqueResult.output_nonzero_rgb_texel_count) {
    $failures.Add(
        "texture_mask: expected output_nonzero_rgb_texel_count ($($textureMaskResult.output_nonzero_rgb_texel_count)) " +
        "to exceed opaque ($($opaqueResult.output_nonzero_rgb_texel_count))"
    )
}
if ($factorMaskResult.output_nonzero_rgb_texel_count -le $opaqueResult.output_nonzero_rgb_texel_count) {
    $failures.Add(
        "factor_mask: expected output_nonzero_rgb_texel_count ($($factorMaskResult.output_nonzero_rgb_texel_count)) " +
        "to exceed opaque ($($opaqueResult.output_nonzero_rgb_texel_count))"
    )
}
if ($textureMaskResult.output_nonzero_rgb_texel_count -ne $factorMaskResult.output_nonzero_rgb_texel_count) {
    $failures.Add(
        "texture_mask: expected output_nonzero_rgb_texel_count ($($textureMaskResult.output_nonzero_rgb_texel_count)) " +
        "to match factor_mask ($($factorMaskResult.output_nonzero_rgb_texel_count))"
    )
}

if ($textureCutoffBoundaryResult.alpha_masked_instance_count -ne 2) {
    $failures.Add(
        "texture_cutoff_boundary: expected alpha_masked_instance_count=2 but saw " +
        $textureCutoffBoundaryResult.alpha_masked_instance_count
    )
}
if ($textureCutoffBoundaryResult.alpha_blended_instance_count -ne 0) {
    $failures.Add(
        "texture_cutoff_boundary: expected alpha_blended_instance_count=0 but saw " +
        $textureCutoffBoundaryResult.alpha_blended_instance_count
    )
}
if ($textureCutoffBoundaryResult.fully_transparent_masked_primitive_count -ne 0) {
    $failures.Add(
        "texture_cutoff_boundary: expected fully_transparent_masked_primitive_count=0 but saw " +
        $textureCutoffBoundaryResult.fully_transparent_masked_primitive_count
    )
}
if ($textureCutoffBoundaryResult.skipped_primitive_count -ne 0) {
    $failures.Add(
        "texture_cutoff_boundary: expected skipped_primitive_count=0 but saw " +
        $textureCutoffBoundaryResult.skipped_primitive_count
    )
}
if ($textureCutoffBoundaryResult.material_texture_count -ne 1) {
    $failures.Add(
        "texture_cutoff_boundary: expected material_texture_count=1 but saw " +
        $textureCutoffBoundaryResult.material_texture_count
    )
}
if ($textureCutoffBoundaryResult.uploaded_material_texture_count -ne 1) {
    $failures.Add(
        "texture_cutoff_boundary: expected uploaded_material_texture_count=1 but saw " +
        $textureCutoffBoundaryResult.uploaded_material_texture_count
    )
}
if ($textureCutoffBoundaryResult.output_nonzero_rgb_texel_count -ne $opaqueResult.output_nonzero_rgb_texel_count) {
    $failures.Add(
        "texture_cutoff_boundary: expected output_nonzero_rgb_texel_count ($($textureCutoffBoundaryResult.output_nonzero_rgb_texel_count)) " +
        "to match opaque ($($opaqueResult.output_nonzero_rgb_texel_count))"
    )
}

if ($blendTextureFallbackResult.alpha_masked_instance_count -ne 0) {
    $failures.Add(
        "blend_texture_fallback: expected alpha_masked_instance_count=0 but saw " +
        $blendTextureFallbackResult.alpha_masked_instance_count
    )
}
if ($blendTextureFallbackResult.alpha_blended_instance_count -ne 2) {
    $failures.Add(
        "blend_texture_fallback: expected alpha_blended_instance_count=2 but saw " +
        $blendTextureFallbackResult.alpha_blended_instance_count
    )
}
if ($blendTextureFallbackResult.fully_transparent_masked_primitive_count -ne 0) {
    $failures.Add(
        "blend_texture_fallback: expected fully_transparent_masked_primitive_count=0 but saw " +
        $blendTextureFallbackResult.fully_transparent_masked_primitive_count
    )
}
if ($blendTextureFallbackResult.skipped_primitive_count -ne 0) {
    $failures.Add(
        "blend_texture_fallback: expected skipped_primitive_count=0 but saw " +
        $blendTextureFallbackResult.skipped_primitive_count
    )
}
if ($blendTextureFallbackResult.material_texture_count -ne 1) {
    $failures.Add(
        "blend_texture_fallback: expected material_texture_count=1 but saw " +
        $blendTextureFallbackResult.material_texture_count
    )
}
if ($blendTextureFallbackResult.uploaded_material_texture_count -ne 1) {
    $failures.Add(
        "blend_texture_fallback: expected uploaded_material_texture_count=1 but saw " +
        $blendTextureFallbackResult.uploaded_material_texture_count
    )
}
if ($blendTextureFallbackResult.output_nonzero_rgb_texel_count -ne $opaqueResult.output_nonzero_rgb_texel_count) {
    $failures.Add(
        "blend_texture_fallback: expected output_nonzero_rgb_texel_count ($($blendTextureFallbackResult.output_nonzero_rgb_texel_count)) " +
        "to match opaque ($($opaqueResult.output_nonzero_rgb_texel_count))"
    )
}
if (@($blendTextureFallbackResult.scene_warning_codes) -notcontains "rt_scene_alpha_blend_fallback") {
    $failures.Add("blend_texture_fallback: expected scene warning code rt_scene_alpha_blend_fallback")
}

$status = "ValidationFailed"
$pass = $false
if ($failures.Count -eq 0) {
    $status = "ValidationPassed"
    $pass = $true
}
elseif ($opaqueResult.timed_out -or
        $factorMaskResult.timed_out -or
        $factorCutoffBoundaryResult.timed_out -or
        $textureMaskResult.timed_out -or
        $textureCutoffBoundaryResult.timed_out -or
        $blendTextureFallbackResult.timed_out) {
    $status = "ValidationTimeout"
}

$summaryObject = [ordered]@{
    status = $status
    pass = $pass
    opaque = $opaqueResult
    factor_mask = $factorMaskResult
    factor_cutoff_boundary = $factorCutoffBoundaryResult
    texture_mask = $textureMaskResult
    texture_cutoff_boundary = $textureCutoffBoundaryResult
    blend_texture_fallback = $blendTextureFallbackResult
    generated_fixtures = [ordered]@{
        texture_mask_scene = Convert-ToRepoRelativePath -Path $textureMaskFixture.scene_path -RepoRoot $repoRoot
        texture_mask_texture = Convert-ToRepoRelativePath -Path $textureMaskFixture.texture_path -RepoRoot $repoRoot
        factor_cutoff_boundary_scene = Convert-ToRepoRelativePath -Path $factorCutoffBoundaryFixture.scene_path -RepoRoot $repoRoot
        texture_cutoff_boundary_scene = Convert-ToRepoRelativePath -Path $textureCutoffBoundaryFixture.scene_path -RepoRoot $repoRoot
        texture_cutoff_boundary_texture = Convert-ToRepoRelativePath -Path $textureCutoffBoundaryFixture.texture_path -RepoRoot $repoRoot
        texture_cutoff_boundary_alpha_cutoff = $textureCutoffBoundaryFixture.alpha_cutoff
        blend_texture_fallback_scene = Convert-ToRepoRelativePath -Path $blendTextureFallbackFixture.scene_path -RepoRoot $repoRoot
        blend_texture_fallback_texture = Convert-ToRepoRelativePath -Path $blendTextureFallbackFixture.texture_path -RepoRoot $repoRoot
    }
    failures = @($failures)
}

$summaryJsonPath = Join-Path $sessionRoot "summary.json"
$summaryObject | ConvertTo-Json -Depth 16 | Out-File -LiteralPath $summaryJsonPath -Encoding utf8

Write-Host "STATUS=$status"
Write-Host "SUMMARY_JSON=$(Convert-ToRepoRelativePath -Path $summaryJsonPath -RepoRoot $repoRoot)"
Write-Host "OPAQUE_OUTPUT=$($opaqueResult.output_root)"
Write-Host "FACTOR_MASK_OUTPUT=$($factorMaskResult.output_root)"
Write-Host "FACTOR_CUTOFF_BOUNDARY_OUTPUT=$($factorCutoffBoundaryResult.output_root)"
Write-Host "TEXTURE_MASK_OUTPUT=$($textureMaskResult.output_root)"
Write-Host "TEXTURE_CUTOFF_BOUNDARY_OUTPUT=$($textureCutoffBoundaryResult.output_root)"
Write-Host "BLEND_TEXTURE_FALLBACK_OUTPUT=$($blendTextureFallbackResult.output_root)"
Write-Host "TEXTURE_MASK_SCENE=$(Convert-ToRepoRelativePath -Path $textureMaskFixture.scene_path -RepoRoot $repoRoot)"
Write-Host "TEXTURE_MASK_TEXTURE=$(Convert-ToRepoRelativePath -Path $textureMaskFixture.texture_path -RepoRoot $repoRoot)"
Write-Host "FACTOR_CUTOFF_BOUNDARY_SCENE=$(Convert-ToRepoRelativePath -Path $factorCutoffBoundaryFixture.scene_path -RepoRoot $repoRoot)"
Write-Host "TEXTURE_CUTOFF_BOUNDARY_SCENE=$(Convert-ToRepoRelativePath -Path $textureCutoffBoundaryFixture.scene_path -RepoRoot $repoRoot)"
Write-Host "TEXTURE_CUTOFF_BOUNDARY_TEXTURE=$(Convert-ToRepoRelativePath -Path $textureCutoffBoundaryFixture.texture_path -RepoRoot $repoRoot)"
Write-Host "BLEND_TEXTURE_FALLBACK_SCENE=$(Convert-ToRepoRelativePath -Path $blendTextureFallbackFixture.scene_path -RepoRoot $repoRoot)"
Write-Host "BLEND_TEXTURE_FALLBACK_TEXTURE=$(Convert-ToRepoRelativePath -Path $blendTextureFallbackFixture.texture_path -RepoRoot $repoRoot)"
Write-Host "OPAQUE_RGB=$($opaqueResult.output_nonzero_rgb_texel_count)"
Write-Host "FACTOR_MASK_RGB=$($factorMaskResult.output_nonzero_rgb_texel_count)"
Write-Host "FACTOR_CUTOFF_BOUNDARY_RGB=$($factorCutoffBoundaryResult.output_nonzero_rgb_texel_count)"
Write-Host "TEXTURE_MASK_RGB=$($textureMaskResult.output_nonzero_rgb_texel_count)"
Write-Host "TEXTURE_CUTOFF_BOUNDARY_RGB=$($textureCutoffBoundaryResult.output_nonzero_rgb_texel_count)"
Write-Host "BLEND_TEXTURE_FALLBACK_RGB=$($blendTextureFallbackResult.output_nonzero_rgb_texel_count)"
Write-Host "FAILURE_COUNT=$($failures.Count)"
foreach ($failure in @($failures | Select-Object -First 10)) {
    Write-Host "FAILURE=$failure"
}

if ($pass) {
    exit 0
}

exit 1
