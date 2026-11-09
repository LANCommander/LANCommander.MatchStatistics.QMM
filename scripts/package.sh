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

cp "$root/configs/lancommander.cfg" "$out/"
cp "$root/README.md" "$root/LICENSE" "$out/"

echo "Packaged $count plugin binaries into $out"
