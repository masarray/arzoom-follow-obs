[CmdletBinding()]
param(
    [string] $BuildDir = '.phase0-tests',
    [switch] $SkipBenchmark
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$BuildPath = Join-Path $RepoRoot $BuildDir
$BenchmarkOutput = Join-Path $RepoRoot 'phase0-benchmark.txt'

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string] $FilePath,
        [string[]] $Arguments = @()
    )

    Write-Host "> $FilePath $($Arguments -join ' ')"
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

Push-Location $RepoRoot
try {
    if (Test-Path -LiteralPath $BuildPath) {
        Remove-Item -LiteralPath $BuildPath -Recurse -Force
    }

    Invoke-Checked cmake @('-S', 'tests', '-B', $BuildPath)
    Invoke-Checked cmake @('--build', $BuildPath, '--config', 'Release', '--parallel')
    Invoke-Checked ctest @('--test-dir', $BuildPath, '-C', 'Release', '--output-on-failure')

    if (-not $SkipBenchmark) {
        $benchmarkCandidates = @(
            (Join-Path $BuildPath 'Release/arzoom-motion-benchmark.exe'),
            (Join-Path $BuildPath 'arzoom-motion-benchmark.exe'),
            (Join-Path $BuildPath 'Release/arzoom-motion-benchmark'),
            (Join-Path $BuildPath 'arzoom-motion-benchmark')
        )
        $benchmarkExe = $benchmarkCandidates |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
            Select-Object -First 1

        if (-not $benchmarkExe) {
            throw 'arzoom-motion-benchmark executable was not generated.'
        }

        Write-Host "> $benchmarkExe"
        & $benchmarkExe 2>&1 | Tee-Object -FilePath $BenchmarkOutput
        if ($LASTEXITCODE -ne 0) {
            throw "arzoom-motion-benchmark failed with exit code $LASTEXITCODE"
        }

        Write-Host "Benchmark report: $BenchmarkOutput"
    }

    Write-Host 'ArZoom Phase 0 validation: PASS'
}
finally {
    Pop-Location
}
