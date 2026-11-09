// What the core needs from the game it runs in. Implemented once per game in
// src/games/, which is the only code built against the game's SDK headers;
// the core itself knows nothing of any game's structures.

#pragma once

#include <string>
#include <vector>

namespace lc
{

class Stats;
struct LogLine;

constexpr int kMaxClients = 64;

// What a module can read about a connected client. Anything it cannot read
// keeps its "unknown" value.
struct ClientState
{
    int   team = -1;            // the game's team number; -1 unknown
    bool  hasScore = false;
    int   score = 0;            // the scoreboard score (frags, points)
    bool  hasPosition = false;
    float origin[3] = { 0, 0, 0 };
    int   kills  = -1;          // running totals, for games that keep them but log
    int   deaths = -1;          // no kills (MOHAA's STAT_KILLS/STAT_DEATHS); -1 unknown
    bool  spectator = false;    // watching, not playing
};

class Game
{
public:
    virtual ~Game() = default;

    // ── The engine ──────────────────────────────────────────────────────────

    // The QMM short code (GAME_STR): "Q3A", "WET", ...
    virtual const char* Code() const = 0;

    virtual std::string Cvar(const char* name) = 0;
    virtual void        RegisterCvar(const char* name, const char* defaultValue) = 0;

    // A line on the server console (and in QMM's log).
    virtual void        Print(const std::string& text) = 0;

    virtual std::string Userinfo(int client) = 0;
    virtual int         MaxClients() = 0;

    // The mod directory QMM loaded ("baseq3", "etmain", "legacy").
    virtual std::string ModDir() { return {}; }

    // ── The rules ───────────────────────────────────────────────────────────

    // The mode reported when lc_mode is empty: "ffa", "ctf", "objective", ...
    virtual std::string Mode() { return {}; }

    // The cvars kept with each match as its settings, when lc_settings_cvars is
    // left at its default ("").
    virtual std::string SettingsCvars() const { return "g_gametype,timelimit,fraglimit,capturelimit,g_friendlyfire"; }

    // False when nothing more than the generic layer is known (no struct reads).
    virtual bool ReadClient(int /*client*/, ClientState& /*state*/) { return false; }

    // The teams that are ranked against each other in the current mode, as
    // the game's team numbers. Empty for free-for-all.
    virtual std::vector<int> PlayingTeams() { return {}; }

    // The name reported for a team ("red", "axis"), "" for none.
    virtual std::string TeamName(int /*team*/) { return {}; }

    virtual bool TeamScore(int /*team*/, int& /*score*/) { return false; }

    virtual bool InWarmup() { return false; }
    virtual bool InIntermission() { return false; }

    // The game writes "Kill:" lines (G_LogPrintf). When false, the core pairs
    // the kills and deaths ClientState reports instead.
    virtual bool LogsKills() const { return false; }

    // The world's entity number, a Kill line's attacker for falls, lava and
    // trigger_hurt (ENTITYNUM_WORLD). -1 when the game has none.
    virtual int WorldEntity() const { return -1; }

    // ── Hooks for the module's own events ───────────────────────────────────

    // Every tagged console line, after the core has handled the common ones.
    virtual void OnLogLine(Stats& /*stats*/, const LogLine& /*line*/) {}

    // Once a server frame, while a match is open.
    virtual void OnFrame(Stats& /*stats*/) {}

    // A match has just started (reset any per-match state).
    virtual void OnMatchStarted(Stats& /*stats*/) {}
};

} // namespace lc
