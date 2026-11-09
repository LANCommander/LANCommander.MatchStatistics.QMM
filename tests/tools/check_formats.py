"""Checks the log lines the plugin parses against the games' own code.

The core and the modules read lines the games write with G_LogPrintf (see
src/core/logparse.h). This checks, for every game with a module:

  1. that its G_LogPrintf echoes each line to the console (G_Printf when
     g_dedicated), which is how the lines reach the plugin (a G_PRINT syscall);
  2. that every format the plugin parses for it is one the game writes,
     exactly.

The sources come from tests/tools/fetch_sources.sh (out/sources/<game>/).
Run with: python -I tests/tools/check_formats.py   (exit code = problems)
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCES = ROOT / "out" / "sources"

# What src/core/logparse.cpp and stats.cpp read from every Q3-family game.
CORE = [
    "Kill: %i %i %i: %s killed %s by %s\\n",
    "Exit: %s\\n",
    "Item: %i %s\\n",
]

# What each module reads on top of the core, as the game's source spells it.
MODULES = {
    "q3a": CORE,
    "rtcwmp": CORE,
    "wet": CORE + [
        "Medic_Revive: %d %d\\n",
        "Dynamite_Plant: %d\\n",
        "Dynamite_Diffuse: %d\\n",
        "Repair: %d\\n",
        "Health_Pack: %d %d\\n",
        "Ammo_Pack: %d %d\\n",
    ],
    "jk2mp": CORE,
    "jamp": CORE,
    "stvoyhm": CORE,
}

# The tags src/games/wet.cpp lists, which must match the formats above.
MODULE_SOURCES = {
    "wet": (ROOT / "src" / "games" / "wet.cpp", re.compile(r'\{\s*"(\w+)",\s*"\w+",\s*"\w+"\s*\}')),
}

LOG_PRINTF = re.compile(r'G_LogPrintf\s*\(\s*"((?:[^"\\]|\\.)*)"')
ECHO = re.compile(r"if\s*\(\s*g_dedicated\.integer\s*\)\s*\{?\s*G_Printf\s*\(", re.S)


def game_files(game):
    return [p for p in (SOURCES / game).rglob("*") if p.suffix in (".c", ".cpp")]


def main():
    problems = 0

    if not SOURCES.is_dir():
        print(f"No game sources in {SOURCES}: run tests/tools/fetch_sources.sh first")
        return 1

    for game, formats in MODULES.items():
        files = game_files(game)

        if not files:
            print(f"{game}: no sources (run tests/tools/fetch_sources.sh)")
            problems += 1
            continue

        written = set()
        echoes = False

        for path in files:
            text = path.read_text(encoding="latin-1")
            written.update(LOG_PRINTF.findall(text))

            # The body of G_LogPrintf itself.
            match = re.search(r"void\s+QDECL\s+G_LogPrintf\s*\([^)]*\)\s*\{(.*?)\n\}", text, re.S)
            if match and ECHO.search(match.group(1)):
                echoes = True

        if echoes:
            print(f"ok    {game}: G_LogPrintf echoes to the console")
        else:
            print(f"FAIL  {game}: G_LogPrintf does not echo to the console under g_dedicated")
            problems += 1

        for fmt in formats:
            if fmt in written:
                print(f"ok    {game}: {fmt}")
            else:
                print(f"FAIL  {game}: never writes {fmt}")
                problems += 1

    for game, (path, pattern) in MODULE_SOURCES.items():
        tags = pattern.findall(path.read_text(encoding="utf-8"))
        expected = {fmt.split(":")[0] for fmt in MODULES[game] if fmt not in CORE}

        for tag in tags:
            if tag in expected:
                print(f"ok    {path.name}: reads {tag}")
            else:
                print(f"FAIL  {path.name}: reads {tag}, which check_formats.py does not list")
                problems += 1

        for tag in expected - set(tags):
            print(f"FAIL  {path.name}: does not read {tag}, which check_formats.py lists")
            problems += 1

    print(f"{problems} problem(s)")
    return problems


if __name__ == "__main__":
    sys.exit(main())
