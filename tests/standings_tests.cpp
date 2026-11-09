// src/core/standings.cpp

#include "check.h"
#include "standings.h"

using namespace lc;

static const MatchResult* Find(const std::vector<MatchResult>& results, const std::string& key)
{
    for (const MatchResult& result : results)
    {
        if (result.playerKey == key || (result.playerKey.empty() && result.team == key))
            return &result;
    }

    return nullptr;
}

int main()
{
    // Free-for-all: ranked by score, ties share a rank, leavers did not finish.
    {
        const Standings s = ComputeStandings(
            { { "name:a", "", 10, true }, { "name:b", "", 15, true }, { "name:c", "", 10, true }, { "name:d", "", 20, false } },
            {});

        Check(s.teams.empty(), "FFA has no team results");
        const MatchResult* a = Find(s.players, "name:a");
        const MatchResult* b = Find(s.players, "name:b");
        const MatchResult* c = Find(s.players, "name:c");
        const MatchResult* d = Find(s.players, "name:d");
        Check(a && b && c && d, "FFA has every player");
        CheckEq(d->rank, 1, "the leaver with the top score still ranks first");
        Check(d->status == ResultStatus::DidNotFinish, "a leaver did not finish");
        CheckEq(b->rank, 2, "second");
        CheckEq(a->rank, 3, "tie shares a rank");
        CheckEq(c->rank, 3, "tie shares a rank (2)");
        Check(a->status == ResultStatus::Finished, "everyone else finished");
    }

    // Team game: players take their team's place.
    {
        const Standings s = ComputeStandings(
            { { "name:r1", "red", 5, true }, { "name:b1", "blue", 9, true }, { "name:r2", "red", 1, false },
              { "name:spec", "spectator", 0, true } },
            { { "red", 3 }, { "blue", 1 } });

        const MatchResult* red  = Find(s.teams, "red");
        const MatchResult* blue = Find(s.teams, "blue");
        Check(red && blue, "both teams ranked");
        CheckEq(red->rank, 1, "red wins");
        Check(red->status == ResultStatus::Finished && blue->status == ResultStatus::Defeated, "team statuses");

        const MatchResult* b1 = Find(s.players, "name:b1");
        const MatchResult* r2 = Find(s.players, "name:r2");
        CheckEq(b1->rank, 2, "a player takes the team's rank, not their own");
        Check(b1->status == ResultStatus::Defeated, "losing team's players are defeated");
        Check(r2->status == ResultStatus::DidNotFinish, "a winning team's leaver did not finish");
        Check(Find(s.players, "name:spec") == nullptr, "spectators take no place");
    }

    // One team with players is not a team game.
    {
        const Standings s = ComputeStandings({ { "name:a", "red", 3, true }, { "name:b", "red", 4, true } }, { { "red", 7 } });
        Check(s.teams.empty() && s.players.size() == 2, "one team ranks as free-for-all");
    }

    return g_failures;
}
