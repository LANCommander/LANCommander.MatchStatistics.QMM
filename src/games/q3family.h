// What the Quake III-derived games share, compiled with each one's SDK (Q3A,
// RTCW MP, ET, JK2 MP, JA MP, Elite Force). Only the playerState_t is read
// from a client: it is the engine's, at the start of gclient_t in every mod,
// so this works under third-party mods too, whose private fields after it
// (sess, pers) move about.
//
// Each game's module (src/games/<game>.cpp) derives from Q3FamilyGame for its
// mode names, teams and awards.

#pragma once

// The two playing teams, as the game numbers and names them. Q3's red and blue
// unless the module says otherwise before including this (ET: axis, allies).
#ifndef LC_TEAM_A
#define LC_TEAM_A       TEAM_RED
#define LC_TEAM_A_NAME  "red"
#define LC_TEAM_B       TEAM_BLUE
#define LC_TEAM_B_NAME  "blue"
#endif

#include <array>
#include <cstdlib>
#include <string>
#include <vector>

#include "../plugin/qmm_game.h"

namespace lc
{

// A counter in ps.persistant[] the game keeps per player for the map, and
// what each step of it is reported as.
struct Award
{
    int         pers;       // the PERS_* index
    const char* statistic;  // added to per step ("captures")
    const char* event;      // recorded per step ("flag_capture"); null for none
};

class Q3FamilyGame : public QmmGame
{
public:
    bool LogsKills() const override { return true; }
    int  WorldEntity() const override { return ENTITYNUM_WORLD; }

    bool ReadClient(int client, ClientState& state) override
    {
        gclient_t* c = Client(client);

        if (!c)
            return false;

        const playerState_t& ps = c->ps;

        state.team        = ps.persistant[PERS_TEAM];
        state.spectator   = state.team == TEAM_SPECTATOR;
        state.hasScore    = true;
        state.score       = ps.persistant[PERS_SCORE];
        state.hasPosition = true;
        state.origin[0]   = ps.origin[0];
        state.origin[1]   = ps.origin[1];
        state.origin[2]   = ps.origin[2];
        return true;
    }

    std::string Mode() override
    {
        const std::vector<const char*>& names = ModeNames();
        const int gametype = IntCvar("g_gametype");
        return gametype >= 0 && gametype < static_cast<int>(names.size()) ? names[gametype] : std::to_string(gametype);
    }

    std::vector<int> PlayingTeams() override
    {
        if (!TeamGame())
            return {};

        return { LC_TEAM_A, LC_TEAM_B };
    }

    std::string TeamName(int team) override
    {
        switch (team)
        {
        case LC_TEAM_A:      return LC_TEAM_A_NAME;
        case LC_TEAM_B:      return LC_TEAM_B_NAME;
        case TEAM_SPECTATOR: return "spectator";
        default:             return {};
        }
    }

#ifndef LC_NO_CS_SCORES
    // CS_SCORES1/2: the team scores in team modes (the top two players' in the
    // others, which PlayingTeams() leaves out).
    bool TeamScore(int team, int& score) override
    {
        if (team == LC_TEAM_A)
            score = std::atoi(ConfigString(CS_SCORES1).c_str());
        else if (team == LC_TEAM_B)
            score = std::atoi(ConfigString(CS_SCORES2).c_str());
        else
            return false;

        return true;
    }
#endif

    // CS_WARMUP holds when warmup ends ("-1" while waiting for players), and
    // is emptied when play starts.
    bool InWarmup() override { return !ConfigString(CS_WARMUP).empty(); }

    // Set to "1" by LogExit.
    bool InIntermission() override { return std::atoi(ConfigString(CS_INTERMISSION).c_str()) != 0; }

    void OnMatchStarted(Stats& stats) override
    {
        for (auto& counters : m_awardCounts)
            counters.fill(-1);

        // What the players have already earned is not this match's.
        CountAwards(stats, false);
    }

    void OnFrame(Stats& stats) override
    {
        CountAwards(stats, true);
    }

protected:
    // Mode names by g_gametype.
    virtual const std::vector<const char*>& ModeNames() const = 0;

    // Whether g_gametype is a team mode.
    virtual bool TeamGame() = 0;

    virtual const std::vector<Award>& Awards() const
    {
        static const std::vector<Award> none;
        return none;
    }

private:
    static constexpr int kMaxAwards = 16;

    void CountAwards(Stats& stats, bool report)
    {
        const std::vector<Award>& awards = Awards();

        for (int client = 0; client < kMaxClients; ++client)
        {
            const std::string key = stats.Key(client);

            if (key != m_awardKeys[client])
            {
                m_awardKeys[client] = key;
                m_awardCounts[client].fill(-1);
            }

            gclient_t* c = key.empty() ? nullptr : Client(client);

            if (!c)
                continue;

            for (size_t i = 0; i < awards.size() && i < kMaxAwards; ++i)
            {
                const int count = c->ps.persistant[awards[i].pers];
                int& last = m_awardCounts[client][i];

                if (report && last >= 0 && count > last)
                {
                    for (int step = last; step < count; ++step)
                    {
                        if (awards[i].event)
                            stats.SendEvent(awards[i].event, client, -1);
                    }

                    stats.AddStat(client, awards[i].statistic, count - last);
                }

                // Lower: the game reset it (a team change); start from there.
                last = count;
            }
        }
    }

    std::array<std::string, kMaxClients>                    m_awardKeys{};
    std::array<std::array<int, kMaxAwards>, kMaxClients>    m_awardCounts{};
};

} // namespace lc
