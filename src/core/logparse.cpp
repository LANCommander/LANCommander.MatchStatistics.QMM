#include "logparse.h"

#include "text.h"

#include <cctype>

namespace lc
{

namespace
{

constexpr size_t kMaxTagLength = 32;

bool IsTagChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ' ' || c == '-';
}

// Reads one integer at text[pos...], skipping spaces before it.
bool ReadInt(std::string_view text, size_t& pos, int& value)
{
    while (pos < text.size() && text[pos] == ' ')
        ++pos;

    const size_t start = pos;

    if (pos < text.size() && text[pos] == '-')
        ++pos;

    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])))
        ++pos;

    if (pos == start || (pos == start + 1 && text[start] == '-'))
        return false;

    value = ParseInt(text.substr(start, pos - start), 0);
    return true;
}

bool ParseKill(LogLine& line)
{
    // "%i %i %i: %s killed %s by %s"
    const std::string_view rest = line.rest;
    size_t pos = 0;

    if (!ReadInt(rest, pos, line.attacker) || !ReadInt(rest, pos, line.victim) || !ReadInt(rest, pos, line.means))
        return false;

    if (pos >= rest.size() || rest[pos] != ':')
        return false;

    // Names may contain " by ", the means of death has no spaces: the last one.
    const size_t by = rest.rfind(" by ");

    if (by != std::string_view::npos && by > pos)
        line.weapon = std::string(Trim(rest.substr(by + 4)));

    return true;
}

bool ParseTeamScores(std::string_view text, LogLine& line)
{
    // "red:%i  blue:%i"
    size_t pos = 4; // past "red:"

    if (!ReadInt(text, pos, line.red))
        return false;

    const size_t blue = text.find("blue:", pos);

    if (blue == std::string_view::npos)
        return false;

    pos = blue + 5;
    return ReadInt(text, pos, line.blue);
}

} // namespace

std::optional<LogLine> ParseLogLine(std::string_view text)
{
    const size_t newline = text.find_first_of("\r\n");

    if (newline != std::string_view::npos)
        text = text.substr(0, newline);

    const size_t colon = text.find(':');

    if (colon == 0 || colon == std::string_view::npos || colon > kMaxTagLength)
        return std::nullopt;

    for (size_t i = 0; i < colon; ++i)
    {
        if (!IsTagChar(text[i]))
            return std::nullopt;
    }

    LogLine line;
    line.tag  = std::string(text.substr(0, colon));
    line.rest = std::string(Trim(text.substr(colon + 1)));

    if (line.tag == "Kill")
    {
        if (ParseKill(line))
            line.kind = LogKind::Kill;
    }
    else if (line.tag == "Exit")
        line.kind = LogKind::Exit;
    else if (line.tag == "Warmup")
        line.kind = LogKind::Warmup;
    else if (line.tag == "InitGame")
        line.kind = LogKind::InitGame;
    else if (line.tag == "ShutdownGame")
        line.kind = LogKind::ShutdownGame;
    else if (line.tag == "red")
    {
        if (ParseTeamScores(text, line))
            line.kind = LogKind::TeamScores;
    }
    else if (line.tag == "Item")
    {
        size_t pos = 0;

        if (ReadInt(line.rest, pos, line.client))
        {
            line.item = std::string(Trim(std::string_view(line.rest).substr(pos)));
            line.kind = LogKind::Item;
        }
    }

    return line;
}

std::vector<int> LeadingInts(std::string_view text)
{
    std::vector<int> values;
    size_t pos = 0;
    int value;

    while (ReadInt(text, pos, value))
    {
        values.push_back(value);

        if (pos < text.size() && text[pos] != ' ')
            break;
    }

    return values;
}

} // namespace lc
