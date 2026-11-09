// src/core/stats.cpp: the match lifecycle against a fake game and a reporter
// that records every call.

#include "check.h"
#include "stats.h"

#include <map>
#include <vector>

using namespace lc;

namespace
{

struct FakeGame : Game
{
    std::map<std::string, std::string> cvars =
    {
        { "lc_enabled", "1" }, { "lc_min_players", "1" }, { "lc_record_bots", "1" }, { "lc_record_positions", "0" },
        { "lc_flush_interval", "2000" }, { "mapname", "Q3DM17" }, { "net_port", "27960" }, { "sv_hostname", "^1LAN" },
        { "g_gametype", "0" },
    };
    std::map<int, std::string> userinfo;
    std::map<int, ClientState> clients;
    std::vector<int> teams;
    std::map<int, int> teamScores;
    bool logsKills = true;
    bool warmup = false;
    std::vector<std::string> printed;

    const char* Code() const override { return "Q3A"; }
    std::string Cvar(const char* name) override { auto it = cvars.find(name); return it == cvars.end() ? "" : it->second; }
    void RegisterCvar(const char* name, const char* value) override { cvars.emplace(name, value); }
    void Print(const std::string& text) override { printed.push_back(text); }
    std::string Userinfo(int client) override { return userinfo[client]; }
    int MaxClients() override { return 16; }
    std::string Mode() override { return "ffa"; }

    bool ReadClient(int client, ClientState& state) override
    {
        auto it = clients.find(client);
        if (it == clients.end())
            return false;
        state = it->second;
        return true;
    }

    std::vector<int> PlayingTeams() override { return teams; }
    std::string TeamName(int team) override { return team == 1 ? "red" : team == 2 ? "blue" : ""; }
    bool TeamScore(int team, int& score) override { auto it = teamScores.find(team); if (it == teamScores.end()) return false; score = it->second; return true; }
    bool InWarmup() override { return warmup; }
    bool LogsKills() const override { return logsKills; }
    int  WorldEntity() const override { return 1022; }
};

struct Call
{
    std::string what, a, b, c;
    double value = 0;
    MatchResult result;
};

struct FakeReporter : Reporter
{
    std::vector<Call> calls;
    MatchHandle next = 1;
    DeliverySettings settings;

    bool Configure(const DeliverySettings& s) override { settings = s; calls.push_back({ "configure" }); return true; }
    MatchHandle Start(const MatchInfo& info) override { calls.push_back({ "start", info.externalId, info.map, info.mode }); return next++; }
    bool PlayerJoined(MatchHandle, const std::string& key, const std::string& name, const std::string& team, long long) override
    { calls.push_back({ "joined", key, name, team }); return true; }
    bool PlayerLeft(MatchHandle, const std::string& key, long long) override { calls.push_back({ "left", key }); return true; }
    bool Event(MatchHandle, const std::string& type, long long, long long, const std::string& actor, const std::string& subject,
               const std::string& payload) override
    { calls.push_back({ "event:" + type, actor, subject, payload }); return true; }
    bool SetStatistic(MatchHandle, const std::string& key, const std::string& team, const std::string& name, double value) override
    { calls.push_back({ "set:" + name, key, team, "", value }); return true; }
    bool AddStatistic(MatchHandle, const std::string& key, const std::string& team, const std::string& name, double delta) override
    { calls.push_back({ "add:" + name, key, team, "", delta }); return true; }
    bool SetResult(MatchHandle, const MatchResult& result) override { Call call{ "result" }; call.result = result; calls.push_back(call); return true; }
    bool End(MatchHandle, const std::string& reason, bool abandoned) override { calls.push_back({ "end", reason, abandoned ? "abandoned" : "" }); return true; }
    bool Flush(MatchHandle, unsigned) override { return true; }
    bool Status(MatchHandle, DeliveryStatus&) override { return false; }

    int Count(const std::string& what) const
    {
        int n = 0;
        for (const Call& call : calls)
            n += call.what == what;
        return n;
    }

    const Call* Last(const std::string& what) const
    {
        for (auto it = calls.rbegin(); it != calls.rend(); ++it)
        {
            if (it->what == what)
                return &*it;
        }
        return nullptr;
    }

    double Sum(const std::string& what, const std::string& key) const
    {
        double sum = 0;
        for (const Call& call : calls)
        {
            if (call.what == what && call.a == key)
                sum += call.value;
        }
        return sum;
    }
};

void Connect(Stats& stats, FakeGame& game, int client, const std::string& name, bool bot = false, const std::string& guid = "")
{
    game.userinfo[client] = "\\name\\" + name + (guid.empty() ? "" : "\\cl_guid\\" + guid);
    ClientState state;
    state.team = 0;
    state.hasScore = true;
    game.clients[client] = state;
    stats.OnClientConnect(client, bot);
    stats.OnClientBegin(client);
}

} // namespace

int main()
{
    // ── An FFA match from start to game over ────────────────────────────────
    {
        FakeGame game;
        FakeReporter reporter;
        Stats stats(game, reporter);

        stats.OnInit(false);
        Check(game.cvars.count("lc_server_url") == 1, "the cvars are registered");
        Check(reporter.Count("configure") == 1, "the delivery settings are applied at init");
        CheckEq(reporter.settings.reporterName, "LAN", "the reporter name defaults to the hostname, without colours");

        stats.OnFrame(1000);
        CheckEq(reporter.Count("start"), 0, "no match with nobody in (lc_min_players 1)");

        Connect(stats, game, 0, "^1Pat", false, "0123456789ABCDEF0123456789ABCDEF");
        Connect(stats, game, 1, "Sarge", true);
        Connect(stats, game, 2, "^2Bob");

        stats.OnFrame(1050);
        CheckEq(reporter.Count("start"), 1, "the match starts once a human is in");
        CheckEq(reporter.Last("start")->b, "q3dm17", "the map is reported lowercase");
        CheckEq(reporter.Count("joined"), 3, "everyone in joins it");
        CheckEq(reporter.calls[2].what == "joined" ? reporter.calls[2].a : reporter.calls[3].a,
                "guid:0123456789ABCDEF0123456789ABCDEF", "a cl_guid makes the key");

        bool botKey = false, nameKey = false;
        for (const Call& call : reporter.calls)
        {
            botKey  |= call.what == "joined" && call.a == "bot:Sarge";
            nameKey |= call.what == "joined" && call.a == "name:Bob";
        }
        Check(botKey, "a bot's key is bot:<name>");
        Check(nameKey, "no guid: name:<name>, colours stripped");

        stats.OnPrint("Kill: 0 2 7: Pat killed Bob by MOD_ROCKET\n");
        const Call* kill = reporter.Last("event:kill");
        Check(kill && kill->a.rfind("guid:", 0) == 0 && kill->b == "name:Bob", "a kill names attacker and victim");
        Check(kill && kill->c.find("\"weapon\":\"rocket\"") != std::string::npos, "the weapon is the means of death, short");
        CheckEq(static_cast<long long>(reporter.Sum("add:kills", "guid:0123456789ABCDEF0123456789ABCDEF")), 1, "kills counted");
        CheckEq(static_cast<long long>(reporter.Sum("add:deaths", "name:Bob")), 1, "deaths counted");

        stats.OnPrint("Kill: 1022 2 22: <world> killed Bob by MOD_FALLING\n");
        kill = reporter.Last("event:kill");
        Check(kill && kill->a.empty() && kill->c.find("\"suicide\":true") != std::string::npos &&
              kill->c.find("\"world\":true") != std::string::npos, "a world kill is a suicide with no actor");
        CheckEq(static_cast<long long>(reporter.Sum("add:suicides", "name:Bob")), 1, "suicides counted");

        stats.OnPrint("Kill: 40 2 0: npc killed Bob by MOD_UNKNOWN\n");
        CheckEq(reporter.Count("event:kill"), 3, "an attacker that is no player still kills");
        stats.OnPrint("Kill: 2 40 0: Bob killed npc by MOD_SABER\n");
        CheckEq(reporter.Count("event:kill"), 3, "an NPC's death is not recorded");

        game.userinfo[2] = "\\name\\Bobby";
        stats.OnClientUserinfoChanged(2);
        const Call* rename = reporter.Last("event:name_change");
        Check(rename && rename->a == "name:Bob", "a rename keeps the key it joined under");

        game.clients[0].score = 12;
        game.clients[2].score = 3;
        game.clients[1].score = 5;
        stats.OnFrame(2000);

        // The deciding frag, in the same frame as the exit, after the last poll.
        game.clients[0].score = 13;
        stats.OnPrint("Exit: Fraglimit hit.\n");
        CheckEq(reporter.Count("end"), 0, "the end waits for the next frame");
        stats.OnFrame(2050);
        CheckEq(static_cast<long long>(reporter.Last("set:score") ? 0 : -1), 0, "scores are set at the end");

        for (const Call& call : reporter.calls)
        {
            if (call.what == "result" && call.result.playerKey.rfind("guid:", 0) == 0)
                CheckEq(static_cast<long long>(call.result.score), 13, "the final score is read at the end, not a frame late");
        }
        CheckEq(reporter.Count("event:game_end"), 1, "a game_end event");
        const Call* end = reporter.Last("end");
        Check(end && end->a == "game_over" && end->b.empty(), "the match ends as game_over, not abandoned");
        CheckEq(reporter.Count("result"), 3, "a result per player");

        for (const Call& call : reporter.calls)
        {
            if (call.what == "result" && call.result.playerKey == "name:Bob")
                CheckEq(call.result.rank, 3, "ranked by score");
        }

        stats.OnFrame(3000);
        CheckEq(reporter.Count("start"), 1, "no second match on the same map after game over");

        // A bot added mid-map: no GAME_CLIENT_CONNECT or _BEGIN, only the slot.
        stats.OnShutdown(false);
        stats.OnInit(false);
        Connect(stats, game, 0, "Pat");
        stats.OnFrame(10);
        const int joinedBefore = reporter.Count("joined");
        game.userinfo[5] = "\\name\\Grunt";
        game.clients[5] = ClientState{};
        stats.OnBotAllocated(5);
        stats.OnFrame(20);
        Check(reporter.Count("joined") == joinedBefore + 1 && reporter.Last("joined")->a == "bot:Grunt",
              "a bot from G_BOT_ALLOCATE_CLIENT joins on the next frame");

        stats.OnShutdown(false);
        stats.OnInit(false);
        Connect(stats, game, 0, "Pat");
        stats.OnFrame(100);
        CheckEq(reporter.Count("start"), 3, "every map starts a new match");
        stats.OnShutdown(false);
        Check(reporter.Last("end")->a == "map_change" && reporter.Last("end")->b.empty(), "a map change ends it, not abandoned");
    }

    // ── Teams, team kills, leavers, restarts ────────────────────────────────
    {
        FakeGame game;
        FakeReporter reporter;
        Stats stats(game, reporter);
        game.teams = { 1, 2 };
        game.teamScores = { { 1, 3 }, { 2, 1 } };

        stats.OnInit(false);
        Connect(stats, game, 0, "A");
        Connect(stats, game, 1, "B");
        Connect(stats, game, 2, "C");
        game.clients[0].team = 1;
        game.clients[1].team = 1;
        game.clients[2].team = 2;
        stats.OnFrame(10);

        stats.OnPrint("Kill: 0 1 3: A killed B by MOD_SHOTGUN");
        Check(reporter.Last("event:kill")->c.find("\"teamkill\":true") != std::string::npos, "a teamkill is flagged");
        CheckEq(static_cast<long long>(reporter.Sum("add:teamkills", "name:A")), 1, "teamkills counted, not kills");
        CheckEq(static_cast<long long>(reporter.Sum("add:kills", "name:A")), 0, "a teamkill is no kill");

        game.clients[2].team = 1;
        stats.OnFrame(20);
        const Call* change = reporter.Last("event:team_change");
        Check(change && change->c == "{\"from\":\"blue\",\"to\":\"red\"}", "a team change is recorded");
        game.clients[2].team = 2;
        stats.OnFrame(25);

        stats.OnClientDisconnect(1);
        Check(reporter.Last("left") && reporter.Last("left")->a == "name:B", "a leaver leaves the match");

        stats.OnPrint("Exit: Capturelimit hit.");
        stats.OnFrame(30);
        Check(reporter.Last("event:game_end")->c.find("\"winner\":\"red\"") != std::string::npos, "the winning team");

        int teamResults = 0;
        for (const Call& call : reporter.calls)
        {
            if (call.what != "result")
                continue;
            teamResults += call.result.playerKey.empty();
            if (call.result.playerKey == "name:B")
                Check(call.result.status == ResultStatus::DidNotFinish, "the leaver did not finish");
            if (call.result.playerKey == "name:C")
                Check(call.result.status == ResultStatus::Defeated && call.result.rank == 2, "blue was defeated");
        }
        CheckEq(teamResults, 2, "a result per team");

        // A map_restart mid-match throws it away.
        stats.OnShutdown(false);
        stats.OnInit(false);
        Connect(stats, game, 0, "A");
        stats.OnFrame(10);
        stats.OnShutdown(true);
        Check(reporter.Last("end")->a == "restarted" && reporter.Last("end")->b == "abandoned", "a restart abandons the match");
    }

    // ── Warmup, filters, bots off, kills from counters ──────────────────────
    {
        FakeGame game;
        FakeReporter reporter;
        Stats stats(game, reporter);
        game.warmup = true;
        game.logsKills = false;
        game.cvars["lc_events_exclude"] = "kill";
        game.cvars["lc_record_bots"] = "0";

        stats.OnInit(false);
        Connect(stats, game, 0, "A");
        Connect(stats, game, 1, "Bot", true);
        Connect(stats, game, 2, "C");
        stats.OnFrame(10);
        CheckEq(reporter.Count("start"), 0, "no match during warmup");

        game.warmup = false;
        game.clients[0].kills = 0;
        game.clients[0].deaths = 0;
        game.clients[2].kills = 0;
        game.clients[2].deaths = 0;
        stats.OnFrame(20);
        CheckEq(reporter.Count("start"), 1, "the match starts when warmup ends");
        CheckEq(reporter.Count("joined"), 2, "bots are left out with lc_record_bots 0");

        game.clients[0].kills = 1;
        game.clients[2].deaths = 1;
        stats.OnFrame(30);
        CheckEq(reporter.Count("event:kill"), 0, "lc_events_exclude leaves the kill event out");
        CheckEq(static_cast<long long>(reporter.Sum("add:kills", "name:A")), 1, "but its kill still counts");
        CheckEq(static_cast<long long>(reporter.Sum("add:deaths", "name:C")), 1, "and its death");

        Check(stats.OnConsoleCommand({ "lc_abandonmatch" }), "lc_abandonmatch is ours");
        Check(reporter.Last("end")->b == "abandoned", "and abandons the match");
        Check(!stats.OnConsoleCommand({ "map", "q3dm1" }), "other commands are not");
        stats.OnFrame(2100);
        CheckEq(reporter.Count("start"), 1, "no automatic restart after an admin ends it");
        Check(stats.OnConsoleCommand({ "lc_startmatch", "custom" }), "lc_startmatch");
        CheckEq(reporter.Last("start")->c, "custom", "with its mode");
    }

    // ── Kills from counters (MOHAA): pairing ────────────────────────────────
    {
        FakeGame game;
        FakeReporter reporter;
        Stats stats(game, reporter);
        game.logsKills = false;

        stats.OnInit(false);
        Connect(stats, game, 0, "A");
        Connect(stats, game, 1, "B");
        Connect(stats, game, 2, "C");

        for (int client = 0; client < 3; ++client)
        {
            game.clients[client].kills = 0;
            game.clients[client].deaths = 0;
        }

        stats.OnFrame(10);

        game.clients[2].kills = 1;
        game.clients[0].deaths = 1;
        stats.OnFrame(20);
        const Call* kill = reporter.Last("event:kill");
        Check(kill && kill->a == "name:C" && kill->b == "name:A", "one killer and one victim in a frame pair up");

        game.clients[1].deaths = 1;
        stats.OnFrame(30);
        kill = reporter.Last("event:kill");
        Check(kill && kill->a.empty() && kill->b == "name:B" && kill->c.find("\"suicide\":true") != std::string::npos,
              "a death nobody scored for is a suicide");

        game.clients[0].kills = 1;
        game.clients[2].kills = 2;
        game.clients[1].deaths = 3;
        stats.OnFrame(40);
        CheckEq(reporter.Count("event:kill"), 2, "two kills and two deaths at once are not guessed at");
        CheckEq(static_cast<long long>(reporter.Sum("add:deaths", "name:B")), 3, "but they count");
    }

    std::printf("%d failure(s)\n", g_failures);
    return g_failures;
}
