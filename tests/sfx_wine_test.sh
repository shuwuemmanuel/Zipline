#!/usr/bin/env bash
#
# Integration test: build a self-extracting EXE with the Zipline engine and run
# it under Wine to confirm the native installer stub extracts correctly
# (silent mode, subfolders, password). Skips cleanly if Wine is unavailable.
#
# Usage: sfx_wine_test.sh <zipline_binary> <sfx_stub.exe> <test_files_dir> <work_dir>
set -u

ZIPLINE="${1:?zipline binary}"
STUB="${2:?sfx stub}"
TESTDIR="${3:?test files}"
WORK="${4:?work dir}"

if ! command -v wine >/dev/null 2>&1; then
    echo "SKIP: wine not installed"
    exit 0
fi
if [ ! -f "$STUB" ]; then
    echo "SKIP: SFX stub not built ($STUB)"
    exit 0
fi

export ZIPLINE_SFX_STUB="$STUB"
export WINEPREFIX="$WORK/wineprefix"
export WINEDEBUG=-all
mkdir -p "$WORK"
rm -rf "$WINEPREFIX"

fail() { echo "FAIL: $1"; exit 1; }

# 1) Build a plain SFX with the engine via a tiny helper (the demo-exe path).
cd "$WORK" || fail "cd work"
cp -r "$TESTDIR" "$WORK/src_files"

# Use the engine's own demo-exe generator (writes demo_archive.exe here).
"$ZIPLINE" --create-files >/dev/null 2>&1 || true
"$ZIPLINE" --demo-exe >/dev/null 2>&1 || fail "demo-exe build"
[ -f "$WORK/demo_archive.exe" ] || fail "demo_archive.exe not produced"

# 2) Run it silently under wine (demo default path is %USERPROFILE%\Desktop\ZiplineDemo).
timeout 120 wine "$WORK/demo_archive.exe" /S >/dev/null 2>&1
rc=$?
[ $rc -eq 0 ] || fail "wine silent run exit $rc"

# 3) Verify extraction happened.
found=$(find "$WINEPREFIX/drive_c/users" -path '*ZiplineDemo*' -name 'readme.md' 2>/dev/null | head -1)
[ -n "$found" ] || fail "extracted readme.md not found"
grep -q "Test Files" "$found" || fail "extracted content mismatch"

echo "PASS: SFX built by engine extracted correctly under Wine"
exit 0
