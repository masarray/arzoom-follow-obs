# Shared release-approval predicate. Do not infer approval from grep matches:
# the document may contain examples, instructions, or stale duplicated gates.
function Test-ArZoomReleaseApproval {
    [CmdletBinding()]
    param([Parameter(Mandatory=$true)][string] $AcceptanceText)

    $markers = @($AcceptanceText -split '\r?\n' |
        Where-Object { $_ -match '^ReleaseGate:' })
    return ($markers.Count -eq 1 -and
            $markers[0] -ceq 'ReleaseGate: APPROVED')
}
