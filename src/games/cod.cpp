// Call of Duty (1.5) and Call of Duty: United Offensive, multiplayer.
//
// The rules are script (maps/mp/gametypes/<g_gametype>.gsc, in the pk3s), and
// its logPrint lines are what this reads. A CoD server writes them only to
// g_log's file, never to the console: the plugin reads the mod's writes to
// that file (LC_LOG_FROM_FILE, src/plugin/main.cpp). Every stock 1.5 and UO
// gametype writes them the same way (tests/tools/check_cod_formats.py):
//
//   K;<guid>;<num>;<team>;<name>;<guid>;<num>;<team>;<name>;<weapon>;<damage>;<MOD>;<hitloc>
//                                 a death: the victim, then the attacker
//                                 (num -1 and no name: the world)
//   D;                            the same fields, for a hit
//   A;<guid>;<num>;<team>;<name>;<action>   bomb_plant, ctf_captured, ...
//   W;<team>;<guid>;<name>;...    a team won, and its players (L; the losers)
//   W;;<guid>;<name>              dm and bel: the winning player
//
// tdm, hq, dm and bel log their W line once, at the end of the map; sd, re and
// UO's ctf, dom and bas at the end of each round, before map_restart(true)
// starts the next one with the scores kept. Teams are "axis", "allies",
// "spectator", or "" in dm.
//
// Nothing is read from the game's structures: the SDK's are not those of the
// 1.x games (they are KisakCOD's, CoD 4's, with its perks and vehicles). The scoreboard score
// comes from the mod's GAME_GET_CLIENTSCORE export instead (sess.score),
// whose number was checked against 1.5's and UO's vmMain; each client's team
// from the K and D lines.

#include "../plugin/qmm_game.h"

#include <array>
#include <cctype>
#include <string_view>
#include <vector>

namespace lc
{

namespace
{

// The attacker reported for the world (the log's -1). The SDK's ENTITYNUM_WORLD
// is CoD 4's, and only defined for KisakCOD's own builds.
constexpr int kWorld = 1022;

constexpr int kAxis      = 1;
constexpr int kAllies    = 2;
constexpr int kSpectator = 3;

int TeamOf(std::string_view name)
{
    if (name == "axis")      return kAxis;
    if (name == "allies")    return kAllies;
    if (name == "spectator") return kSpectator;
    return -1;
}

// The gametypes whose W line ends the map, not a round. Anything else (a mod's
// own gametype) is taken as round-based: its match then ends with the map.
bool EndsMapOnWin(const std::string& gametype)
{
    return gametype == "dm" || gametype == "tdm" || gametype == "hq" || gametype == "bel";
}

struct Action
{
    const char* action;     // as the script logs it
    const char* event;      // as the SourceMod plugin names it
    const char* statistic;  // added to the actor; null for none
};

constexpr Action kActions[] =
{
    { "bomb_plant",    "bomb_planted",     "bomb_plants" },
    { "bomb_defuse",   "bomb_defused",     "bomb_defuses" },
    { "re_pickup",     "objective_pickup", "objective_pickups" },
    { "re_capture",    "objective_capture","objective_captures" },
    { "ctf_take",      "flag_pickup",      "flag_pickups" },
    { "ctf_pickup",    "flag_pickup",      "flag_pickups" },
    { "ctf_captured",  "flag_capture",     "flag_captures" },
    { "ctf_returned",  "flag_returned",    "flag_returns" },
    { "ctf_defended",  "flag_defended",    "flag_defenses" },
    { "ctf_assist",    "flag_assist",      "assists" },
    { "dom_captured",  "objective_capture","objective_captures" },
    { "bas_planted",   "bomb_planted",     "bomb_plants" },
    { "bas_defused",   "bomb_defused",     "bomb_defuses" },
    { "bas_destroyed", "objective_destroyed", "objectives_destroyed" },
    { "bas_attacked",  "objective_attacked",  nullptr },
    { "bas_breached",  "objective_breached",  nullptr },
    { "bas_defend",    "objective_defended",  "objective_defenses" },
    // bel_alive_tick: every few seconds alive as the hunted; not an event.
};

class CodGame final : public QmmGame
{
public:
    std::string Mode() override { return ToLower(Cvar("g_gametype")); }

    std::string SettingsCvars() const override
    {
        return "g_gametype,scr_friendlyfire,scr_killcam,scr_dm_scorelimit,scr_dm_timelimit,scr_tdm_scorelimit,"
               "scr_tdm_timelimit,scr_sd_roundlimit,scr_sd_scorelimit,scr_sd_timelimit";
    }

    bool LogsKills() const override { return true; }
    int  WorldEntity() const override { return kWorld; }

    bool ReadClient(int client, ClientState& state) override
    {
        if (client < 0 || client >= MaxClients())
            return false;

        state.team      = m_teams[client];
        state.spectator = state.team == kSpectator;
        state.hasScore  = true;
        state.score     = static_cast<int>(QMM_VMMAIN(GAME_GET_CLIENTSCORE, client));
        return true;
    }

    std::vector<int> PlayingTeams() override
    {
        const std::string mode = Mode();

        if (mode == "dm" || mode == "bel")
            return {};

        return { kAxis, kAllies };
    }

    std::string TeamName(int team) override
    {
        switch (team)
        {
        case kAxis:      return "axis";
        case kAllies:    return "allies";
        case kSpectator: return "spectator";
        default:         return {};
        }
    }

    // The rounds (or, in tdm and hq, the map) each team has won, once the
    // script has named a winner. Its own team scores are out of reach.
    bool TeamScore(int team, int& score) override
    {
        if (!m_anyWin || (team != kAxis && team != kAllies))
            return false;

        score = m_wins[team];
        return true;
    }

    bool RestartIsNextRound() override { return !EndsMapOnWin(Mode()); }

    void OnMatchStarted(Stats& stats) override
    {
        m_wins.fill(0);
        m_anyWin = false;

        if (!EndsMapOnWin(Mode()))
            stats.SetRound(1);
    }

    void OnFrame(Stats& stats) override
    {
        // A slot taken by someone else: whatever team the last one was on is not theirs.
        for (int client = 0; client < kMaxClients; ++client)
        {
            const std::string key = stats.Key(client);

            if (key != m_keys[client])
            {
                m_keys[client]  = key;
                m_teams[client] = -1;
            }
        }
    }

    bool OnRawLogLine(Stats& stats, std::string_view line) override
    {
        if (line.size() < 2 || line[1] != ';' || !std::isupper(static_cast<unsigned char>(line[0])))
            return false;

        const std::vector<std::string_view> fields = SplitFields(line, ';');

        switch (line[0])
        {
        case 'K':
        case 'D':
            OnHit(stats, fields, line[0] == 'K');
            return true;

        case 'A':
            OnAction(stats, fields);
            return true;

        case 'W':
            OnWin(stats, fields);
            return true;

        case 'J':   // the hooks have these
        case 'Q':
        case 'L':
            return true;

        default:
            return false;
        }
    }

private:
    int ClientOf(std::string_view num) const
    {
        const int client = ParseInt(num, -1);
        return client >= 0 && client < kMaxClients ? client : -1;
    }

    void SetTeam(int client, std::string_view team)
    {
        const int number = TeamOf(team);

        if (client >= 0 && number >= 0)
            m_teams[client] = number;
    }

    void OnHit(Stats& stats, const std::vector<std::string_view>& f, bool death)
    {
        // A name with a ';' in it shifts the fields: not readable.
        if (f.size() != 13)
            return;

        const int victim   = ClientOf(f[2]);
        const int attacker = ClientOf(f[6]);

        SetTeam(victim, f[3]);
        SetTeam(attacker, f[7]);

        if (!death || victim < 0)
            return;

        Payload extra;

        if (!f[11].empty())
            extra.Set("means", JsonString(ToLower(f[11])));

        if (!f[12].empty() && f[12] != "none")
            extra.Set("hit_location", JsonString(ToLower(f[12])));

        if (f[11] == "MOD_HEAD_SHOT")
            extra.Set("headshot", "true");

        const int damage = ParseInt(f[10], -1);

        if (damage >= 0)
            extra.Set("damage", JsonNumber(damage));

        stats.ReportKill(attacker >= 0 ? attacker : kWorld, victim, std::string(f[9]), &extra);
    }

    void OnAction(Stats& stats, const std::vector<std::string_view>& f)
    {
        if (f.size() != 6 || !stats.Recording())
            return;

        const int client = ClientOf(f[2]);

        if (!stats.IsPlayer(client))
            return;

        SetTeam(client, f[3]);

        for (const Action& known : kActions)
        {
            if (f[5] != known.action)
                continue;

            Payload payload;
            payload.Set("action", JsonString(f[5]));

            if (stats.SendEvent(known.event, client, -1, &payload) && known.statistic)
            {
                stats.AddStat(client, known.statistic, 1);

                const std::string team = stats.ReportedTeam(client);

                if (!team.empty())
                    stats.AddTeamStat(team, known.statistic, 1);
            }

            return;
        }
    }

    void OnWin(Stats& stats, const std::vector<std::string_view>& f)
    {
        if (f.size() < 2 || !stats.MatchOpen())
            return;

        const int team = TeamOf(f[1]);

        if (team == kAxis || team == kAllies)
        {
            ++m_wins[team];
            m_anyWin = true;
        }

        if (EndsMapOnWin(Mode()))
        {
            stats.GameOver("");
            return;
        }

        Payload payload;

        if (team == kAxis || team == kAllies)
            payload.Set("winner", JsonString(TeamName(team)));

        payload.Set("score_axis", JsonNumber(m_wins[kAxis]));
        payload.Set("score_allies", JsonNumber(m_wins[kAllies]));
        stats.SendEvent("round_end", -1, -1, &payload);
        stats.SetRound(stats.CurrentRound() + 1);
    }

    std::array<int, kMaxClients>         m_teams = MakeTeams();
    std::array<std::string, kMaxClients> m_keys{};
    std::array<int, 4>                   m_wins{};
    bool                                 m_anyWin = false;

    static std::array<int, kMaxClients> MakeTeams()
    {
        std::array<int, kMaxClients> teams;
        teams.fill(-1);
        return teams;
    }
};

} // namespace

Game& TheGame()
{
    static CodGame game;
    return game;
}

} // namespace lc
