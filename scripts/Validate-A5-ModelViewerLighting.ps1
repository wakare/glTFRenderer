param(
    [string[]]$Backends = @("dx", "vk"),
    [string]$OutputBase = ".tmp\\regression_validate_a5_model_viewer_lighting",
    [string]$Profile = "scripts\\RegressionCompareProfile.default.json",
    [switch]$DisableVisualCompare,
    [switch]$EnablePerfCompare
)

$validateScript = Join-Path $PSScriptRoot "Validate-RendererRegression.ps1"
if (-not (Test-Path -LiteralPath $validateScript)) {
    throw "Validate-RendererRegression.ps1 not found beside wrapper script: $validateScript"
}

$invokeArgs = @{
    Suite = "glTFRenderer\\RendererDemo\\Resources\\RegressionSuites\\model_viewer_lighting_a5_ibl_smoke.json"
    DemoApp = "DemoAppModelViewer"
    OutputBase = $OutputBase
    Profile = $Profile
    Backends = $Backends
}

if ($DisableVisualCompare) {
    $invokeArgs.DisableVisualCompare = $true
}

if (-not $EnablePerfCompare) {
    $invokeArgs.DisablePerfCompare = $true
}

& $validateScript @invokeArgs
exit $LASTEXITCODE
