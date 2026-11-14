#!/usr/bin/env bash
# Lays out a release: every plugin binary under qmmaddons/lancommander/, as a
# game directory wants it, with the sample config and the README.
#
#   bash scripts/package.sh <artifacts dir> <output dir>
#
# <artifacts dir> holds the CI's per-preset artifacts (windows-x86/, linux-x64/,
# ...) or any tree with the built lancommander_qmm_* binaries in it.

set -euo pipefail

artifacts=${1:?artifacts directory}
out=${2:?output directory}
root="$(cd "$(dirname "$0")/.." && pwd)"

rm -rf "$out"
mkdir -p "$out/qmmaddons/lancommander"

count=0
while IFS= read -r -d '' binary; do
    cp "$binary" "$out/qmmaddons/lancommander/"
    count=$((count + 1))
done < <(find "$artifacts" -type f \( -name 'lancommander_qmm_*.dll' -o -name 'lancommander_qmm_*.so' \) -print0)

if [ "$count" -eq 0 ]; then
    echo "No plugin binaries under $artifacts" >&2
    exit 1
fi

# QMM2 itself, from the CI's qmm2 jobs: our fork's lancommander branch (the
# console fix, and the vmMain-field fix the CoD module needs), until those reach
# an upstream release. Linux is built on Debian 12 because QMM2's own release
# needs a newer glibc; Windows is the MSVC build.
qmm2_linux=$(find "$artifacts" -type f -name 'qmm2_x86_64.so' -print -quit)

if [ -n "$qmm2_linux" ]; then
    mkdir -p "$out/qmm2/linux"
    cp "$(dirname "$qmm2_linux")"/* "$out/qmm2/linux/"
    echo "Packaged QMM2 for Linux from $(dirname "$qmm2_linux")"
fi

qmm2_windows=$(find "$artifacts" -type f -name 'qmm2_x86_64.dll' -print -quit)

if [ -n "$qmm2_windows" ]; then
    mkdir -p "$out/qmm2/windows"
    cp "$(dirname "$qmm2_windows")"/* "$out/qmm2/windows/"
    echo "Packaged QMM2 for Windows from $(dirname "$qmm2_windows")"
fi

cp "$root/configs/lancommander.cfg" "$out/"
cp "$root/README.md" "$root/LICENSE" "$out/"

echo "Packaged $count plugin binaries into $out"
