param(
    [string]$SourceScene = "glTFRenderer\\glTFRenderer\\glTFResources\\Models\\Lantern\\glTF-Binary\\Lantern.glb",
    [string]$ExePath = "glTFRenderer\\x64\\Debug\\LightingBaker.exe",
    [string]$OutputBase = "run_artifacts\\lightingbaker_lantern_metallic_roughness_validate",
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

function Read-UInt32LE {
    param(
        [byte[]]$Bytes,
        [int]$Offset
    )

    if (($Offset + 4) -gt $Bytes.Length) {
        throw "Attempted to read past the end of a GLB chunk."
    }

    return [System.BitConverter]::ToUInt32($Bytes, $Offset)
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

function Extract-GlbChunks {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 20) {
        throw "GLB file is too small: $Path"
    }

    $magic = Read-UInt32LE -Bytes $bytes -Offset 0
    $version = Read-UInt32LE -Bytes $bytes -Offset 4
    $declaredLength = [int](Read-UInt32LE -Bytes $bytes -Offset 8)
    if ($magic -ne 0x46546C67) {
        throw "Unsupported GLB magic in $Path"
    }
    if ($version -ne 2) {
        throw "Unsupported GLB version $version in $Path"
    }
    if ($declaredLength -gt $bytes.Length) {
        throw "GLB declared length exceeds file size: $Path"
    }

    $offset = 12
    $jsonText = $null
    $binChunk = [byte[]]::new(0)
    while (($offset + 8) -le $declaredLength) {
        $chunkLength = [int](Read-UInt32LE -Bytes $bytes -Offset $offset)
        $chunkType = Read-UInt32LE -Bytes $bytes -Offset ($offset + 4)
        $offset += 8
        if (($offset + $chunkLength) -gt $declaredLength) {
            throw "Encountered a truncated GLB chunk in $Path"
        }

        if ($chunkType -eq 0x4E4F534A) {
            $jsonBytes = [byte[]]::new($chunkLength)
            [System.Array]::Copy($bytes, $offset, $jsonBytes, 0, $chunkLength)
            $jsonText = [System.Text.Encoding]::UTF8.GetString($jsonBytes).TrimEnd([char]0, [char]0x20, [char]0x09, [char]0x0D, [char]0x0A)
        }
        elseif ($chunkType -eq 0x004E4942 -and $binChunk.Length -eq 0) {
            $binChunk = [byte[]]::new($chunkLength)
            [System.Array]::Copy($bytes, $offset, $binChunk, 0, $chunkLength)
        }

        $offset += $chunkLength
    }

    if ([string]::IsNullOrWhiteSpace($jsonText)) {
        throw "GLB JSON chunk was not found in $Path"
    }

    return [ordered]@{
        json_text = $jsonText
        bin_chunk = $binChunk
    }
}

function New-LanternFixture {
    param(
        [string]$SourceScenePath,
        [string]$SessionRoot
    )

    $fixtureRoot = Join-Path $SessionRoot "fixture"
    New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null

    $glb = Extract-GlbChunks -Path $SourceScenePath
    $scene = $glb.json_text | ConvertFrom-Json

    if ($null -eq $scene.buffers -or $scene.buffers.Count -lt 1) {
        throw "Fixture source scene does not contain a primary buffer."
    }

    Set-ObjectProperty -Object $scene.buffers[0] -Name "uri" -Value "Lantern.bin"

    if ($null -eq $scene.meshes) {
        throw "Fixture source scene does not contain any meshes."
    }

    foreach ($mesh in @($scene.meshes)) {
        foreach ($primitive in @($mesh.primitives)) {
            if ($null -eq $primitive.PSObject.Properties["attributes"]) {
                continue
            }

            $attributes = $primitive.attributes
            $uv0Property = $attributes.PSObject.Properties["TEXCOORD_0"]
            $uv1Property = $attributes.PSObject.Properties["TEXCOORD_1"]
            if ($null -ne $uv0Property -and $null -eq $uv1Property) {
                Add-Member -InputObject $attributes -MemberType NoteProperty -Name "TEXCOORD_1" -Value $attributes.TEXCOORD_0
            }
        }
    }

    $removedNodeIndex = -1
    for ($nodeIndex = 0; $nodeIndex -lt $scene.nodes.Count; ++$nodeIndex) {
        if ($scene.nodes[$nodeIndex].name -eq "LanternPole_Lantern") {
            $removedNodeIndex = $nodeIndex
            break
        }
    }

    if ($removedNodeIndex -ge 0) {
        foreach ($node in @($scene.nodes)) {
            if ($null -eq $node.PSObject.Properties["children"]) {
                continue
            }

            $node.children = @($node.children | Where-Object { [int]$_ -ne $removedNodeIndex })
        }

        foreach ($sceneEntry in @($scene.scenes)) {
            if ($null -eq $sceneEntry.PSObject.Properties["nodes"]) {
                continue
            }

            $sceneEntry.nodes = @($sceneEntry.nodes | Where-Object { [int]$_ -ne $removedNodeIndex })
        }
    }

    $fixtureScenePath = Join-Path $fixtureRoot "Lantern_metallic_roughness_uv1_body_chain.gltf"
    $fixtureBinPath = Join-Path $fixtureRoot "Lantern.bin"
    $sceneJson = $scene | ConvertTo-Json -Depth 100
    [System.IO.File]::WriteAllText($fixtureScenePath, $sceneJson, [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllBytes($fixtureBinPath, $glb.bin_chunk)

    return [ordered]@{
        fixture_root = $fixtureRoot
        scene_path = $fixtureScenePath
        bin_path = $fixtureBinPath
        removed_node_index = $removedNodeIndex
    }
}

$repoRoot = (Resolve-Path -LiteralPath ".").Path
$runtimeRoot = Resolve-RepoPath -Path "glTFRenderer" -RepoRoot $repoRoot -RequireExists
$sourceScenePath = Resolve-RepoPath -Path $SourceScene -RepoRoot $repoRoot -RequireExists
$exeFullPath = Resolve-RepoPath -Path $ExePath -RepoRoot $repoRoot -RequireExists
$outputBasePath = Resolve-RepoPath -Path $OutputBase -RepoRoot $repoRoot
$logDirPath = Resolve-RepoPath -Path $LogDir -RepoRoot $repoRoot

New-Item -ItemType Directory -Path $outputBasePath -Force | Out-Null
New-Item -ItemType Directory -Path $logDirPath -Force | Out-Null

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$sessionRoot = Join-Path $outputBasePath ("lantern_mr_" + $stamp)
New-Item -ItemType Directory -Path $sessionRoot -Force | Out-Null

$fixture = New-LanternFixture -SourceScenePath $sourceScenePath -SessionRoot $sessionRoot
$bakeOutputRoot = Join-Path $sessionRoot "bake_output"
$stdoutPath = Join-Path $logDirPath "lightingbaker_lantern_mr_$stamp.stdout.log"
$stderrPath = Join-Path $logDirPath "lightingbaker_lantern_mr_$stamp.stderr.log"

$arguments = @(
    "--scene", $fixture.scene_path,
    "--output", $bakeOutputRoot,
    "--samples-per-iteration", "1",
    "--target-samples", "1"
)

$process = Start-Process -FilePath $exeFullPath `
    -ArgumentList $arguments `
    -WorkingDirectory $runtimeRoot `
    -NoNewWindow `
    -PassThru `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
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

$manifestPath = Join-Path $bakeOutputRoot "manifest.json"
$sceneSummaryPath = Join-Path $bakeOutputRoot "debug\\raytracing_scene_summary.json"
$requiredArtifacts = @(
    $manifestPath,
    $sceneSummaryPath,
    (Join-Path $bakeOutputRoot "atlases\\indirect_00.rgba16f.bin"),
    (Join-Path $bakeOutputRoot "atlases\\indirect_00.master.rgba16f.bin"),
    (Join-Path $bakeOutputRoot "atlases\\indirect_00.preview.png"),
    (Join-Path $bakeOutputRoot "debug\\coverage_00.png"),
    (Join-Path $bakeOutputRoot "debug\\chart_id_00.png")
)

$missingArtifacts = @($requiredArtifacts | Where-Object { -not (Test-Path -LiteralPath $_) })
$metallicRoughnessTexturedInstanceCount = 0
if (Test-Path -LiteralPath $sceneSummaryPath) {
    $sceneSummary = [System.IO.File]::ReadAllText($sceneSummaryPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    $property = $sceneSummary.PSObject.Properties["metallic_roughness_textured_instance_count"]
    if ($null -ne $property) {
        $metallicRoughnessTexturedInstanceCount = [int]$property.Value
    }
}

$status = "RunFailed"
$pass = $false
if ($timedOut) {
    $status = "RunTimeout"
}
elseif ($exitCode -eq 0 -and $missingArtifacts.Count -eq 0 -and $metallicRoughnessTexturedInstanceCount -eq 2) {
    $status = "FixturePassed"
    $pass = $true
}
elseif ($exitCode -eq 0 -and $missingArtifacts.Count -gt 0) {
    $status = "ArtifactsMissing"
}
elseif ($exitCode -eq 0 -and $metallicRoughnessTexturedInstanceCount -ne 2) {
    $status = "UnexpectedSceneSummary"
}

$summaryObject = [ordered]@{
    status = $status
    pass = $pass
    exit_code = $exitCode
    duration_ms = [int]$stopwatch.ElapsedMilliseconds
    timed_out = $timedOut
    source_scene = Convert-ToRepoRelativePath -Path $sourceScenePath -RepoRoot $repoRoot
    fixture_scene = Convert-ToRepoRelativePath -Path $fixture.scene_path -RepoRoot $repoRoot
    fixture_bin = Convert-ToRepoRelativePath -Path $fixture.bin_path -RepoRoot $repoRoot
    bake_output_root = Convert-ToRepoRelativePath -Path $bakeOutputRoot -RepoRoot $repoRoot
    metallic_roughness_textured_instance_count = $metallicRoughnessTexturedInstanceCount
    removed_node_index = $fixture.removed_node_index
    stdout = Convert-ToRepoRelativePath -Path $stdoutPath -RepoRoot $repoRoot
    stderr = Convert-ToRepoRelativePath -Path $stderrPath -RepoRoot $repoRoot
    missing_artifacts = @($missingArtifacts | ForEach-Object { Convert-ToRepoRelativePath -Path $_ -RepoRoot $repoRoot })
}

$summaryJsonPath = Join-Path $sessionRoot "summary.json"
$summaryObject | ConvertTo-Json -Depth 16 | Out-File -LiteralPath $summaryJsonPath -Encoding utf8

Write-Host "STATUS=$status"
Write-Host "EXITCODE=$exitCode"
Write-Host "DURATION_MS=$([int]$stopwatch.ElapsedMilliseconds)"
Write-Host "SOURCE_SCENE=$(Convert-ToRepoRelativePath -Path $sourceScenePath -RepoRoot $repoRoot)"
Write-Host "FIXTURE_SCENE=$(Convert-ToRepoRelativePath -Path $fixture.scene_path -RepoRoot $repoRoot)"
Write-Host "OUTPUT_ROOT=$(Convert-ToRepoRelativePath -Path $bakeOutputRoot -RepoRoot $repoRoot)"
Write-Host "SUMMARY_JSON=$(Convert-ToRepoRelativePath -Path $summaryJsonPath -RepoRoot $repoRoot)"
Write-Host "STDOUT=$(Convert-ToRepoRelativePath -Path $stdoutPath -RepoRoot $repoRoot)"
Write-Host "STDERR=$(Convert-ToRepoRelativePath -Path $stderrPath -RepoRoot $repoRoot)"
Write-Host "MISSING_ARTIFACTS=$($missingArtifacts.Count)"
Write-Host "MR_TEXTURED_INSTANCES=$metallicRoughnessTexturedInstanceCount"

if ($pass) {
    exit 0
}

exit 1
