// Small string helpers shared by the core and the game modules. No SDK and
// no QMM: unit-tested on their own.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace lc
{

// A name without its colour codes: "^1Pat^7" -> "Pat". A caret followed by any
// character but another caret or the end is a code (Q_CleanStr's rule). "^^"
// keeps one caret, as the games print it.
std::string StripColors(std::string_view text);

// Leading and trailing whitespace removed.
std::string_view Trim(std::string_view text);

std::string ToLower(std::string_view text);

// Splits on commas, trimming each item and leaving out empty ones.
std::vector<std::string> SplitList(std::string_view list);

// '*' matches any run of characters, case-insensitively: "grenade_*", "*_capture".
bool WildcardMatch(std::string_view pattern, std::string_view text);

// True when text matches any of the comma-separated patterns.
bool MatchesAny(std::string_view patterns, std::string_view text);

// The value of key in an info string ("\name\Pat\team\red"), "" when absent.
std::string InfoValue(std::string_view info, std::string_view key);

// A whole number, or fallback when text is not one.
int ParseInt(std::string_view text, int fallback = 0);

} // namespace lc
