#include "text.h"

#include <cctype>
#include <charconv>

namespace lc
{

std::string StripColors(std::string_view text)
{
    std::string out;
    out.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '^' && i + 1 < text.size())
        {
            if (text[i + 1] != '^')
            {
                ++i;
                continue;
            }

            out += '^';
            ++i;
            continue;
        }

        out += text[i];
    }

    return out;
}

std::string_view Trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);

    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);

    return text;
}

std::string ToLower(std::string_view text)
{
    std::string out(text);

    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    return out;
}

std::vector<std::string> SplitList(std::string_view list)
{
    std::vector<std::string> items;

    while (!list.empty())
    {
        const size_t comma = list.find(',');
        const std::string_view item = Trim(list.substr(0, comma));

        if (!item.empty())
            items.emplace_back(item);

        if (comma == std::string_view::npos)
            break;

        list.remove_prefix(comma + 1);
    }

    return items;
}

namespace
{

bool WildcardMatchAt(std::string_view pattern, std::string_view text)
{
    // Iterative glob with a single backtrack point: linear in practice.
    size_t p = 0, t = 0, star = std::string_view::npos, mark = 0;

    while (t < text.size())
    {
        if (p < pattern.size() && pattern[p] == '*')
        {
            star = p++;
            mark = t;
        }
        else if (p < pattern.size() &&
                 std::tolower(static_cast<unsigned char>(pattern[p])) ==
                 std::tolower(static_cast<unsigned char>(text[t])))
        {
            ++p;
            ++t;
        }
        else if (star != std::string_view::npos)
        {
            p = star + 1;
            t = ++mark;
        }
        else
            return false;
    }

    while (p < pattern.size() && pattern[p] == '*')
        ++p;

    return p == pattern.size();
}

} // namespace

bool WildcardMatch(std::string_view pattern, std::string_view text)
{
    return WildcardMatchAt(Trim(pattern), text);
}

bool MatchesAny(std::string_view patterns, std::string_view text)
{
    for (const std::string& pattern : SplitList(patterns))
    {
        if (WildcardMatch(pattern, text))
            return true;
    }

    return false;
}

std::string InfoValue(std::string_view info, std::string_view key)
{
    // \key\value\key\value; the leading backslash is optional.
    if (!info.empty() && info.front() == '\\')
        info.remove_prefix(1);

    while (!info.empty())
    {
        const size_t keyEnd = info.find('\\');

        if (keyEnd == std::string_view::npos)
            return {};

        const std::string_view name = info.substr(0, keyEnd);
        info.remove_prefix(keyEnd + 1);

        const size_t valueEnd = info.find('\\');
        const std::string_view value = info.substr(0, valueEnd);

        if (name.size() == key.size())
        {
            bool same = true;

            for (size_t i = 0; i < name.size() && same; ++i)
                same = std::tolower(static_cast<unsigned char>(name[i])) == std::tolower(static_cast<unsigned char>(key[i]));

            if (same)
                return std::string(value);
        }

        if (valueEnd == std::string_view::npos)
            break;

        info.remove_prefix(valueEnd + 1);
    }

    return {};
}

int ParseInt(std::string_view text, int fallback)
{
    text = Trim(text);

    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

    if (error != std::errc() || end != text.data() + text.size())
        return fallback;

    return value;
}

} // namespace lc
