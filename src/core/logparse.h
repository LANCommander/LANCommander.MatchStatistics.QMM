// The lines id Tech 3 games write with G_LogPrintf. On a dedicated server
// G_LogPrintf echoes each one to the console through G_Printf, without its
// timestamp, so every one reaches the plugin as a G_PRINT syscall:
//
//   Kill: 2 3 10: Pat killed Bob by MOD_RAILGUN
//   Exit: Fraglimit hit.
//   red:8  blue:5
//
// The formats are the games' own (tests/tools/check_formats.py checks the
// ones parsed here against the game sources). No SDK and no QMM here.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lc
{

enum class LogKind
{
    Other,       // a tagged line nothing below covers: see tag and rest
    Kill,        // Kill: %i %i %i: %s killed %s by %s
    Exit,        // Exit: %s
    Warmup,      // Warmup:
    InitGame,    // InitGame: %s
    ShutdownGame,// ShutdownGame:
    TeamScores,  // red:%i  blue:%i
    Item,        // Item: %i %s
};

struct LogLine
{
    LogKind          kind = LogKind::Other;
    std::string      tag;     // the text before the first ':' ("Kill", "Medic_Revive")
    std::string      rest;    // after the ':' and its spaces

    // Kill: entity numbers (the world is the SDK's ENTITYNUM_WORLD, see
    // Game::WorldEntity), the means of death and its name (MOD_RAILGUN).
    int              attacker = -1;
    int              victim   = -1;
    int              means    = -1;
    std::string      weapon;

    // Item: the client and the item's classname.
    int              client = -1;
    std::string      item;

    // TeamScores.
    int              red  = 0;
    int              blue = 0;
};

// One console line, without its newline. nullopt when it carries no tag (most
// console output). Only the first line of text is read: one G_LogPrintf call
// writes one line, so anything after a newline is something else (a chat
// message cannot smuggle a "Kill:" line in that way).
std::optional<LogLine> ParseLogLine(std::string_view text);

// The whole numbers at the start of text, separated by spaces: "3 7" -> {3, 7}.
// For lines like "Medic_Revive: %d %d". Stops at the first non-number.
std::vector<int> LeadingInts(std::string_view text);

} // namespace lc
