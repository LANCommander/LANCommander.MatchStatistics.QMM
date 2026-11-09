// src/core/text.cpp

#include "check.h"
#include "text.h"

int main()
{
    using namespace lc;

    CheckEq(StripColors("^1Pat^7"), "Pat", "colour codes are removed");
    CheckEq(StripColors("a^^b"), "a^b", "^^ keeps one caret");
    CheckEq(StripColors("trailing^"), "trailing^", "a caret at the end stays");
    CheckEq(StripColors("^xF00Name"), "F00Name", "only the caret and one character go");

    CheckEq(std::string(Trim("  x y \t")), "x y", "Trim");

    const auto items = SplitList(" a, ,b ,c,");
    Check(items.size() == 3 && items[0] == "a" && items[1] == "b" && items[2] == "c", "SplitList trims and drops empties");

    Check(WildcardMatch("grenade_*", "grenade_thrown"), "trailing wildcard");
    Check(WildcardMatch("*_capture", "flag_capture"), "leading wildcard");
    Check(WildcardMatch("KILL", "kill"), "case-insensitive");
    Check(!WildcardMatch("kill", "kills"), "no partial match");
    Check(WildcardMatch("*", "anything"), "lone star");
    Check(WildcardMatch("a*b*c", "aXbYbZc"), "backtracking");
    Check(MatchesAny("spawn, kill ,chat", "kill"), "MatchesAny");
    Check(!MatchesAny("", "kill"), "an empty list matches nothing");

    const std::string info = "\\name\\^1Pat\\cl_guid\\ABCDEF0123456789\\t\\1";
    CheckEq(InfoValue(info, "name"), "^1Pat", "InfoValue first key");
    CheckEq(InfoValue(info, "CL_GUID"), "ABCDEF0123456789", "InfoValue is case-insensitive on keys");
    CheckEq(InfoValue(info, "t"), "1", "InfoValue last key");
    CheckEq(InfoValue(info, "missing"), "", "InfoValue missing key");
    CheckEq(InfoValue("name\\Bob", "name"), "Bob", "InfoValue without a leading backslash");

    CheckEq(ParseInt(" 42 "), 42, "ParseInt");
    CheckEq(ParseInt("-3"), -3, "ParseInt negative");
    CheckEq(ParseInt("4x", 7), 7, "ParseInt garbage falls back");

    return g_failures;
}
