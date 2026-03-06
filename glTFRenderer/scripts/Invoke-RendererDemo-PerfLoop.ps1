param(
    [string]$SuitePath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'RendererDemo\Resources\RegressionSuites\frosted_glass_b9_smoke.json'),
    [ValidateSet('Mailbox', 'VSync')]
    [string]$PresentMode = 'Mailbox',
    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64',
    [string]$ExePath = '',
    [string]$OutputRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build_logs\perf_loop'),
    [int]$BuildTimeoutSec = 900,
    [int]$RunTimeoutSec = 600,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedSuitePath = (Resolve-Path $SuitePath).Path
$resolvedOutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$sessionDir = Join-Path $resolvedOutputRoot "session_$stamp"
$buildLogDir = Join-Path $sessionDir 'build'
$runLogDir = Join-Path $sessionDir 'runs'
$comparisonDir = Join-Path $sessionDir 'comparison'
$regressionOutputRoot = Join-Path $sessionDir 'regression_outputs'

New-Item -ItemType Directory -Path $sessionDir -Force | Out-Null
New-Item -ItemType Directory -Path $buildLogDir -Force | Out-Null
New-Item -ItemType Directory -Path $runLogDir -Force | Out-Null
New-Item -ItemType Directory -Path $comparisonDir -Force | Out-Null
New-Item -ItemType Directory -Path $regressionOutputRoot -Force | Out-Null

$resolvedExePath = $ExePath
if (-not $SkipBuild) {
    $buildScript = Join-Path $PSScriptRoot 'Build-RendererDemo-Verify.ps1'
    $buildOutput = & $buildScript `
        -Configuration $Configuration `
        -Platform $Platform `
        -LogDir $buildLogDir `
        -TimeoutSec $BuildTimeoutSec

    $buildResultPath = (($buildOutput | Where-Object { $_ -like 'RESULT_JSON=*' }) | Select-Object -Last 1)
    if (-not $buildResultPath) {
        throw 'Build script did not return RESULT_JSON.'
    }
    $buildResultJsonPath = $buildResultPath.Split('=', 2)[1]
    $buildResult = Get-Content $buildResultJsonPath -Raw | ConvertFrom-Json
    if ($buildResult.status -ne 'SUCCESS') {
        throw "Build failed. Result: $buildResultJsonPath"
    }

    if (-not $resolvedExePath) {
        $resolvedExePath = $buildResult.output_exe
    }
}

$runScript = Join-Path $PSScriptRoot 'Run-RendererDemo-Regression.ps1'
$dxOutput = & $runScript `
    -Api DX12 `
    -SuitePath $resolvedSuitePath `
    -PresentMode $PresentMode `
    -ExePath $resolvedExePath `
    -OutputRoot $regressionOutputRoot `
    -LogDir $runLogDir `
    -Configuration $Configuration `
    -TimeoutSec $RunTimeoutSec

$dxResultPathLine = (($dxOutput | Where-Object { $_ -like 'RESULT_JSON=*' }) | Select-Object -Last 1)
if (-not $dxResultPathLine) {
    throw 'DX12 regression run did not return RESULT_JSON.'
}
$dxResultPath = $dxResultPathLine.Split('=', 2)[1]

$vkOutput = & $runScript `
    -Api Vulkan `
    -SuitePath $resolvedSuitePath `
    -PresentMode $PresentMode `
    -ExePath $resolvedExePath `
    -OutputRoot $regressionOutputRoot `
    -LogDir $runLogDir `
    -Configuration $Configuration `
    -TimeoutSec $RunTimeoutSec

$vkResultPathLine = (($vkOutput | Where-Object { $_ -like 'RESULT_JSON=*' }) | Select-Object -Last 1)
if (-not $vkResultPathLine) {
    throw 'Vulkan regression run did not return RESULT_JSON.'
}
$vkResultPath = $vkResultPathLine.Split('=', 2)[1]

$compareScript = Join-Path $PSScriptRoot 'Compare-RendererDemo-Regression.ps1'
$compareOutput = & $compareScript -DxManifestPath $dxResultPath -VkManifestPath $vkResultPath -OutputDir $comparisonDir

$jsonReportLine = (($compareOutput | Where-Object { $_ -like 'JSON_REPORT=*' }) | Select-Object -Last 1)
$mdReportLine = (($compareOutput | Where-Object { $_ -like 'MD_REPORT=*' }) | Select-Object -Last 1)

if (-not $jsonReportLine -or -not $mdReportLine) {
    throw 'Comparison script did not return report paths.'
}

$jsonReportPath = $jsonReportLine.Split('=', 2)[1]
$mdReportPath = $mdReportLine.Split('=', 2)[1]

Write-Output "STATUS=SUCCESS"
Write-Output "SESSION_DIR=$sessionDir"
Write-Output "DX_RESULT_JSON=$dxResultPath"
Write-Output "VK_RESULT_JSON=$vkResultPath"
Write-Output "JSON_REPORT=$jsonReportPath"
Write-Output "MD_REPORT=$mdReportPath"
