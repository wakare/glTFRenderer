param(
    [string]$DocRoot = "Doc",
    [string]$RepoRoot = "."
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-ReferenceParts {
    param(
        [string]$Token
    )

    $result = @{
        Path = $Token
        Line = $null
    }

    if ($Token -match "^(?<path>.+)#L(?<line>\d+)(?:C\d+)?$") {
        $result.Path = $Matches["path"]
        $result.Line = [int]$Matches["line"]
        return $result
    }

    if ($Token -match "^(?<path>.+):(?<line>\d+)$") {
        $result.Path = $Matches["path"]
        $result.Line = [int]$Matches["line"]
    }

    return $result
}

function Resolve-ReferencedPath {
    param(
        [string]$RawPath,
        [string]$RepoRootFullPath,
        [string]$SourceDocDirectory
    )

    if ([string]::IsNullOrWhiteSpace($RawPath)) {
        return $null
    }

    if ([System.IO.Path]::IsPathRooted($RawPath)) {
        if (Test-Path -LiteralPath $RawPath) {
            return (Resolve-Path -LiteralPath $RawPath).Path
        }
        return $null
    }

    $candidates = @(
        (Join-Path $RepoRootFullPath $RawPath),
        (Join-Path $SourceDocDirectory $RawPath),
        (Join-Path $RepoRootFullPath ("glTFRenderer/" + $RawPath))
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return $null
}

function Get-LineCount {
    param(
        [string]$FilePath,
        [hashtable]$Cache
    )

    if (-not $Cache.ContainsKey($FilePath)) {
        $Cache[$FilePath] = (Get-Content -LiteralPath $FilePath | Measure-Object -Line).Lines
    }
    return [int]$Cache[$FilePath]
}

$repoRootFullPath = (Resolve-Path -LiteralPath $RepoRoot).Path
$docRootFullPath = Join-Path $repoRootFullPath $DocRoot

if (-not (Test-Path -LiteralPath $docRootFullPath)) {
    Write-Host "[DocRef][Error] Doc root not found: $docRootFullPath"
    exit 2
}

$markdownFiles = Get-ChildItem -LiteralPath $docRootFullPath -Recurse -Filter *.md -File

$lineCountCache = @{}
$issues = New-Object System.Collections.Generic.List[object]
$referenceCount = 0

foreach ($md in $markdownFiles) {
    $content = Get-Content -LiteralPath $md.FullName -Raw
    $contentWithoutCodeFence = [regex]::Replace($content, '(?s)\x60\x60\x60.*?\x60\x60\x60', '')
    $matches = [regex]::Matches($contentWithoutCodeFence, '(?<!\x60)\x60([^\x60\r\n]+)\x60(?!\x60)')

    foreach ($match in $matches) {
        $token = $match.Groups[1].Value.Trim()

        if ([string]::IsNullOrWhiteSpace($token)) {
            continue
        }

        if ($token -match "^\w+://") {
            continue
        }

        if ($token.Contains("*") -or $token.Contains("?")) {
            continue
        }

        if ($token -notmatch "[\\/]" -or $token -notmatch "\.[A-Za-z0-9]+") {
            continue
        }

        $parts = Get-ReferenceParts -Token $token
        $referenceCount++

        $resolvedPath = Resolve-ReferencedPath `
            -RawPath $parts.Path `
            -RepoRootFullPath $repoRootFullPath `
            -SourceDocDirectory $md.DirectoryName

        if ($null -eq $resolvedPath) {
            $issues.Add([PSCustomObject]@{
                Doc = $md.FullName
                Ref = $token
                Reason = "FileNotFound"
            })
            continue
        }

        if ($null -ne $parts.Line) {
            $lineCount = Get-LineCount -FilePath $resolvedPath -Cache $lineCountCache
            if ($parts.Line -lt 1 -or $parts.Line -gt $lineCount) {
                $issues.Add([PSCustomObject]@{
                    Doc = $md.FullName
                    Ref = $token
                    Reason = "LineOutOfRange (max line: $lineCount)"
                })
            }
        }
    }
}

Write-Host ("[DocRef] Scanned {0} markdown files, checked {1} references, found {2} issue(s)." -f $markdownFiles.Count, $referenceCount, $issues.Count)

if ($issues.Count -gt 0) {
    foreach ($issue in $issues) {
        $docRelative = $issue.Doc.Replace($repoRootFullPath + [System.IO.Path]::DirectorySeparatorChar, "")
        Write-Host ('[DocRef][Error] {0} -> {1}: {2}' -f $docRelative, $issue.Ref, $issue.Reason)
    }
    exit 1
}

exit 0
