"""Checks src/games/cod.cpp against Call of Duty's own gametype scripts.

CoD's rules are script (maps/mp/gametypes/*.gsc), shipped only in the game's
pk3 files, so this reads them from an installed game rather than a clone:

    python -I tests/tools/check_cod_formats.py "D:/Games/Call of Duty"

It reads Main/ (1.5) and uo/ (United Offensive), the newest pk3 winning for
each script, as the game loads them, and checks:

  1. that every K and D line has cod.cpp's 13 fields, in its order;
  2. that every A line is <guid>;<num>;<team>;<name>;<action>, and that its
     action is one kActions lists (or one IGNORED names);
  3. that W lines come from endMap in the gametypes EndsMapOnWin names and
     from endRound in all the others (whose rounds restart the map).

Exit code = problems.
"""

import pathlib
import re
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
MODULE = ROOT / "src" / "games" / "cod.cpp"

KILL_FIELDS = ("lpselfguid", "lpselfnum", "lpselfteam", "lpselfname", "lpattackguid", "lpattacknum",
               "lpattackerteam", "lpattackname", "sWeapon", "iDamage", "sMeansOfDeath", "sHitLoc")

# Actions cod.cpp leaves out on purpose.
IGNORED = {"bel_alive_tick"}

LOGPRINT = re.compile(r'logPrint\((.*)\);')
FUNCTION = re.compile(r'^([A-Za-z_]\w*)\s*\(')


def scripts(game_dir):
    """The gametype scripts the game would load: the newest pk3 wins."""
    found = {}
    for pk3 in sorted(game_dir.glob("*.pk3"), key=lambda p: p.name.lower()):
        with zipfile.ZipFile(pk3) as z:
            for name in z.namelist():
                lower = name.lower()
                if lower.startswith("maps/mp/gametypes/") and lower.endswith(".gsc"):
                    found[lower.rsplit("/", 1)[1]] = (pk3.name, z.read(name).decode("latin-1"))
    return found


def terms(expression):
    """The '+'-joined parts of a logPrint argument, spaces and quotes kept."""
    return [part.strip() for part in expression.split("+")]


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1

    install = pathlib.Path(sys.argv[1])
    module = MODULE.read_text(encoding="utf-8")
    actions = set(re.findall(r'\{\s*"(\w+)",\s*"\w+",\s*(?:"\w+"|nullptr)\s*\}', module))
    map_enders = set(re.search(r"bool EndsMapOnWin.*?\{(.*?)\n\}", module, re.S).group(1).split('"')[1::2])

    problems = 0
    checked = 0

    for sub in ("Main", "uo"):
        game_dir = install / sub
        if not game_dir.is_dir():
            continue

        for gsc, (pk3, text) in sorted(scripts(game_dir).items()):
            gametype = gsc[:-4]
            function = None
            label = f"{sub}/{pk3}/{gsc}"

            for line in text.splitlines():
                m = FUNCTION.match(line)
                if m:
                    function = m.group(1)

                m = LOGPRINT.search(line)
                if not m:
                    continue

                parts = terms(m.group(1))
                head = parts[0].strip('"')

                if head in ("K;", "D;"):
                    checked += 1
                    fields = [p for p in parts[1:] if p not in ('";"', '"\\n"')]
                    names = [re.sub(r"(?i)^lp(self|attack)guid$", lambda g: f"lp{g.group(1).lower()}guid", f) for f in fields]
                    expected = [f.lower() for f in KILL_FIELDS]
                    if [n.lower() for n in names] != expected:
                        print(f"FAIL  {label}: {head} fields {fields}")
                        problems += 1

                elif head == "A;":
                    checked += 1
                    action = parts[-2].strip('"').strip(";") if parts[-1] == '"\\n"' else parts[-1].strip('"').rstrip("\\n").split(";")[-1]
                    if len([p for p in parts if p == '";"']) < 3 and '";allies;"' not in parts:
                        print(f"FAIL  {label}: A line without guid;num;team;name: {m.group(1)}")
                        problems += 1
                    if action not in actions and action not in IGNORED:
                        print(f"FAIL  {label}: action {action} is not in cod.cpp's kActions")
                        problems += 1

                elif head.startswith("W;"):
                    checked += 1
                    expected = "endMap" if gametype in map_enders else "endRound"
                    if function != expected:
                        print(f"FAIL  {label}: W line in {function}, cod.cpp expects {expected}")
                        problems += 1

    if checked == 0:
        print(f"No CoD gametype scripts under {install} (Main/ or uo/)")
        return 1

    print(f"{checked} log lines checked, {problems} problem(s)")
    return problems


if __name__ == "__main__":
    sys.exit(main())
