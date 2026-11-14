$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'release-acceptance.ps1')

$cases = @(
    @{ Name = 'BLOCKED'; Text = 'ReleaseGate: BLOCKED'; Expected = $false },
    @{ Name = 'APPROVED'; Text = 'ReleaseGate: APPROVED'; Expected = $true },
    @{ Name = 'BLOCKED plus example'; Text = "ReleaseGate: BLOCKED`nReleaseGate: APPROVED"; Expected = $false },
    @{ Name = 'duplicate APPROVED'; Text = "ReleaseGate: APPROVED`nReleaseGate: APPROVED"; Expected = $false },
    @{ Name = 'missing marker'; Text = 'No approval recorded'; Expected = $false },
    @{ Name = 'wrong case'; Text = 'ReleaseGate: Approved'; Expected = $false }
)
foreach ($case in $cases) {
    $actual = Test-ArZoomReleaseApproval -AcceptanceText $case.Text
    if ($actual -ne $case.Expected) {
        throw "Release gate regression: $($case.Name) expected $($case.Expected)"
    }
}

$actualPath = Join-Path $PSScriptRoot '../docs/RELEASE_ACCEPTANCE_v0.7.2.md'
$actualDoc = Get-Content -LiteralPath $actualPath -Raw
$actualMarkers = @($actualDoc -split '\r?\n' |
    Where-Object { $_ -match '^ReleaseGate:' })
if ($actualMarkers.Count -ne 1 -or
    $actualMarkers[0] -notin @('ReleaseGate: BLOCKED', 'ReleaseGate: APPROVED')) {
    throw 'Actual release checklist must have exactly one valid gate line.'
}
# Unlike a test that hardcodes BLOCKED forever, this permits real owner
# approval after field acceptance without breaking the normal CI suite.

$workflow = Get-Content -LiteralPath (Join-Path $PSScriptRoot '../.github/workflows/release.yml') -Raw
if ($workflow -match '(?m)^  push:' -or
    $workflow -notmatch 'approve_stable_release' -or
    $workflow -notmatch 'tested_commit_sha' -or
    $workflow -notmatch 'Test-ArZoomReleaseApproval') {
    throw 'Release workflow no longer enforces explicit manual approval'
}
Write-Host '[PASS] Release gate fails closed: six cases + preview/trigger safeguards.'
