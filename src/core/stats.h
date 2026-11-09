// The plugin's core: settings, players, the match and its events. Game-neutral;
// src/main.cpp feeds it QMM's hooks and src/games/ the game's own knowledge
// (through Game). One instance for the process: it outlives QMM's unload of
// the plugin at each map change (the image is pinned, see interposer.cpp).

#pragma once

#include "game.h"
#include "logparse.h"
#include "payload.h"
#include "reporter.h"

#include <array>
#include <chrono>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace lc
{

struct Settings
{
    bool             enabled = true;
    DeliverySettings delivery;
    int              minPlayers = 1;
    std::string      mode;
    std::string      settingsCvars;
    bool             recordBots      = true;
    bool             recordChat      = false;
    bool             recordPositions = true;
    bool             recordItems     = false;
    std::string      eventsInclude;
    std::string      eventsExclude;
};

class Stats
{
public:
    Stats(Game& game, Reporter& reporter);

    Game& GetGame() { return m_game; }

    // ── From QMM's hooks (src/main.cpp) ─────────────────────────────────────

    // After the mod's GAME_INIT. restart: a map_restart, not a new map.
    void OnInit(bool restart);

    // Before the mod's GAME_SHUTDOWN.
    void OnShutdown(bool restart);

    // After the mod's GAME_RUN_FRAME. levelTimeMs: the game's clock, negative
    // for games whose RunFrame does not pass one.
    void OnFrame(long long levelTimeMs);

    void OnClientConnect(int client, bool bot);

    // The engine gave a bot a slot (G_BOT_ALLOCATE_CLIENT). The mod connects
    // bots itself, never through GAME_CLIENT_CONNECT: the bot is taken as
    // connected on the next frame, once the mod has set its userinfo.
    void OnBotAllocated(int client);
    void OnClientUserinfoChanged(int client);
    void OnClientBegin(int client);
    void OnClientDisconnect(int client);
    void OnChat(int client, const std::string& text, bool teamOnly);

    // A server console command (rcon). True when it was one of ours.
    bool OnConsoleCommand(const std::vector<std::string>& argv);

    // Every G_PRINT: the log lines among them.
    void OnPrint(std::string_view text);

    // ── For the game modules ────────────────────────────────────────────────

    const Settings& GetSettings() const { return m_settings; }

    bool        Recording() const;
    bool        MatchOpen() const { return m_match != 0; }
    long long   GameTimeMs() const;
    void        SetRound(int round) { m_round = round; }
    int         CurrentRound() const { return m_round; }

    bool        IsPlayer(int client) const;
    std::string Key(int client) const;          // "" when not recorded (a bot with lc_record_bots 0)
    std::string ReportedTeam(int client) const; // the client's team name, "" for none

    // Records an event. actor/subject are client numbers, -1 for none. Adds
    // the round and warmup flags. Returns false when nothing was recorded
    // because no match is recording, or because it only involves unrecorded
    // players. The lc_events_* filter leaves an event out but still returns
    // true, so its statistics count.
    bool SendEvent(const std::string& type, int actor, int subject, Payload* payload = nullptr);

    void AddStat(int client, const std::string& name, double delta);
    void AddTeamStat(const std::string& team, const std::string& name, double delta);

    // Adds "<prefix>_position" when positions are recorded and readable.
    void AddPosition(Payload& payload, const char* prefix, int client);

    // The match is over (the scoreboard is final): it ends on the next frame,
    // after whatever the game logs with its exit.
    void GameOver(const std::string& message);

    // ── Admin ───────────────────────────────────────────────────────────────

    bool StartMatch(const std::string& mode, bool manual);
    bool EndMatch(const std::string& reason, bool abandoned);

private:
    struct Player
    {
        bool        connected = false;
        bool        bot       = false;
        std::string key;
        std::string name;
        int         team = -1;
        bool        hasScore = false;
        int         score = 0;
        int         kills  = -1;
        int         deaths = -1;
        bool        inMatch = false;
        long long   joinedAtMs = 0;
    };

    struct Leaver
    {
        int score = 0;
        int team  = -1;
    };

    void ReadSettings(bool force);
    void ApplyDelivery();
    void TryStartMatch();
    void Poll();
    void PollClient(int client);
    void PairKillsAndDeaths(const std::vector<std::pair<int, int>>& kills, const std::vector<std::pair<int, int>>& deaths);
    void HandleKill(int attacker, int victim, const std::string& weapon);
    void JoinMatch(int client);
    void LeaveMatch(int client);
    void AddTimePlayed(int client);
    void SetResults();
    void ResetPlayers();
    std::string BuildKey(int client, const std::string& userinfo) const;
    std::string TeamNameOf(int team) const;
    std::string ExternalId() const;
    std::string SettingsJson() const;
    std::string MetadataJson() const;
    bool EventFiltered(const std::string& type) const;
    void PrintStatus();
    long long SteadyMs() const;
    long long NowMs() const;

    Game&     m_game;
    Reporter& m_reporter;

    Settings  m_settings;
    bool      m_deliveryApplied = false;
    long long m_lastSettingsRead = -1;

    std::array<Player, kMaxClients> m_players{};
    std::array<bool, kMaxClients>   m_botPending{};
    std::map<std::string, Leaver>   m_leavers;
    std::vector<std::string>        m_teamsWithPlayers; // team names, during the match

    MatchHandle m_match = 0;
    std::string m_externalId;
    long long   m_matchStartMs = 0;
    bool        m_doneThisMap = false;      // ended; no automatic start until the next map
    bool        m_gameOverPending = false;
    std::string m_gameOverMessage;
    bool        m_warmup = false;
    bool        m_intermission = false;
    int         m_round = 0;

    long long   m_levelTimeMs = -1;
    std::chrono::steady_clock::time_point m_epoch = std::chrono::steady_clock::now();
};

} // namespace lc
