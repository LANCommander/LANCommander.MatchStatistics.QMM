// Medal of Honor: Allied Assault.
//
// MOHAA logs no kills: its death messages go to each client as localized text
// with names, not numbers (openmohaa's G_PrintDeathMessage). What it keeps in
// the engine's playerState_t is enough, though: ps.stats[STAT_KILLS],
// [STAT_DEATHS] and [STAT_TEAM] (fgame/bg_public.h). The core pairs the kills
// and deaths that go up in the same frame into kill events (Stats::Poll).
// Team scores and round wins live in the game's dmManager, which is C++ the
// plugin cannot read safely, so team games are ranked by player kills.

#include <iterator>
#include <string>
#include <vector>

#include "../plugin/qmm_game.h"

namespace lc
{

namespace
{

class MohaaGame final : public QmmGame
{
public:
    bool ReadClient(int client, ClientState& state) override
    {
        gclient_t* c = Client(client);

        if (!c)
            return false;

        const playerState_t& ps = c->ps;

        state.team        = ps.stats[STAT_TEAM];
        state.spectator   = state.team == TEAM_SPECTATOR;
        state.kills       = ps.stats[STAT_KILLS];
        state.deaths      = ps.stats[STAT_DEATHS];
        state.hasScore    = true;
        state.score       = state.kills;
        state.hasPosition = true;
        state.origin[0]   = ps.origin[0];
        state.origin[1]   = ps.origin[1];
        state.origin[2]   = ps.origin[2];
        return true;
    }

    std::string Mode() override
    {
        // GT_SINGLE_PLAYER, GT_FFA, GT_TEAM, GT_TEAM_ROUNDS, GT_OBJECTIVE,
        // GT_TOW, GT_LIBERATION
        static const char* const names[] =
            { "single_player", "ffa", "tdm", "round_based", "objective", "tug_of_war", "liberation" };

        const int gametype = IntCvar("g_gametype");
        return gametype >= 0 && gametype < static_cast<int>(std::size(names)) ? names[gametype] : std::to_string(gametype);
    }

    std::string SettingsCvars() const override
    {
        return "g_gametype,timelimit,fraglimit,roundlimit,g_teamdamage";
    }

    std::vector<int> PlayingTeams() override
    {
        if (IntCvar("g_gametype") < GT_TEAM)
            return {};

        return { TEAM_ALLIES, TEAM_AXIS };
    }

    std::string TeamName(int team) override
    {
        switch (team)
        {
        case TEAM_ALLIES:    return "allies";
        case TEAM_AXIS:      return "axis";
        case TEAM_SPECTATOR: return "spectator";
        default:             return {};
        }
    }
};

} // namespace

Game& TheGame()
{
    static MohaaGame game;
    return game;
}

} // namespace lc
