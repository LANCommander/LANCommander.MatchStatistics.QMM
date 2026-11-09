// Return to Castle Wolfenstein multiplayer.
//
// Log lines (src/game/g_main.c, g_combat.c): Kill, Item, Exit (the core). In
// the Wolfenstein modes TEAM_RED is the Axis and TEAM_BLUE the Allies.

#define LC_TEAM_A       TEAM_RED
#define LC_TEAM_A_NAME  "axis"
#define LC_TEAM_B       TEAM_BLUE
#define LC_TEAM_B_NAME  "allies"
#include "wolf.h"

namespace lc
{

namespace
{

class RtcwmpGame final : public WolfGame
{
protected:
    bool TeamGame() override { return IntCvar("g_gametype") >= GT_TEAM; }

    const std::vector<const char*>& ModeNames() const override
    {
        // GT_FFA, GT_TOURNAMENT, GT_SINGLE_PLAYER, GT_TEAM, GT_CTF, GT_WOLF,
        // GT_WOLF_STOPWATCH, GT_WOLF_CP, GT_WOLF_CPH
        static const std::vector<const char*> names =
            { "ffa", "duel", "single_player", "tdm", "ctf", "objective", "stopwatch", "checkpoint", "capture_and_hold" };
        return names;
    }
};

} // namespace

Game& TheGame()
{
    static RtcwmpGame game;
    return game;
}

} // namespace lc
