"""Static regression guard: latest EXE/ZIP URLs must remain version-independent."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = "https://github.com/masarray/arzoom-follow-obs/releases/latest/download/"
EXE = BASE + "ArZoom-OBS-Setup-windows-x64.exe"
ZIP = BASE + "ArZoom-OBS-Manual-windows-x64.zip"
SUMS = BASE + "SHA256SUMS.txt"

for name in ("README.md", "docs/index.html", "docs/guide.html"):
    text = (ROOT / name).read_text(encoding="utf-8")
    assert EXE in text, f"{name}: no direct latest-stable installer"
    assert ZIP in text, f"{name}: no direct latest-stable ZIP"
    assert SUMS in text, f"{name}: missing latest SHA256SUMS link"
    assert "/releases/download/v0." not in text, f"{name}: stale version-locked download link"
assert EXE in (ROOT / "docs/troubleshooting.html").read_text(encoding="utf-8")
landing = (ROOT / "docs/index.html").read_text(encoding="utf-8")
assert 'src="latest-release.js"' in landing
assert "data-latest-stable-version" in landing
assert '/releases/latest/download/' in landing
release = (ROOT / ".github/workflows/release.yml").read_text(encoding="utf-8")
assert 'ArZoom-OBS-Manual-windows-x64.zip' in release
assert 'Copy-Item -LiteralPath $manualZip -Destination $latestManualZip -Force' in release
assert 'release/ArZoom-OBS-Manual-windows-x64.zip' in release
sync = (ROOT / ".github/workflows/sync-stable-downloads.yml").read_text(encoding="utf-8")
assert 'contents: write' in sync and 'scripts/sync-latest-stable-zip.sh' in sync
backfill = (ROOT / "scripts/sync-latest-stable-zip.sh").read_text(encoding="utf-8")
assert 'gh api "repos/$repo/releases/latest"' in backfill
assert 'sha256sum' in backfill and '--pattern "$alias"' in backfill
print("[PASS] direct latest-stable EXE/ZIP/checksum URLs and release alias contracts")
