// Jedi Knight: Jedi Academy multiplayer (base and OpenJK).
//
// Log lines (codemp/game/g_main.c, g_combat.c): Kill (NPCs and the world as
// attackers too; an NPC's death is left out), Item, Exit, red/blue scores.
// Captures, assists, defends and medals come from ps.persistant[], as in Q3.

#include "q3family.h"

namespace lc
{

namespace
{

class JampGame final : public Q3FamilyGame
{
protected:
    bool TeamGame() override { return IntCvar("g_gametype") >= GT_TEAM; }

    const std::vector<const char*>& ModeNames() const override
    {
        // GT_FFA, GT_HOLOCRON, GT_JEDIMASTER, GT_DUEL, GT_POWERDUEL,
        // GT_SINGLE_PLAYER, GT_TEAM, GT_SIEGE, GT_CTF, GT_CTY
        static const std::vector<const char*> names =
            { "ffa", "holocron", "jedi_master", "duel", "power_duel", "single_player", "tdm", "siege", "ctf", "cty" };
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
    static JampGame game;
    return game;
}

} // namespace lc
