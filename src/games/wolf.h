// What Return to Castle Wolfenstein multiplayer and Wolfenstein: Enemy
// Territory share beyond Q3: the "gamestate" cvar (gamestate_t, q_shared.h)
// for warmup and intermission, and the map's winner in CS_MULTI_MAPWINNER
// ("\winner\0" axis, 1 allies, -1 none yet) rather than team scores.

#pragma once

#define LC_NO_CS_SCORES
#include "q3family.h"

namespace lc
{

class WolfGame : public Q3FamilyGame
{
public:
    // GS_WARMUP_COUNTDOWN, GS_WARMUP and GS_WAITING_FOR_PLAYERS. GS_INITIALIZE
    // (-1) is the cvar's default before the game sets it, and counts as play.
    bool InWarmup() override
    {
        const int state = IntCvar("gamestate");
        return state == GS_WARMUP_COUNTDOWN || state == GS_WARMUP || state == GS_WAITING_FOR_PLAYERS;
    }

    bool InIntermission() override
    {
        return IntCvar("gamestate") == GS_INTERMISSION || Q3FamilyGame::InIntermission();
    }

    // 1 for the map's winner, 0 for the other: the score the standings rank by.
    // Only known once the map is decided.
    bool TeamScore(int team, int& score) override
    {
        const std::string winner = InfoValue(ConfigString(CS_MULTI_MAPWINNER), "winner");

        if (winner.empty() || (team != LC_TEAM_A && team != LC_TEAM_B))
            return false;

        const int winning = ParseInt(winner, -1);

        if (winning != 0 && winning != 1)
            return false;

        score = (winning == 0) == (team == LC_TEAM_A) ? 1 : 0;
        return true;
    }
};

} // namespace lc
