[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Git was not found in PATH.'
}

$repoRoot = (& git rev-parse --show-toplevel 2>$null).Trim()
if (-not $repoRoot) {
    throw 'Run this script from inside the ArZoom Git repository.'
}

Push-Location $repoRoot
try {
    $trackedIgnored = @(& git ls-files -ci --exclude-standard)
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to enumerate tracked files now covered by .gitignore.'
    }

    if ($trackedIgnored.Count -eq 0) {
        Write-Host '[OK] No ignored build/patch artifacts are tracked.' -ForegroundColor Green
        exit 0
    }

    Write-Host "Removing $($trackedIgnored.Count) ignored artifact(s) from the Git index..." -ForegroundColor Yellow
    foreach ($path in $trackedIgnored) {
        if ([string]::IsNullOrWhiteSpace($path)) {
            continue
        }

        & git rm --cached --ignore-unmatch -- $path
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to untrack: $path"
        }
    }

    Write-Host '[OK] Generated files are no longer tracked.' -ForegroundColor Green
    Write-Host 'Local files were preserved. Review git status, then commit the cleanup.'
}
finally {
    Pop-Location
}
