// Star Trek: Voyager Elite Force, Holomatch (multiplayer).
//
// Log lines (code/game/g_main.c, g_combat.c, checked against lilium-voyager's
// game code): Kill, Item, Exit, red/blue scores. Elite Force keeps no capture,
// assist or defend counters in ps.persistant[]; its medals are there.

#include "q3family.h"

namespace lc
{

namespace
{

class StvoyhmGame final : public Q3FamilyGame
{
protected:
    bool TeamGame() override { return IntCvar("g_gametype") >= GT_TEAM; }

    const std::vector<const char*>& ModeNames() const override
    {
        // GT_FFA, GT_TOURNAMENT, GT_SINGLE_PLAYER, GT_TEAM, GT_CTF
        static const std::vector<const char*> names = { "ffa", "duel", "single_player", "tdm", "ctf" };
        return names;
    }

    const std::vector<Award>& Awards() const override
    {
        static const std::vector<Award> awards =
        {
            { PERS_IMPRESSIVE_COUNT,    "impressives",  "impressive" },
            { PERS_EXCELLENT_COUNT,     "excellents",   "excellent" },
            { PERS_GAUNTLET_FRAG_COUNT, "humiliations", "humiliation" },
        };
        return awards;
    }
};

} // namespace

Game& TheGame()
{
    static StvoyhmGame game;
    return game;
}

} // namespace lc
