#!/usr/bin/env bash
# Fetches the game code of every game with a module, into out/sources/<game>/,
# for tests/tools/check_formats.py. Sparse, shallow clones: only the game code
# directories, a few MB each. The SDK headers in external/qmm_sdks have no .c
# files, so the log formats have to come from the games' released sources.
#
#   bash tests/tools/fetch_sources.sh

set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$root/out/sources"
cd "$root/out/sources"

clone() {
    local name=$1 url=$2
    shift 2

    if [ -d "$name" ]; then
        echo "have $name"
        return
    fi

    git clone -q --depth 1 --filter=blob:none --sparse "$url" "$name"
    git -C "$name" sparse-checkout set --no-cone "$@"
    echo "got  $name"
}

clone q3a     https://github.com/id-Software/Quake-III-Arena '/code/game/*'
clone rtcwmp  https://github.com/id-Software/RTCW-MP         '/src/game/*'
clone wet     https://github.com/id-Software/Enemy-Territory '/src/game/*'
clone jk2mp   https://github.com/zachlatta/jedi-outcast      '/CODE-mp/game/*'
clone jamp    https://github.com/grayj/Jedi-Academy          '/codemp/game/*'
# Raven's Elite Force Holomatch game code, as lilium-voyager carries it.
clone stvoyhm https://github.com/clover-moe/lilium-voyager   '/code/game/*'
# The engines LANCommander's images run, for check_formats.py's VARIANTS.
clone openjk   https://github.com/JACoders/OpenJK            '/codemp/game/*'
clone etlegacy https://github.com/etlegacy/etlegacy          '/src/game/*'
# OpenMoHAA's reimplementation: MOHAA's own game code was never released.
clone mohaa   https://github.com/openmoh/openmohaa           '/code/fgame/*'
