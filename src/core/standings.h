// The final standings, worked out the same way as the SourceMod plugin's
// (scripting/lancommander_stats/match.sp there):
//
// - Team games (two or more teams that had players): teams are ranked by
//   score, and every player takes their team's place, Finished for the
//   winners and Defeated for the rest.
// - Free-for-all: players are ranked by score.
// - A player who left before the end did not finish.
//
// Pure: no game, no Interposer. Unit-tested in tests/standings_tests.cpp.

#pragma once

#include "reporter.h"

#include <string>
#include <vector>

namespace lc
{

struct StandingPlayer
{
    std::string key;
    std::string team;       // the reported team name; "" for none
    int         score = 0;
    bool        finished = true;
};

struct StandingTeam
{
    std::string name;
    int         score = 0;
};

struct Standings
{
    std::vector<MatchResult> teams;
    std::vector<MatchResult> players;
};

// teams: the playing teams that had players during the match. With fewer than
// two the match is ranked as free-for-all. In a team game, players on no
// ranked team (spectators) take no place.
Standings ComputeStandings(const std::vector<StandingPlayer>& players, const std::vector<StandingTeam>& teams);

} // namespace lc
