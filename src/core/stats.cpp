#include "stats.h"

#include "standings.h"
#include "text.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>

#ifndef LC_PLUGIN_VERSION
#define LC_PLUGIN_VERSION "0.0.0"
#endif

namespace lc
{

namespace
{

constexpr long long kSettingsPollMs = 1000;

struct CvarDefault
{
    const char* name;
    const char* value;
};

// Every lc_* cvar and its default. README.md documents them.
constexpr CvarDefault kCvars[] =
{
    { "lc_enabled",          "1" },
    { "lc_server_url",       "" },
    { "lc_token",            "" },
    { "lc_game_id",          "" },
    { "lc_server_id",        "" },
    { "lc_reporter_name",    "" },
    { "lc_flush_interval",   "2000" },
    { "lc_min_players",      "1" },
    { "lc_mode",             "" },
    { "lc_settings_cvars",   "" },
    { "lc_record_bots",      "1" },
    { "lc_record_chat",      "0" },
    { "lc_record_positions", "1" },
    { "lc_record_items",     "0" },
    { "lc_events_include",   "" },
    { "lc_events_exclude",   "" },
};

// "MOD_RAILGUN" -> "railgun", as weapons are named in the SourceMod reports.
std::string WeaponName(const std::string& meansOfDeath)
{
    std::string_view name = meansOfDeath;

    if (name.size() > 4 && (name.substr(0, 4) == "MOD_" || name.substr(0, 4) == "mod_"))
        name.remove_prefix(4);

    return ToLower(name);
}

} // namespace

Stats::Stats(Game& game, Reporter& reporter) : m_game(game), m_reporter(reporter)
{
}

// ── Settings ─────────────────────────────────────────────────────────────────

void Stats::ReadSettings(bool force)
{
    const long long now = SteadyMs();

    if (!force && m_lastSettingsRead >= 0 && now - m_lastSettingsRead < kSettingsPollMs)
        return;

    m_lastSettingsRead = now;

    const auto boolean = [&](const char* name) { return ParseInt(m_game.Cvar(name), 0) != 0; };

    Settings s;
    s.enabled                  = boolean("lc_enabled");
    s.delivery.enabled         = s.enabled;
    s.delivery.serverUrl       = std::string(Trim(m_game.Cvar("lc_server_url")));
    s.delivery.token           = std::string(Trim(m_game.Cvar("lc_token")));
    s.delivery.gameId          = std::string(Trim(m_game.Cvar("lc_game_id")));
    s.delivery.serverId        = std::string(Trim(m_game.Cvar("lc_server_id")));
    s.delivery.reporterName    = std::string(Trim(m_game.Cvar("lc_reporter_name")));
    s.delivery.flushIntervalMs = std::clamp(ParseInt(m_game.Cvar("lc_flush_interval"), 2000), 100, 600000);

    if (s.delivery.reporterName.empty())
        s.delivery.reporterName = StripColors(m_game.Cvar("sv_hostname"));

    s.minPlayers      = std::max(0, ParseInt(m_game.Cvar("lc_min_players"), 1));
    s.mode            = std::string(Trim(m_game.Cvar("lc_mode")));
    s.settingsCvars   = std::string(Trim(m_game.Cvar("lc_settings_cvars")));
    s.recordBots      = boolean("lc_record_bots");
    s.recordChat      = boolean("lc_record_chat");
    s.recordPositions = boolean("lc_record_positions");
    s.recordItems     = boolean("lc_record_items");
    s.eventsInclude   = m_game.Cvar("lc_events_include");
    s.eventsExclude   = m_game.Cvar("lc_events_exclude");

    const bool deliveryChanged = !(s.delivery == m_settings.delivery);
    m_settings = std::move(s);

    if (deliveryChanged || !m_deliveryApplied)
        ApplyDelivery();
}

void Stats::ApplyDelivery()
{
    m_deliveryApplied = true;

    if (!m_reporter.Configure(m_settings.delivery))
        m_game.Print("[LANCommander] The match statistics settings were refused (is lc_game_id a GUID, lc_server_url http:// or https://?)");
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void Stats::OnInit(bool restart)
{
    for (const CvarDefault& cvar : kCvars)
        m_game.RegisterCvar(cvar.name, cvar.value);

    ReadSettings(true);

    ResetPlayers();

    // A new map, or the same one restarted: a new game either way (Q3 restarts
    // the map after the intermission when there is no nextmap).
    (void)restart;
    m_doneThisMap = false;

    m_levelTimeMs     = -1;
    m_warmup          = false;
    m_intermission    = false;
    m_gameOverPending = false;
    m_gameOverMessage.clear();
    m_round = 0;
}

void Stats::OnShutdown(bool restart)
{
    if (m_gameOverPending)
        OnFrame(m_levelTimeMs);

    // A map_restart in the middle of play throws the match away; a map change
    // (or the server quitting) ends it as it stands. Without a game over of its
    // own, the map's end is the match's.
    if (m_match)
        EndMatch(restart ? "restarted" : "map_change", restart);
}

void Stats::OnFrame(long long levelTimeMs)
{
    m_levelTimeMs = levelTimeMs;

    ReadSettings(false);

    if (m_gameOverPending)
    {
        m_gameOverPending = false;

        Payload payload;

        if (!m_gameOverMessage.empty())
            payload.Set("message", JsonString(m_gameOverMessage));

        // The winning team, when one alone has the best score.
        std::string winner;
        int best = 0, bestCount = 0;

        for (int team : m_game.PlayingTeams())
        {
            int score;

            if (!m_game.TeamScore(team, score))
                continue;

            if (bestCount == 0 || score > best)
            {
                best = score;
                bestCount = 1;
                winner = TeamNameOf(team);
            }
            else if (score == best)
                ++bestCount;
        }

        if (bestCount == 1 && !winner.empty())
            payload.Set("winner", JsonString(winner));

        SendEvent("game_end", -1, -1, &payload);

        if (EndMatch("game_over", false))
            m_doneThisMap = true;

        return;
    }

    Poll();
    TryStartMatch();

    if (m_match)
        m_game.OnFrame(*this);
}

void Stats::Poll()
{
    m_warmup       = m_game.InWarmup();
    m_intermission = m_game.InIntermission();

    std::vector<std::pair<int, int>> kills, deaths;

    for (int client = 0; client < kMaxClients; ++client)
    {
        if (m_botPending[client])
        {
            m_botPending[client] = false;
            OnClientConnect(client, true);
        }

        Player& p = m_players[client];

        if (!p.connected)
            continue;

        const int oldKills  = p.kills;
        const int oldDeaths = p.deaths;

        PollClient(client);

        // Bots never pass through GAME_CLIENT_BEGIN: Q3's G_AddBot calls the
        // mod's ClientBegin itself. Anyone connected joins here.
        if (m_match && !p.inMatch && !p.key.empty())
            JoinMatch(client);

        if (m_game.LogsKills() || !Recording())
            continue;

        if (oldKills >= 0 && p.kills > oldKills)
            kills.emplace_back(client, p.kills - oldKills);

        if (oldDeaths >= 0 && p.deaths > oldDeaths)
            deaths.emplace_back(client, p.deaths - oldDeaths);
    }

    if (!kills.empty() || !deaths.empty())
        PairKillsAndDeaths(kills, deaths);

    // A game with no game over the plugin can see in its log: the intermission
    // is the end.
    if (m_match && m_intermission && !m_gameOverPending)
        GameOver("");
}

void Stats::PollClient(int client)
{
    Player& p = m_players[client];
    ClientState state;

    if (!m_game.ReadClient(client, state))
        return;

    if (state.hasScore)
    {
        p.hasScore = true;
        p.score    = state.score;
    }

    p.kills  = state.kills;
    p.deaths = state.deaths;

    if (state.team >= 0 && state.team != p.team)
    {
        const int old = p.team;
        p.team = state.team;

        if (p.inMatch && old >= 0)
        {
            Payload payload;
            const std::string from = TeamNameOf(old), to = TeamNameOf(state.team);

            if (!from.empty())
                payload.Set("from", JsonString(from));

            if (!to.empty())
                payload.Set("to", JsonString(to));

            SendEvent("team_change", client, -1, &payload);
            m_reporter.PlayerJoined(m_match, p.key, p.name, to, GameTimeMs());
        }
    }

    if (p.inMatch)
    {
        const std::string team = ReportedTeam(client);

        if (!team.empty() && std::find(m_teamsWithPlayers.begin(), m_teamsWithPlayers.end(), team) == m_teamsWithPlayers.end())
            m_teamsWithPlayers.push_back(team);
    }
}

void Stats::TryStartMatch()
{
    if (m_match || m_doneThisMap || !m_settings.enabled || m_warmup || m_intermission)
        return;

    int humans = 0;

    for (const Player& p : m_players)
    {
        if (p.connected && !p.bot)
            ++humans;
    }

    if (humans < m_settings.minPlayers)
        return;

    StartMatch("", false);
}

bool Stats::StartMatch(const std::string& modeIn, bool manual)
{
    if (m_match || (!manual && !m_settings.enabled))
        return false;

    std::string mode = modeIn;

    if (mode.empty())
        mode = m_settings.mode;

    if (mode.empty())
        mode = m_game.Mode();

    m_matchStartMs = NowMs();
    m_externalId   = ExternalId();

    MatchInfo info;
    info.externalId   = m_externalId;
    info.map          = ToLower(m_game.Cvar("mapname"));
    info.mode         = mode;
    info.gameVersion  = m_game.Cvar("version");
    info.serverName   = StripColors(m_game.Cvar("sv_hostname"));
    info.settingsJson = SettingsJson();
    info.metadataJson = MetadataJson();

    const std::string port = m_game.Cvar("net_port");

    if (!port.empty())
    {
        std::string ip = m_game.Cvar("net_ip");

        if (ip.empty() || ip == "localhost" || ip == "0.0.0.0")
            ip.clear();

        info.serverAddress = ip.empty() ? ":" + port : ip + ":" + port;
    }

    const MatchHandle match = m_reporter.Start(info);

    if (!match)
    {
        m_game.Print("[LANCommander] The Interposer refused to start a match (external id " + m_externalId + ")");
        return false;
    }

    m_match = match;
    m_leavers.clear();
    m_teamsWithPlayers.clear();

    for (int client = 0; client < kMaxClients; ++client)
    {
        m_players[client].inMatch = false;

        if (m_players[client].connected)
            JoinMatch(client);
    }

    m_game.OnMatchStarted(*this);
    m_game.Print("[LANCommander] Recording match " + m_externalId + (mode.empty() ? "" : " (" + mode + ")"));
    return true;
}

bool Stats::EndMatch(const std::string& reason, bool abandoned)
{
    if (!m_match)
        return false;

    for (int client = 0; client < kMaxClients; ++client)
    {
        Player& p = m_players[client];

        if (!p.inMatch)
            continue;

        // The scoreboard as it stands now: the game over comes in the frame of
        // the deciding kill, after the last poll.
        PollClient(client);
        AddTimePlayed(client);

        if (p.hasScore)
            m_reporter.SetStatistic(m_match, p.key, "", "score", p.score);
    }

    if (!abandoned)
        SetResults();

    m_reporter.End(m_match, reason, abandoned);
    m_game.Print("[LANCommander] Match " + m_externalId + " ended (" + reason + (abandoned ? ", abandoned)" : ")"));

    m_match = 0;
    m_externalId.clear();

    for (Player& p : m_players)
        p.inMatch = false;

    return true;
}

void Stats::GameOver(const std::string& message)
{
    if (!m_match)
        return;

    m_gameOverPending = true;
    m_gameOverMessage = message;
}

void Stats::SetResults()
{
    std::vector<StandingPlayer> players;
    bool anyScore = false;

    for (int client = 0; client < kMaxClients; ++client)
    {
        const Player& p = m_players[client];

        if (!p.inMatch)
            continue;

        anyScore |= p.hasScore;
        players.push_back({ p.key, ReportedTeam(client), p.score, true });
    }

    for (const auto& [key, leaver] : m_leavers)
        players.push_back({ key, TeamNameOf(leaver.team), leaver.score, false });

    std::vector<StandingTeam> teams;

    for (int team : m_game.PlayingTeams())
    {
        const std::string name = TeamNameOf(team);
        int score;

        if (name.empty() || !m_game.TeamScore(team, score))
            continue;

        if (std::find(m_teamsWithPlayers.begin(), m_teamsWithPlayers.end(), name) == m_teamsWithPlayers.end())
            continue;

        teams.push_back({ name, score });
        m_reporter.SetStatistic(m_match, "", name, "score", score);
    }

    // Nothing the game told us ranks anyone: no results rather than made-up ones.
    if (!anyScore && teams.size() < 2)
        return;

    const Standings standings = ComputeStandings(players, teams);

    for (const MatchResult& result : standings.teams)
        m_reporter.SetResult(m_match, result);

    for (const MatchResult& result : standings.players)
        m_reporter.SetResult(m_match, result);
}

// ── Players ──────────────────────────────────────────────────────────────────

void Stats::ResetPlayers()
{
    for (Player& p : m_players)
        p = Player{};

    m_botPending.fill(false);
}

std::string Stats::BuildKey(int client, const std::string& userinfo) const
{
    const Player& p = m_players[client];

    if (p.bot)
        return m_settings.recordBots ? "bot:" + p.name : std::string();

    // A client's own GUID, where the game (or its engine) has one: ioq3 and ET
    // cl_guid, OpenJK ja_guid. PunkBuster's "unknown" and the like are not ids.
    for (const char* field : { "cl_guid", "ja_guid", "guid" })
    {
        const std::string guid = InfoValue(userinfo, field);

        if (guid.size() < 8 || guid.size() > 64)
            continue;

        const bool clean = std::all_of(guid.begin(), guid.end(),
            [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'; });

        const std::string lower = ToLower(guid);

        if (clean && lower != "unknown" && lower != "no_guid" && lower.find_first_not_of('0') != std::string::npos)
            return "guid:" + guid;
    }

    return p.name.empty() ? std::string() : "name:" + p.name;
}

void Stats::OnClientConnect(int client, bool bot)
{
    if (client < 0 || client >= kMaxClients)
        return;

    // A connect on a slot still in use (a missed disconnect): the old one left.
    if (m_players[client].connected)
        OnClientDisconnect(client);

    Player& p = m_players[client];
    p = Player{};
    p.connected = true;
    p.bot       = bot;

    const std::string userinfo = m_game.Userinfo(client);
    p.name = StripColors(InfoValue(userinfo, "name"));
    p.key  = BuildKey(client, userinfo);
}

void Stats::OnBotAllocated(int client)
{
    if (client >= 0 && client < kMaxClients)
        m_botPending[client] = true;
}

void Stats::OnClientBegin(int client)
{
    if (client < 0 || client >= kMaxClients || !m_players[client].connected)
        return;

    PollClient(client);

    if (m_match && !m_players[client].inMatch)
        JoinMatch(client);
}

void Stats::OnClientUserinfoChanged(int client)
{
    if (client < 0 || client >= kMaxClients || !m_players[client].connected)
        return;

    Player& p = m_players[client];
    const std::string name = StripColors(InfoValue(m_game.Userinfo(client), "name"));

    if (name.empty() || name == p.name)
        return;

    const std::string old = p.name;
    p.name = name;

    if (p.key.empty())
        return;

    if (p.inMatch)
    {
        Payload payload;
        payload.Set("from", JsonString(old));
        payload.Set("to", JsonString(name));
        SendEvent("name_change", client, -1, &payload);

        // The key stays: a rename does not split a player in two.
        m_reporter.PlayerJoined(m_match, p.key, p.name, ReportedTeam(client), GameTimeMs());
    }
}

void Stats::OnClientDisconnect(int client)
{
    if (client < 0 || client >= kMaxClients || !m_players[client].connected)
        return;

    if (m_players[client].inMatch)
    {
        PollClient(client);
        SendEvent("disconnect", client, -1);
        LeaveMatch(client);
    }

    m_players[client] = Player{};
}

void Stats::JoinMatch(int client)
{
    Player& p = m_players[client];

    if (!m_match || p.key.empty())
        return;

    m_reporter.PlayerJoined(m_match, p.key, p.name, ReportedTeam(client), GameTimeMs());

    if (!p.inMatch)
    {
        p.inMatch    = true;
        p.joinedAtMs = NowMs();

        // Back after leaving: a finisher again, not a leaver.
        m_leavers.erase(p.key);
    }
}

void Stats::LeaveMatch(int client)
{
    Player& p = m_players[client];

    AddTimePlayed(client);

    if (p.hasScore)
        m_reporter.SetStatistic(m_match, p.key, "", "score", p.score);

    m_leavers[p.key] = Leaver{ p.score, p.team };
    m_reporter.PlayerLeft(m_match, p.key, GameTimeMs());
    p.inMatch = false;
}

void Stats::AddTimePlayed(int client)
{
    Player& p = m_players[client];

    if (!p.inMatch)
        return;

    const long long now = NowMs();
    const long long played = now - p.joinedAtMs;

    if (played > 0)
        m_reporter.AddStatistic(m_match, p.key, "", "time_played_ms", static_cast<double>(played));

    p.joinedAtMs = now;
}

bool Stats::IsPlayer(int client) const
{
    return client >= 0 && client < kMaxClients && m_players[client].connected;
}

std::string Stats::Key(int client) const
{
    return IsPlayer(client) ? m_players[client].key : std::string();
}

std::string Stats::TeamNameOf(int team) const
{
    return team >= 0 ? m_game.TeamName(team) : std::string();
}

std::string Stats::ReportedTeam(int client) const
{
    return IsPlayer(client) ? TeamNameOf(m_players[client].team) : std::string();
}

void Stats::OnChat(int client, const std::string& text, bool teamOnly)
{
    if (!m_settings.recordChat || !IsPlayer(client))
        return;

    const std::string message(Trim(StripColors(text)));

    if (message.empty())
        return;

    Payload payload;
    payload.Set("message", JsonString(message));

    if (teamOnly)
        payload.Set("team_only", "true");

    SendEvent("chat", client, -1, &payload);
}

// ── Events ───────────────────────────────────────────────────────────────────

bool Stats::Recording() const
{
    return m_match != 0;
}

long long Stats::SteadyMs() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_epoch).count();
}

long long Stats::NowMs() const
{
    return m_levelTimeMs >= 0 ? m_levelTimeMs : SteadyMs();
}

long long Stats::GameTimeMs() const
{
    if (!m_match)
        return 0;

    const long long elapsed = NowMs() - m_matchStartMs;
    return elapsed > 0 ? elapsed : 0;
}

bool Stats::EventFiltered(const std::string& type) const
{
    if (!Trim(m_settings.eventsInclude).empty() && !MatchesAny(m_settings.eventsInclude, type))
        return true;

    return MatchesAny(m_settings.eventsExclude, type);
}

bool Stats::SendEvent(const std::string& type, int actor, int subject, Payload* payloadIn)
{
    if (!Recording())
        return false;

    const std::string actorKey   = Key(actor);
    const std::string subjectKey = Key(subject);

    // Only unrecorded players (bots, with lc_record_bots 0) involved.
    if ((IsPlayer(actor) || IsPlayer(subject)) && actorKey.empty() && subjectKey.empty())
        return false;

    if (EventFiltered(type))
        return true;

    Payload local;
    Payload& payload = payloadIn ? *payloadIn : local;

    if (m_round > 0 && !payload.Has("round"))
        payload.Set("round", JsonNumber(m_round));

    std::string json = payload.Size() ? payload.ToJson() : std::string();

    if (json.size() > kMaxPayloadBytes)
    {
        m_game.Print("[LANCommander] A " + type + " event is over the size LANCommander takes; its detail is left out");
        json.clear();
    }

    return m_reporter.Event(m_match, type, GameTimeMs(), -1, actorKey, subjectKey, json);
}

void Stats::AddStat(int client, const std::string& name, double delta)
{
    const std::string key = Key(client);

    if (!Recording() || key.empty() || !m_players[client].inMatch)
        return;

    m_reporter.AddStatistic(m_match, key, "", name, delta);
}

void Stats::AddTeamStat(const std::string& team, const std::string& name, double delta)
{
    if (!Recording() || team.empty())
        return;

    m_reporter.AddStatistic(m_match, "", team, name, delta);
}

void Stats::AddPosition(Payload& payload, const char* prefix, int client)
{
    if (!m_settings.recordPositions || !IsPlayer(client))
        return;

    ClientState state;

    if (!m_game.ReadClient(client, state) || !state.hasPosition)
        return;

    payload.Set(std::string(prefix) + "_position",
        "[" + JsonNumber(state.origin[0]) + "," + JsonNumber(state.origin[1]) + "," + JsonNumber(state.origin[2]) + "]");
}

void Stats::HandleKill(int attacker, int victim, const std::string& weapon)
{
    // An NPC's death (Jedi Academy logs those too).
    if (!IsPlayer(victim))
        return;

    const bool attackerIsPlayer = IsPlayer(attacker);
    const bool suicide  = !attackerIsPlayer || attacker == victim;
    const bool teamkill = !suicide && m_players[attacker].team >= 0 && m_players[attacker].team == m_players[victim].team &&
        !ReportedTeam(victim).empty() && !m_game.PlayingTeams().empty();

    Payload payload;

    if (!weapon.empty())
        payload.Set("weapon", JsonString(WeaponName(weapon)));

    if (suicide)
        payload.Set("suicide", "true");

    if (attacker >= 0 && attacker == m_game.WorldEntity())
        payload.Set("world", "true");

    if (teamkill)
        payload.Set("teamkill", "true");

    AddPosition(payload, "victim", victim);

    if (!suicide)
    {
        AddPosition(payload, "attacker", attacker);

        ClientState from, to;

        if (m_settings.recordPositions && m_game.ReadClient(attacker, from) && m_game.ReadClient(victim, to) &&
            from.hasPosition && to.hasPosition)
        {
            const float dx = from.origin[0] - to.origin[0];
            const float dy = from.origin[1] - to.origin[1];
            const float dz = from.origin[2] - to.origin[2];
            payload.Set("distance", JsonNumber(std::sqrt(dx * dx + dy * dy + dz * dz)));
        }
    }

    if (!SendEvent("kill", attackerIsPlayer ? attacker : -1, victim, &payload))
        return;

    AddStat(victim, "deaths", 1);

    if (suicide)
        AddStat(victim, "suicides", 1);
    else if (teamkill)
        AddStat(attacker, "teamkills", 1);
    else
        AddStat(attacker, "kills", 1);
}

// Games that keep kill and death counts but log no kills (MOHAA): the counts
// that went up in the same frame. One killer and one victim make a kill event;
// anything less clear only counts.
void Stats::PairKillsAndDeaths(const std::vector<std::pair<int, int>>& kills,
                               const std::vector<std::pair<int, int>>& deaths)
{
    for (const auto& [client, count] : kills)
        AddStat(client, "kills", count);

    if (kills.size() == 1 && deaths.size() == 1 && kills[0].second == 1 && deaths[0].second == 1 &&
        kills[0].first != deaths[0].first)
    {
        const int attacker = kills[0].first, victim = deaths[0].first;
        Payload payload;
        AddPosition(payload, "victim", victim);
        AddPosition(payload, "attacker", attacker);
        SendEvent("kill", attacker, victim, &payload);
        AddStat(victim, "deaths", 1);
        return;
    }

    for (const auto& [client, count] : deaths)
    {
        if (kills.empty())
        {
            Payload payload;
            payload.Set("suicide", "true");
            AddPosition(payload, "victim", client);
            SendEvent("kill", -1, client, &payload);
        }

        AddStat(client, "deaths", count);
    }
}

void Stats::OnPrint(std::string_view text)
{
    const std::optional<LogLine> line = ParseLogLine(text);

    if (!line)
        return;

    switch (line->kind)
    {
    case LogKind::Kill:
        if (Recording())
            HandleKill(line->attacker, line->victim, line->weapon);
        break;

    case LogKind::Exit:
        GameOver(line->rest);
        break;

    case LogKind::Item:
        if (m_settings.recordItems && IsPlayer(line->client))
        {
            Payload payload;
            payload.Set("item", JsonString(line->item));
            SendEvent("item", line->client, -1, &payload);
        }
        break;

    default:
        break;
    }

    m_game.OnLogLine(*this, *line);
}

// ── What is kept with the match ──────────────────────────────────────────────

std::string Stats::ExternalId() const
{
    // This server, this map and when the match started: one token.
    std::string port = m_game.Cvar("net_port");
    std::string map  = ToLower(m_game.Cvar("mapname"));

    for (char& c : map)
    {
        if (c == '/' || c == '\\' || c == ' ')
            c = '_';
    }

    return std::string(m_game.Code()) + "-" + (port.empty() ? "0" : port) + "-" + map + "-" +
        std::to_string(static_cast<long long>(std::time(nullptr)));
}

std::string Stats::SettingsJson() const
{
    Payload settings;
    const std::string list = m_settings.settingsCvars.empty() ? m_game.SettingsCvars() : m_settings.settingsCvars;

    for (const std::string& name : SplitList(list))
    {
        const std::string value = m_game.Cvar(name.c_str());

        if (!value.empty())
            settings.Set(name, JsonString(value));
    }

    return settings.Size() ? settings.ToJson() : std::string();
}

std::string Stats::MetadataJson() const
{
    Payload metadata;
    metadata.Set("game", JsonString(m_game.Code()));

    const std::string mod = m_game.ModDir();

    if (!mod.empty())
        metadata.Set("mod", JsonString(mod));

    const std::string gamename = m_game.Cvar("gamename");

    if (!gamename.empty())
        metadata.Set("gamename", JsonString(gamename));

    metadata.Set("plugin_version", JsonString(LC_PLUGIN_VERSION));
    metadata.Set("max_players", JsonNumber(m_game.MaxClients()));

    return metadata.ToJson();
}

// ── Admin commands ───────────────────────────────────────────────────────────

bool Stats::OnConsoleCommand(const std::vector<std::string>& argv)
{
    if (argv.empty())
        return false;

    const std::string command = ToLower(argv[0]);
    const std::string arg = argv.size() > 1 ? argv[1] : std::string();

    if (command == "lc_status")
    {
        PrintStatus();
        return true;
    }

    if (command == "lc_flush")
    {
        if (!m_match)
            m_game.Print("[LANCommander] No match is being recorded");
        else
            m_game.Print(m_reporter.Flush(m_match, 5000) ? "[LANCommander] Everything has been sent"
                                                         : "[LANCommander] Some of the match is still waiting to be sent");
        return true;
    }

    if (command == "lc_startmatch")
    {
        if (m_match)
            m_game.Print("[LANCommander] A match is already being recorded: lc_endmatch first");
        else if (!StartMatch(arg, true))
            m_game.Print("[LANCommander] Could not start a match");
        return true;
    }

    if (command == "lc_endmatch" || command == "lc_abandonmatch")
    {
        const bool abandoned = command == "lc_abandonmatch";

        if (EndMatch(abandoned ? "aborted" : (arg.empty() ? "admin" : arg), abandoned))
            m_doneThisMap = true;
        else
            m_game.Print("[LANCommander] No match is being recorded");
        return true;
    }

    return false;
}

void Stats::PrintStatus()
{
    const Settings& s = m_settings;

    m_game.Print(std::string("[LANCommander] Reporting ") + (s.enabled ? "on" : "off (lc_enabled 0)") + ", to " +
        (s.delivery.serverUrl.empty() ? "nowhere (lc_server_url is empty)" : s.delivery.serverUrl) +
        (s.delivery.token.empty() ? ", no lc_token" : "") + (s.delivery.gameId.empty() ? ", no lc_game_id" : ""));

    if (!m_match)
    {
        int humans = 0;

        for (const Player& p : m_players)
            humans += p.connected && !p.bot;

        m_game.Print("[LANCommander] No match is being recorded" +
            std::string(m_doneThisMap ? " (this map's match has ended)" :
                        m_warmup ? " (warmup)" :
                        humans < s.minPlayers ? " (" + std::to_string(humans) + " of lc_min_players " + std::to_string(s.minPlayers) + " players)" : ""));
        return;
    }

    int players = 0;

    for (const Player& p : m_players)
        players += p.inMatch;

    m_game.Print("[LANCommander] Recording " + m_externalId + ": " + std::to_string(players) + " players, " +
        std::to_string(GameTimeMs() / 1000) + " s");

    DeliveryStatus status;

    if (m_reporter.Status(m_match, status))
    {
        static const char* const kSinks[] = { "logged only", "sent to LANCommander", "sent by the launcher" };
        m_game.Print(std::string("[LANCommander] ") + kSinks[std::clamp(status.sink, 0, 2)] +
            (status.matchId.empty() ? "" : ", LANCommander match " + status.matchId) +
            ", " + std::to_string(status.pendingEvents) + " events waiting, " +
            std::to_string(status.rejectedEvents) + " rejected" +
            (status.lastError.empty() ? "" : ", last problem: " + status.lastError));
    }
}

} // namespace lc
