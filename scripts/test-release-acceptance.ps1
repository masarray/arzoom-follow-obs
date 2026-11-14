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
if (Test-ArZoomReleaseApproval -AcceptanceText $actualDoc) {
    throw 'Preview is incorrectly marked release-approved'
}

$workflow = Get-Content -LiteralPath (Join-Path $PSScriptRoot '../.github/workflows/release.yml') -Raw
if ($workflow -match '(?m)^  push:' -or
    $workflow -notmatch 'approve_stable_release' -or
    $workflow -notmatch 'tested_commit_sha' -or
    $workflow -notmatch 'Test-ArZoomReleaseApproval') {
    throw 'Release workflow no longer enforces explicit manual approval'
}
Write-Host '[PASS] Release gate fails closed: six cases + preview/trigger safeguards.'
