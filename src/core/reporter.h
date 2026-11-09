// Where match reports go: the Interposer's match API (interposer.cpp) in the
// plugin, a recording fake in the unit tests. One method per InterposerMatch*
// function, with C++ types.

#pragma once

#include <cstdint>
#include <string>

namespace lc
{

using MatchHandle = std::uint32_t;

// InterposerMatchResult.status
enum class ResultStatus : int
{
    Finished     = 0,
    DidNotFinish = 1,
    Disqualified = 2,
    Defeated     = 3,
    Forfeited    = 4,
};

struct DeliverySettings
{
    bool        enabled = true;
    std::string serverUrl;
    std::string token;
    std::string gameId;
    std::string serverId;
    std::string reporterName;
    int         flushIntervalMs = 2000;

    bool operator==(const DeliverySettings& o) const
    {
        return enabled == o.enabled && serverUrl == o.serverUrl && token == o.token && gameId == o.gameId &&
            serverId == o.serverId && reporterName == o.reporterName && flushIntervalMs == o.flushIntervalMs;
    }
};

struct MatchInfo
{
    std::string externalId;
    std::string map;
    std::string mode;
    std::string gameVersion;
    std::string serverName;
    std::string serverAddress;
    std::string settingsJson;   // a JSON object, or empty
    std::string metadataJson;   // a JSON object, or empty
};

struct MatchResult
{
    std::string  playerKey;     // a player, a team, or both
    std::string  team;
    int          rank   = 0;
    ResultStatus status = ResultStatus::Finished;
    bool         hasScore = false;
    double       score    = 0;
};

struct DeliveryStatus
{
    int          sink = 0;              // 0 none (logged only), 1 HTTP, 2 the launcher
    std::string  matchId;               // LANCommander's id, once known
    unsigned     pendingEvents      = 0;
    unsigned     rejectedEvents     = 0;
    unsigned     pendingAttachments = 0;
    bool         ended     = false;
    bool         delivered = false;
    std::string  lastError;
};

class Reporter
{
public:
    virtual ~Reporter() = default;

    virtual bool        Configure(const DeliverySettings& settings) = 0;
    virtual MatchHandle Start(const MatchInfo& info) = 0;
    virtual bool PlayerJoined(MatchHandle match, const std::string& key, const std::string& name,
                              const std::string& team, long long gameTimeMs) = 0;
    virtual bool PlayerLeft(MatchHandle match, const std::string& key, long long gameTimeMs) = 0;
    virtual bool Event(MatchHandle match, const std::string& type, long long gameTimeMs, long long tick,
                       const std::string& actorKey, const std::string& subjectKey, const std::string& payloadJson) = 0;
    virtual bool SetStatistic(MatchHandle match, const std::string& key, const std::string& team,
                              const std::string& name, double value) = 0;
    virtual bool AddStatistic(MatchHandle match, const std::string& key, const std::string& team,
                              const std::string& name, double delta) = 0;
    virtual bool SetResult(MatchHandle match, const MatchResult& result) = 0;
    virtual bool End(MatchHandle match, const std::string& reason, bool abandoned) = 0;
    virtual bool Flush(MatchHandle match, unsigned timeoutMs) = 0;
    virtual bool Status(MatchHandle match, DeliveryStatus& status) = 0;
};

// The Interposer, embedded in this plugin and attached once per process.
// Attach pins the plugin's image so QMM's unload at each map change leaves it
// (and the Interposer's delivery thread) in place. Returns false only when the
// Interposer could not be brought up; a second Interposer already in the
// process is fine, and the calls forward to it.
bool AttachInterposer(std::string& message);
Reporter& InterposerReporter();

} // namespace lc
