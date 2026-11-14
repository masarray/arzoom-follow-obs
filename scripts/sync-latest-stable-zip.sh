#!/usr/bin/env bash
set -euo pipefail

# One-time backfill for the existing stable release; future releases attach the
# fixed alias in release.yml. Never change /latest, prereleases, or binaries.
repo="${GITHUB_REPOSITORY:?missing GitHub repository}"
[[ "$repo" == "masarray/arzoom-follow-obs" ]] || { echo "Unexpected repository: $repo" >&2; exit 1; }
version="$(python3 -c 'import json; print(json.load(open("buildspec.json", encoding="utf-8"))["version"])')"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Invalid stable version" >&2; exit 1; }
tag="v$version"
latest="$(gh api "repos/$repo/releases/latest" --jq .tag_name)"
[[ "$latest" == "$tag" ]] || { echo "FAIL CLOSED: latest is $latest, buildspec is $tag; no release mutation" >&2; exit 1; }

release="$(gh release view "$tag" --repo "$repo" --json isDraft,isPrerelease,assets)"
python3 -c 'import json,sys; d=json.loads(sys.argv[1]); assert not d["isDraft"] and not d["isPrerelease"], "Must be public stable"' "$release"
source="ArZoom-OBS-Manual-v${version}-windows-x64.zip"
alias="ArZoom-OBS-Manual-windows-x64.zip"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

gh release download "$tag" --repo "$repo" --pattern "$source" --dir "$work"
gh release download "$tag" --repo "$repo" --pattern SHA256SUMS.txt --dir "$work"
# SHA256SUMS.txt was emitted on Windows (CRLF). Strip CR before awk compares
# the filename; otherwise the final field includes a hidden carriage return.
expected="$(tr -d '\r' < "$work/SHA256SUMS.txt" | awk -v f="$source" '$2==f {print tolower($1)}')"
[[ "$expected" =~ ^[a-f0-9]{64}$ ]] || { echo "Missing validated source SHA256 entry" >&2; exit 1; }
actual="$(sha256sum "$work/$source" | cut -d' ' -f1)"
[[ "$actual" == "$expected" ]] || { echo "Versioned ZIP SHA256 mismatch" >&2; exit 1; }
cp "$work/$source" "$work/$alias"

if gh release view "$tag" --repo "$repo" --json assets --jq '.assets[].name' | grep -Fxq "$alias"; then
  echo "Stable alias exists: verifying immutable contents instead of overwriting"
else
  gh release upload "$tag" "$work/$alias" --repo "$repo"
fi
mkdir "$work/verify"
gh release download "$tag" --repo "$repo" --pattern "$alias" --dir "$work/verify"
[[ "$(sha256sum "$work/verify/$alias" | cut -d' ' -f1)" == "$expected" ]] || { echo "Published latest ZIP alias hash mismatch" >&2; exit 1; }
echo "Verified stable ZIP alias for $tag with SHA256 $expected"
