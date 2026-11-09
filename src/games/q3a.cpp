// Quake III Arena and Team Arena (baseq3, missionpack and mods built on them).
//
// Log lines (code/game/g_main.c, g_combat.c, g_items.c): Kill, Item, Exit,
// red/blue scores (the core). Flag captures, assists, defends and the medals
// come from ps.persistant[], which the game keeps for the map.

#include "q3family.h"

namespace lc
{

namespace
{

class Q3aGame final : public Q3FamilyGame
{
protected:
    bool TeamGame() override { return IntCvar("g_gametype") >= GT_TEAM; }

    const std::vector<const char*>& ModeNames() const override
    {
        // gametype_t: GT_FFA, GT_TOURNAMENT, GT_SINGLE_PLAYER, GT_TEAM, GT_CTF,
        // GT_1FCTF, GT_OBELISK, GT_HARVESTER
        static const std::vector<const char*> names =
            { "ffa", "duel", "single_player", "tdm", "ctf", "one_flag_ctf", "overload", "harvester" };
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
    static Q3aGame game;
    return game;
}

} // namespace lc
