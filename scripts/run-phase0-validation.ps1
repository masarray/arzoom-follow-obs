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

function Resolve-BenchmarkExecutable {
    param([Parameter(Mandatory = $true)][string] $Name)

    $candidates = @(
        (Join-Path $BuildPath "Release/$Name.exe"),
        (Join-Path $BuildPath "$Name.exe"),
        (Join-Path $BuildPath "Release/$Name"),
        (Join-Path $BuildPath $Name)
    )
    return $candidates |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1
}

Push-Location $RepoRoot
try {
    $sceneCamera = Join-Path $RepoRoot 'src/arzoom-scene-camera.cpp'
    if (-not (Test-Path -LiteralPath $sceneCamera -PathType Leaf)) {
        throw 'Phase 4 Scene Camera manager is missing.'
    }

    $forbiddenMutation = Select-String -LiteralPath $sceneCamera -Pattern 'obs_sceneitem_set_' -SimpleMatch
    if ($forbiddenMutation) {
        throw 'Phase 4 scene-safety contract violated: Scene Camera must not mutate scene-item transforms.'
    }

    $legacyCameraSource = Join-Path $RepoRoot 'src/arzoom-camera-source.cpp'
    if (Test-Path -LiteralPath $legacyCameraSource -PathType Leaf) {
        throw 'Phase 4 architecture regression: legacy custom ArZoom Camera input-source path is still present.'
    }

    if (Test-Path -LiteralPath $BuildPath) {
        Remove-Item -LiteralPath $BuildPath -Recurse -Force
    }

    Invoke-Checked cmake @('-S', 'tests', '-B', $BuildPath)
    Invoke-Checked cmake @('--build', $BuildPath, '--config', 'Release', '--parallel')
    Invoke-Checked ctest @('--test-dir', $BuildPath, '-C', 'Release', '--output-on-failure')

    if (-not $SkipBenchmark) {
        if (Test-Path -LiteralPath $BenchmarkOutput) {
            Remove-Item -LiteralPath $BenchmarkOutput -Force
        }

        $benchmarks = @(
            @{ Name = 'arzoom-motion-benchmark'; Heading = '=== Phase 0 v0.1.4 baseline camera ===' },
            @{ Name = 'arzoom-smart-camera-benchmark'; Heading = '=== Phase 1 Smart Zone camera ===' },
            @{ Name = 'arzoom-click-benchmark'; Heading = '=== Phase 2 fixed-state click visualization ===' },
            @{ Name = 'arzoom-presenter-benchmark'; Heading = '=== Phase 3 presenter controls ===' }
        )

        foreach ($benchmark in $benchmarks) {
            $benchmarkExe = Resolve-BenchmarkExecutable -Name $benchmark.Name
            if (-not $benchmarkExe) {
                throw "$($benchmark.Name) executable was not generated."
            }

            $benchmark.Heading | Tee-Object -FilePath $BenchmarkOutput -Append
            Write-Host "> $benchmarkExe"
            & $benchmarkExe 2>&1 | Tee-Object -FilePath $BenchmarkOutput -Append
            if ($LASTEXITCODE -ne 0) {
                throw "$($benchmark.Name) failed with exit code $LASTEXITCODE"
            }
            '' | Tee-Object -FilePath $BenchmarkOutput -Append
        }

        Write-Host "Benchmark report: $BenchmarkOutput"
    }

    Write-Host 'ArZoom deterministic Phase 0/1/2/3/3.5/4 validation: PASS'
}
finally {
    Pop-Location
}
