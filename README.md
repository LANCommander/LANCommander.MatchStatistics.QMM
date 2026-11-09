# LANCommander Match Statistics for QMM2

Reports the matches played on a Quake III-era game server to [LANCommander](https://github.com/LANCommander/LANCommander)'s
match statistics. Each report covers who played, what happened (kills, objectives, chat) and how the
match ended. LANCommander builds leaderboards from these reports and links in-game players to its
users.

It is a plugin for [QMM2](https://github.com/thecybermind/qmm2), the plugin manager for id Tech 2 and
3 servers. It is the sibling of [LANCommander.SourceMod.MatchStatistics](https://github.com/LANCommander/LANCommander.SourceMod.MatchStatistics)
and reports the same events and statistics under the same names.

Delivery is handled by the [LANCommander Interposer](https://github.com/LANCommander/LANCommander.Interposer),
which is built into the plugin.

- It sends events in batches.
- It retries while LANCommander can't be reached.
- It keeps anything unsent on disk, so a crash, restart or network outage loses nothing.

## Games

| Game | QMM code | What is recorded |
|---|---|---|
| **Quake III Arena** / Team Arena | `Q3A` | Kills (weapon, suicides, world deaths, team kills), scores, team scores and standings. Flag captures, assists, defends, medals (impressive, excellent, humiliation), item pickups (optional). |
| **Wolfenstein: Enemy Territory** | `WET` | Kills, scores and the map's winner, Axis against Allies. Revives, dynamite plants and defuses, MG42 repairs, health and ammo packs handed out. |
| **Return to Castle Wolfenstein** (MP) | `RTCWMP` | Kills, scores, the map's winner. |
| **Jedi Knight II: Jedi Outcast** (MP) | `JK2MP` | As Quake III: kills, scores, team scores, captures, assists, defends, medals. |
| **Jedi Knight: Jedi Academy** (MP) | `JAMP` | As Quake III. NPC deaths are left out. |
| **Star Trek: Voyager Elite Force** (Holomatch) | `STVOYHM` | Kills, scores, team scores, medals. |
| **Medal of Honor: Allied Assault** | `MOHAA` | Kills and deaths, and kill events where one killer and one victim can be paired (the game logs no kills). Teams. Players are ranked by kills. |
| Every other QMM multiplayer game: CoD, CoD:UO, CoD 1.1, SoF2, MOH:SH, MOH:BT, Elite Force II, Quake II, Quake II Remastered, SiN, SoF, Kingpin | | The match, the players joining, leaving and renaming, and chat. No kills or standings. |

QMM2's single-player games are not supported, because a match there means nothing to LANCommander.

**What has been run.** Quake III Arena (ioq3, x64, Windows) has been run end to end with bots. The
other modules are checked against the games' released code (see [Building](#building)) but have not
been run on a server yet.

## Installing

1. **Install QMM2** in the game's mod directory. See its
   [Installation](https://github.com/thecybermind/qmm2/wiki/Installation) page: QMM takes the mod
   DLL's file name and loads the original mod itself.
2. **Copy the plugin.** Put the release's `qmmaddons/` folder into the mod directory (`baseq3/`,
   `etmain/`, ...). Add the binary for the game and the server's architecture to the `plugins` list
   in `qmm2.json`:

   ```json
   {
     "plugins": [ "qmmaddons/lancommander/lancommander_qmm_WET.so" ]
   }
   ```

   The binaries are named `lancommander_qmm_<GAME>` for 32-bit servers and
   `lancommander_qmm_x86_64_<GAME>` for 64-bit ones, with `.dll` or `.so`. They are named the
   way stub_qmm names its builds. Most of these games only have 32-bit servers.
3. **Create a token.** In LANCommander, open **Profile → Access Tokens** and create a token with
   **Report matches** (the `matches:report` scope). Create it under an administrator account if the
   reports should count as authoritative.
4. **Configure the server.** Copy `lancommander.cfg` next to `server.cfg`, fill it in, and
   `exec lancommander.cfg` from `server.cfg`:

   ```
   set lc_server_url "http://lancommander.lan:1337"
   set lc_token      "lc_pat_..."
   set lc_game_id    "6f1e2c64-0d47-4b8e-9a43-3a1c2bde0f11"
   ```

   **Use `set`.** The cvars only exist once the plugin has registered them, at the first map load,
   after `server.cfg` has run. A bare `lc_token "..."` line in `server.cfg` is an unknown command.
5. **Start the server.** `lc_status` on the server console (or through rcon) shows the match being
   recorded and how delivery is going.

**Linux servers can only use `http://`.** The Interposer's Linux HTTP client has no TLS.
LANCommander on a LAN is normally plain HTTP.

**Linux needs glibc 2.36 or newer** (Debian 12, Ubuntu 22.10 or later, and LANCommander's own game
server images).

## Configuration

### Where reports go

| Cvar | Default | |
|---|---|---|
| `lc_enabled` | `1` | `0` records nothing. |
| `lc_server_url` | | The LANCommander server. |
| `lc_token` | | A personal access token with the `matches:report` scope. `%VARIABLE%` reads an environment variable, so the token can be kept out of the config file. |
| `lc_game_id` | | The game's id in LANCommander (a GUID). |
| `lc_server_id` | | This server's id in LANCommander, if LANCommander manages it. |
| `lc_reporter_name` | `sv_hostname` | How the server is named in LANCommander. |
| `lc_flush_interval` | `2000` | Milliseconds between batches of events. |

The plugin reads these cvars about once a second. A change applies to the next match.

### What a match is

| Cvar | Default | |
|---|---|---|
| `lc_min_players` | `1` | Human players needed before a match opens. Bots don't count. |
| `lc_mode` | | The mode reported. Empty takes it from `g_gametype` (`ffa`, `ctf`, `objective`, `stopwatch`, `siege`, ...). |
| `lc_settings_cvars` | per game | Cvars kept with each match as its settings. The default is the game's limits and rules, e.g. `g_gametype,timelimit,fraglimit,capturelimit,g_friendlyfire`. |

**When a match starts.** A match opens once warmup is over and enough players are in.

**When it ends.** It ends at game over: the game's `Exit:` (the limit hit, the objective won), or
the intermission. If the game has no game over the plugin can see, the match ends when the map
changes.

**Restarts and shutdowns.**

- A `map_restart` in the middle of play abandons the match. That includes a vote and an admin's
  restart.
- If the server shuts down mid-match, the match is ended with the map, as it stands.
- Warmup is never recorded: these games end warmup with a `map_restart`.

### What is recorded

| Cvar | Default | |
|---|---|---|
| `lc_record_bots` | `1` | Bots as players (`bot:<name>`). |
| `lc_record_chat` | `0` | Chat messages (`say`, `say_team`). |
| `lc_record_positions` | `1` | Player positions in kill events, and the distance. |
| `lc_record_items` | `0` | Item pickups (`Item:` log lines). |
| `lc_events_include` | | Records only these event types. Comma-separated, `*` as a wildcard. Empty records every type. |
| `lc_events_exclude` | | Never records these event types. Applied after `lc_events_include`. |

**Filtering leaves statistics alone.** The `lc_events_*` filters affect only which individual
events are recorded. With `kill` excluded, `kills`, `deaths` and the standings are still counted.

## Admin commands (server console or rcon)

| Command | |
|---|---|
| `lc_status` | The match being recorded, where it is sent, what is still waiting, and the last delivery problem. |
| `lc_flush` | Sends what is waiting now. Blocks the server for up to 5 s. |
| `lc_startmatch [mode]` | Starts a match now, ignoring `lc_min_players`. |
| `lc_endmatch [reason]` | Ends the match. No new match starts until the next map. |
| `lc_abandonmatch` | Ends the match as abandoned, so its results don't count. |

## Players

A player key names a player to LANCommander for the whole match:

- **`guid:<id>`**: the client's own GUID, where it sends one (`cl_guid` in ioquake3, ET and
  ET: Legacy, RTCW; `ja_guid` in OpenJK).
- **`bot:<name>`**: a bot.
- **`name:<name>`**: anyone else, by name without colour codes. This is the usual case at a LAN
  party.

The key is fixed when the player connects, so a rename does not split a player in two.

## Events and statistics

**Every game.**

- Events: `team_change`, `name_change`, `disconnect`, `chat` (optional), `game_end` (with the
  message and the winning team).
- Statistics: `time_played_ms`, and `score` where the game has one.

**Kills** (every game with a module):

- The `kill` event carries the `weapon` (the means of death: `railgun`, `mp40`, `saber`), `suicide`,
  `world` (falls, lava), `teamkill`, positions and distance.
- Statistics: `kills`, `deaths`, `suicides` and `teamkills`.

**Game modules.** These add events and statistics of their own:

- `flag_capture`/`captures`, `assist`/`assists`, `defend`/`defends`,
  `impressive`/`impressives`, `excellent`/`excellents`, `humiliation`/`humiliations` (Q3, JK2, JA,
  Elite Force).
- `revive`/`revives`, `dynamite_plant`/`dynamite_plants`, `dynamite_defuse`/`dynamite_defuses`,
  `repair`/`repairs`, `health_pack`/`health_packs_given`, `ammo_pack`/`ammo_packs_given` (ET).
- `item` (optional).

**Standings.**

- Team games rank the teams by score; each player takes their team's place, `Finished` for the
  winners and `Defeated` for the rest.
- ET and RTCW rank by the map's winner.
- Free-for-all ranks players by score.
- A player who left before the end is `DidNotFinish`.

**How events are read.** On a dedicated server, these games echo every line of their log
(`G_LogPrintf`) to the console. The plugin reads those lines, so `g_log` doesn't need to be set.
Scores, teams and medals come from each client's `playerState_t`, which the engine shares with every
mod. The plugin therefore works under third-party mods (OSP, ETPro, ET: Legacy, JA+, ...), as long as
they keep the stock log lines.

## Where things are kept

The Interposer works from `.interposer/` next to the plugin (`qmmaddons/lancommander/.interposer/`):

- `Logs/`: a session log per server start. Add `Logging: { Matches: true }` to
  `.interposer/Config.yml` to log every report.
- `Matches/`: reports not yet delivered. They are sent on the next start once the cvars are set,
  and removed once LANCommander has them.

QMM unloads its plugins at every map change. This plugin keeps itself loaded instead, so that the
Interposer, its delivery thread and the reports in flight carry on from map to map. The plugin's
QMM_Attach pins its image in memory.

## Building

CMake and vcpkg build the plugin. QMM2, its SDK headers (`qmm_sdks`) and the Interposer are git
submodules:

```bash
git submodule update --init --recursive
export VCPKG_ROOT=/path/to/vcpkg

# Windows (Visual Studio 2022 or later, v143 toolset); YY-Thunks is needed by the Interposer's CMake
powershell -File external/LANCommander.Interposer/build/Get-YYThunks.ps1
cmake --preset windows-x86 && cmake --build --preset windows-x86-release
cmake --preset windows-x64 && cmake --build --preset windows-x64-release
```

Linux builds run inside the Interposer's build image (Debian 12, x64 and x86 multilib):

```bash
docker build -f external/LANCommander.Interposer/docker/build.Dockerfile -t lancommander-interposer-build external/LANCommander.Interposer/docker
docker run --rm -v "$PWD:/src" -w /src lancommander-interposer-build sh -c '
  cmake --preset linux-x86 && cmake --build --preset linux-x86-release &&
  cmake --preset linux-x64 && cmake --build --preset linux-x64-release'
```

**Build options.**

- One configure builds every game's plugin for that architecture. Add `-DLC_ONLY_GAMES=Q3A;WET`
  to build fewer.
- To build against a working copy of the Interposer, add
  `-DLANCOMMANDER_INTERPOSER_DIR=/path/to/LANCommander.Interposer`.

**Tests.**

- `ctest --preset <preset>` runs the unit tests: the core's match lifecycle against a fake game,
  the log parser, the standings and the JSON payloads.
- **Checking the log formats against the games' code.** `tests/tools/fetch_sources.sh` fetches the
  released game code of every game with a module. Then `python -I tests/tools/check_formats.py`
  checks two things: that each game echoes its log to the console, and that every line the plugin
  parses is one that game writes.
- **End to end.** `tests/e2e/fake_lancommander.py` stands in for LANCommander and logs every
  request; `summarize.py` reports on them.

## Licence

GPLv3. See `LICENSE`.
