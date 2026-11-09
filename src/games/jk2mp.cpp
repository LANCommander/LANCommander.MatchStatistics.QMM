// Jedi Knight II: Jedi Outcast multiplayer.
//
// Log lines (CODE-mp/game/g_main.c, g_combat.c): Kill (NPCs and the world as
// attackers too), Item, Exit, red/blue scores. Captures, assists, defends and
// medals come from ps.persistant[], as in Q3.

#include "q3family.h"

namespace lc
{

namespace
{

class Jk2mpGame final : public Q3FamilyGame
{
protected:
    bool TeamGame() override { return IntCvar("g_gametype") >= GT_TEAM; }

    const std::vector<const char*>& ModeNames() const override
    {
        // GT_FFA, GT_HOLOCRON, GT_JEDIMASTER, GT_TOURNAMENT, GT_SINGLE_PLAYER,
        // GT_TEAM, GT_SAGA, GT_CTF, GT_CTY
        static const std::vector<const char*> names =
            { "ffa", "holocron", "jedi_master", "duel", "single_player", "tdm", "saga", "ctf", "cty" };
        return names;
    }

    const std::vector<Award>& Awards() const override
    {
        static const std::vector<Award> awards =
        {
            { PERS_CAPTURES,            "captures",     "flag_capture" },
            { PERS_ASSIST_COUNT,        "assists",      "assist" },
            { PERS_DEFEND_COUNT,        "defends",      "defend" },
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
    static Jk2mpGame game;
    return game;
}

} // namespace lc
