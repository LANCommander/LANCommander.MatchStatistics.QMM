// Wolfenstein: Enemy Territory (etmain, and mods that keep its log lines:
// ET: Legacy, ETPro, N!tmod, ...).
//
// Log lines (src/game/g_main.c, g_combat.c, g_weapon.c, g_items.c), the OSP
// additions among them, each with entity numbers (= client numbers):
//
//   Medic_Revive: <medic> <revived>      g_weapon.c, the syringe
//   Dynamite_Plant: <planter>            g_weapon.c, on an objective
//   Dynamite_Diffuse: <defuser>          g_weapon.c
//   Repair: <engineer>                   g_weapon.c, an MG42
//   Health_Pack: <medic> <receiver>      g_items.c, teammates only
//   Ammo_Pack: <field ops> <receiver>    g_items.c

#define LC_TEAM_A       TEAM_AXIS
#define LC_TEAM_A_NAME  "axis"
#define LC_TEAM_B       TEAM_ALLIES
#define LC_TEAM_B_NAME  "allies"
#include "wolf.h"

namespace lc
{

namespace
{

struct ObjectiveLine
{
    const char* tag;
    const char* event;
    const char* statistic;  // added to the actor
};

constexpr ObjectiveLine kLines[] =
{
    { "Medic_Revive",     "revive",          "revives" },
    { "Dynamite_Plant",   "dynamite_plant",  "dynamite_plants" },
    { "Dynamite_Diffuse", "dynamite_defuse", "dynamite_defuses" },
    { "Repair",           "repair",          "repairs" },
    { "Health_Pack",      "health_pack",     "health_packs_given" },
    { "Ammo_Pack",        "ammo_pack",       "ammo_packs_given" },
};

class WetGame final : public WolfGame
{
public:
    void OnLogLine(Stats& stats, const LogLine& line) override
    {
        if (!stats.Recording())
            return;

        for (const ObjectiveLine& known : kLines)
        {
            if (line.tag != known.tag)
                continue;

            const std::vector<int> ids = LeadingInts(line.rest);

            if (ids.empty() || !stats.IsPlayer(ids[0]))
                return;

            const int subject = ids.size() > 1 && stats.IsPlayer(ids[1]) ? ids[1] : -1;

            Payload payload;
            stats.AddPosition(payload, "actor", ids[0]);

            if (stats.SendEvent(known.event, ids[0], subject, &payload))
                stats.AddStat(ids[0], known.statistic, 1);

            return;
        }
    }

protected:
    // Every Wolfenstein mode is Axis against Allies.
    bool TeamGame() override { return IntCvar("g_gametype") >= GT_WOLF; }

    const std::vector<const char*>& ModeNames() const override
    {
        // GT_SINGLE_PLAYER, GT_COOP, GT_WOLF, GT_WOLF_STOPWATCH,
        // GT_WOLF_CAMPAIGN, GT_WOLF_LMS
        static const std::vector<const char*> names =
            { "single_player", "coop", "objective", "stopwatch", "campaign", "last_man_standing" };
        return names;
    }
};

} // namespace

Game& TheGame()
{
    static WetGame game;
    return game;
}

} // namespace lc
