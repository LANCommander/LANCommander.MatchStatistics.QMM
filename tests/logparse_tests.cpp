// src/core/logparse.cpp, against lines as the games print them (formats from
// their g_main.c / g_combat.c; tests/tools/check_formats.py checks those).

#include "check.h"
#include "logparse.h"

int main()
{
    using namespace lc;

    {
        const auto line = ParseLogLine("Kill: 2 3 10: Pat killed Bob by MOD_RAILGUN\n");
        Check(line && line->kind == LogKind::Kill, "Kill line");
        CheckEq(line->attacker, 2, "Kill attacker");
        CheckEq(line->victim, 3, "Kill victim");
        CheckEq(line->means, 10, "Kill means of death");
        CheckEq(line->weapon, "MOD_RAILGUN", "Kill weapon");
    }
    {
        const auto line = ParseLogLine("Kill: 1022 4 22: <world> killed Bob by MOD_TRIGGER_HURT");
        Check(line && line->kind == LogKind::Kill && line->attacker == 1022, "world kill");
    }
    {
        // A name with " by " in it: the means of death is after the last one.
        const auto line = ParseLogLine("Kill: 0 1 3: Stand by Me killed Joe by You by MOD_SHOTGUN");
        Check(line && line->weapon == "MOD_SHOTGUN", "names containing \" by \"");
    }
    {
        // ET's ids are wider than Q3's "%3i:" timestamp would suggest, but the
        // console echo has no timestamp at all.
        const auto line = ParseLogLine("Kill: 12 5 6: A killed B by MOD_MP40");
        Check(line && line->attacker == 12 && line->victim == 5, "two-digit client numbers");
    }
    {
        const auto line = ParseLogLine("Kill: x 3 10: broken");
        Check(line && line->kind == LogKind::Other, "a malformed Kill line is not a kill");
    }
    {
        const auto line = ParseLogLine("say: Pat: hello\nKill: 1 2 3: a killed b by MOD_GAUNTLET");
        Check(line && line->tag == "say" && line->kind == LogKind::Other, "only the first line is read");
    }
    {
        const auto line = ParseLogLine("Exit: Fraglimit hit.");
        Check(line && line->kind == LogKind::Exit && line->rest == "Fraglimit hit.", "Exit");
    }
    {
        const auto line = ParseLogLine("red:8  blue:5");
        Check(line && line->kind == LogKind::TeamScores && line->red == 8 && line->blue == 5, "team scores");
    }
    {
        const auto line = ParseLogLine("Item: 3 weapon_rocketlauncher");
        Check(line && line->kind == LogKind::Item && line->client == 3 && line->item == "weapon_rocketlauncher", "Item");
    }
    {
        const auto line = ParseLogLine("Warmup:");
        Check(line && line->kind == LogKind::Warmup, "Warmup");
    }
    {
        const auto line = ParseLogLine("Medic_Revive: 4 7");
        Check(line && line->tag == "Medic_Revive", "a module's tag");
        const auto ints = LeadingInts(line->rest);
        Check(ints.size() == 2 && ints[0] == 4 && ints[1] == 7, "LeadingInts");
    }

    Check(ParseLogLine("Sound: a console message").value().kind == LogKind::Other,
          "any short tag is a tag; the core and modules decide what they read");
    Check(!ParseLogLine("^1Some colored console text: here"), "a colour code is not a tag");
    Check(!ParseLogLine("no colon here"), "untagged console output");
    Check(!ParseLogLine(": empty tag"), "empty tag");

    return g_failures;
}
