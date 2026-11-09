#include "standings.h"

namespace lc
{

Standings ComputeStandings(const std::vector<StandingPlayer>& players, const std::vector<StandingTeam>& teams)
{
    Standings standings;

    if (teams.size() >= 2)
    {
        for (const StandingTeam& team : teams)
        {
            int rank = 1;

            for (const StandingTeam& other : teams)
            {
                if (other.score > team.score)
                    ++rank;
            }

            MatchResult result;
            result.team     = team.name;
            result.rank     = rank;
            result.status   = rank == 1 ? ResultStatus::Finished : ResultStatus::Defeated;
            result.hasScore = true;
            result.score    = team.score;
            standings.teams.push_back(result);
        }

        for (const StandingPlayer& player : players)
        {
            const MatchResult* teamResult = nullptr;

            for (const MatchResult& team : standings.teams)
            {
                if (!player.team.empty() && team.team == player.team)
                    teamResult = &team;
            }

            if (!teamResult)
                continue;

            MatchResult result;
            result.playerKey = player.key;
            result.team      = player.team;
            result.rank      = teamResult->rank;
            result.status    = player.finished ? teamResult->status : ResultStatus::DidNotFinish;
            result.hasScore  = true;
            result.score     = player.score;
            standings.players.push_back(result);
        }

        return standings;
    }

    for (const StandingPlayer& player : players)
    {
        int rank = 1;

        for (const StandingPlayer& other : players)
        {
            if (other.score > player.score)
                ++rank;
        }

        MatchResult result;
        result.playerKey = player.key;
        result.rank      = rank;
        result.status    = player.finished ? ResultStatus::Finished : ResultStatus::DidNotFinish;
        result.hasScore  = true;
        result.score     = player.score;
        standings.players.push_back(result);
    }

    return standings;
}

} // namespace lc
