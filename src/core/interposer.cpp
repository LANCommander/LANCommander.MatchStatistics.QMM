// The Interposer, embedded: attached once per process, with this plugin's
// image pinned so it survives QMM's unload at every map change. The Interposer
// keeps a delivery thread and the open matches; unloading the code under them
// would crash, and detaching and re-attaching at each map change would hold up
// the change for the flush. Pinned, the plugin's statics (the open match, the
// players) also carry over to the next QMM_Attach.
//
// The delivery thread never calls back into the plugin: no match callback is
// registered, so QMM's rule that only the game thread calls into QMM, the
// engine or the mod holds.

#include "reporter.h"
#include "payload.h"

#include <LANCommander.Interposer.h>
#include <LANCommander.Interposer.Embedded.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace lc
{

namespace
{

bool g_attached = false;

#ifdef _WIN32
HMODULE SelfModule()
{
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&SelfModule), &module);
    return module;
}

bool PinSelf()
{
    HMODULE module = nullptr;
    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&PinSelf), &module) != FALSE;
}
#else
bool PinSelf()
{
    Dl_info info{};

    if (!dladdr(reinterpret_cast<void*>(&PinSelf), &info) || !info.dli_fname)
        return false;

    // RTLD_NOLOAD: the image already loaded, now marked never to be unloaded.
    return dlopen(info.dli_fname, RTLD_NOW | RTLD_NOLOAD | RTLD_NODELETE) != nullptr;
}
#endif

const char* OrNull(const std::string& text)
{
    return text.empty() ? nullptr : text.c_str();
}

class InterposerSink final : public Reporter
{
public:
    bool Configure(const DeliverySettings& s) override
    {
        InterposerMatchConfig config{};
        config.structSize      = sizeof(config);
        config.enabled         = s.enabled ? 1 : 0;
        config.serverUrl       = s.serverUrl.c_str();
        config.token           = s.token.c_str();
        config.gameId          = s.gameId.c_str();
        config.serverId        = s.serverId.c_str();
        config.reporterKind    = "Server";
        config.reporterName    = s.reporterName.c_str();
        config.flushIntervalMs = s.flushIntervalMs;
        return InterposerMatchConfigure(&config) != FALSE;
    }

    MatchHandle Start(const MatchInfo& m) override
    {
        InterposerMatchInfo info{};
        info.structSize    = sizeof(info);
        info.externalId    = OrNull(m.externalId);
        info.map           = OrNull(m.map);
        info.mode          = OrNull(m.mode);
        info.gameVersion   = OrNull(m.gameVersion);
        info.serverName    = OrNull(m.serverName);
        info.serverAddress = OrNull(m.serverAddress);
        info.settingsJson  = OrNull(m.settingsJson);
        info.metadataJson  = OrNull(m.metadataJson);
        info.startedOn     = 0;
        return InterposerMatchStart(&info);
    }

    bool PlayerJoined(MatchHandle match, const std::string& key, const std::string& name, const std::string& team,
                      long long gameTimeMs) override
    {
        return InterposerMatchPlayerJoined(match, key.c_str(), OrNull(name), OrNull(team), gameTimeMs) != FALSE;
    }

    bool PlayerLeft(MatchHandle match, const std::string& key, long long gameTimeMs) override
    {
        return InterposerMatchPlayerLeft(match, key.c_str(), gameTimeMs) != FALSE;
    }

    bool Event(MatchHandle match, const std::string& type, long long gameTimeMs, long long tick,
               const std::string& actorKey, const std::string& subjectKey, const std::string& payloadJson) override
    {
        return InterposerMatchEvent(match, type.c_str(), gameTimeMs, tick, OrNull(actorKey), OrNull(subjectKey),
            OrNull(payloadJson)) != FALSE;
    }

    bool SetStatistic(MatchHandle match, const std::string& key, const std::string& team, const std::string& name,
                      double value) override
    {
        return InterposerMatchSetStatistic(match, OrNull(key), OrNull(team), name.c_str(), value) != FALSE;
    }

    bool AddStatistic(MatchHandle match, const std::string& key, const std::string& team, const std::string& name,
                      double delta) override
    {
        return InterposerMatchAddStatistic(match, OrNull(key), OrNull(team), name.c_str(), delta) != FALSE;
    }

    bool SetResult(MatchHandle match, const MatchResult& r) override
    {
        InterposerMatchResult result{};
        result.structSize  = sizeof(result);
        result.playerKey   = OrNull(r.playerKey);
        result.team        = OrNull(r.team);
        result.rank        = r.rank;
        result.status      = static_cast<int>(r.status);
        result.hasScore    = r.hasScore ? 1 : 0;
        result.score       = r.score;
        result.timeMs      = -1;
        result.payloadJson = nullptr;
        return InterposerMatchSetResult(match, &result) != FALSE;
    }

    bool End(MatchHandle match, const std::string& reason, bool abandoned) override
    {
        return InterposerMatchEnd(match, OrNull(reason), abandoned ? TRUE : FALSE) != FALSE;
    }

    bool Flush(MatchHandle match, unsigned timeoutMs) override
    {
        return InterposerMatchFlush(match, timeoutMs) != FALSE;
    }

    bool Status(MatchHandle match, DeliveryStatus& out) override
    {
        InterposerMatchStatus status{};
        status.size = sizeof(status);

        if (!InterposerMatchGetStatus(match, &status))
            return false;

        out.sink               = status.sink;
        out.matchId            = status.matchId;
        out.pendingEvents      = status.pendingEvents;
        out.rejectedEvents     = status.rejectedEvents;
        out.pendingAttachments = status.pendingAttachments;
        out.ended              = status.ended != 0;
        out.delivered          = status.delivered != 0;
        out.lastError          = WideToUtf8(status.lastError);
        return true;
    }
};

} // namespace

bool AttachInterposer(std::string& message)
{
    message.clear();

    if (g_attached)
        return true;

    if (!PinSelf())
    {
        message = "could not pin the plugin in memory; not reporting, since QMM would unload it under the Interposer";
        return false;
    }

#ifdef _WIN32
    // API-only: no detour anywhere in the server.
    const BOOL primary = InterposerEmbeddedAttachApiOnly(SelfModule());
#else
    const BOOL primary = InterposerEmbeddedAttach();
#endif

    // FALSE is not a failure: another Interposer already owns this process (one
    // LANCommander deployed beside the server) and every call forwards to it.
    if (!primary)
        message = "another LANCommander Interposer is loaded in this process; reporting through it";

    g_attached = true;
    return true;
}

Reporter& InterposerReporter()
{
    static InterposerSink sink;
    return sink;
}

} // namespace lc
