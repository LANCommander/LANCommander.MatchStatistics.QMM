// The QMM plugin: QMM's exports, routing the game's calls to the core
// (src/core/stats.h) and its module (src/games/). Built once per game, with
// that game's SDK (sdk.h).
//
// QMM unloads its plugins at every GAME_SHUTDOWN, so at every map change and
// map_restart. This one pins itself in memory when it first attaches (see
// src/core/interposer.cpp): its statics, the core included, live as long as
// the server process, and each QMM_Attach after the first finds them as the
// last map left them.

// The core's headers before the SDK's (see qmm_game.h).
#include "../core/reporter.h"
#include "../core/stats.h"
#include "../core/text.h"

#include <cstring>
#include <string>
#include <vector>

#include "sdk.h"

#ifndef LC_PLUGIN_VERSION
#define LC_PLUGIN_VERSION "0.0.0"
#endif

plugin_info g_plugininfo =
{
    QMM_PIFV_MAJOR,
    QMM_PIFV_MINOR,
    "LANCommander",
    LC_PLUGIN_VERSION,
    "Reports matches to LANCommander's match statistics",
    "LANCommander",
    "https://github.com/LANCommander/LANCommander.QMM.MatchStatistics",
    "LANCOMMANDER",
    0,
    0,
};
plugin_funcs g_pluginfuncs;
plugin_vars  g_pluginvars;

gentity_t* g_gents      = nullptr;
intptr_t   g_numgents   = 0;
intptr_t   g_gentsize   = 0;
gclient_t* g_clients    = nullptr;
intptr_t   g_clientsize = 0;

namespace
{

bool g_active = false;

#if defined(LC_LOG_FROM_FILE)
// The mod's g_log file (sdk.h): the handle it was given, and while it is being
// opened, where the engine writes that handle.
int  g_logFile        = 0;
int* g_logFileOpening = nullptr;

bool IsLogFile(const char* path)
{
    const char* log = QMM_GETSTRCVAR("g_log");
    return path && log && *log && lc::ToLower(path) == lc::ToLower(log);
}
#endif

lc::Stats& Core()
{
    static lc::Stats stats(lc::TheGame(), lc::InterposerReporter());
    return stats;
}

// The client number in a GAME_CLIENT_* call's first argument: the number
// itself, or (see sdk.h) the client's entity.
int ClientOfArg(intptr_t arg, bool connect)
{
#if defined(LC_FAMILY_Q3)
    (void)connect;
    return static_cast<int>(arg);
#else
#if defined(LC_FAMILY_UBER)
    if (connect)
        return static_cast<int>(arg);
#else
    (void)connect;
#endif
    if (!arg || !g_gents || g_gentsize <= 0)
        return -1;

    const int entity = static_cast<int>(NUM_FROM_ENT(arg));

#if defined(LC_FAMILY_Q2)
    return entity - 1;
#else
    return entity;
#endif
#endif
}

std::vector<std::string> CommandArgs()
{
    std::vector<std::string> argv;
    const intptr_t argc = QMM_SYSCALL(G_ARGC);

    for (intptr_t i = 0; i < argc; ++i)
    {
        const char* arg = QMM_ARGV2(i);
        argv.emplace_back(arg ? arg : "");
    }

    return argv;
}

std::string JoinArgs(const std::vector<std::string>& argv, size_t from)
{
    std::string text;

    for (size_t i = from; i < argv.size(); ++i)
    {
        if (i > from)
            text += ' ';
        text += argv[i];
    }

    return text;
}

void OnClientCommand(int client)
{
    const std::vector<std::string> argv = CommandArgs();

    if (argv.empty())
        return;

    const std::string command = lc::ToLower(argv[0]);

    if (command == "say")
        Core().OnChat(client, JoinArgs(argv, 1), false);
    else if (command == "say_team" || command == "say_buddy" || command == "say_teamnl")
        Core().OnChat(client, JoinArgs(argv, 1), true);
    else if (command == "dmmessage" && argv.size() > 2)
    {
        // MOHAA: dmmessage <0 everyone, -1 team, n one player> <text>
        const int target = lc::ParseInt(argv[1], 1);

        if (target <= 0)
            Core().OnChat(client, JoinArgs(argv, 2), target < 0);
    }
}

} // namespace

C_DLLEXPORT void QMM_Query(plugin_info** plugininfo, plugin_funcs** pluginfuncs, plugin_vars** pluginvars)
{
    QMM_GIVE_PTRS();
}

C_DLLEXPORT int QMM_Attach()
{
    const char* engine = QMM_GETGAMEENGINE();

#if defined(GAME_CODUOMP)
    // QMM's game_coduomp.cpp reports CoD: United Offensive as "CODMP".
    if (std::strcmp(engine, "CODUOMP") != 0 && std::strcmp(engine, "CODMP") != 0)
        return 0;
#else
    if (std::strcmp(engine, GAME_STR) != 0)
        return 0;
#endif

    // The mod's arrays are new with every load; G_LOCATE_GAME_DATA gives them.
    g_gents      = nullptr;
    g_numgents   = 0;
    g_gentsize   = 0;
    g_clients    = nullptr;
    g_clientsize = 0;

    std::string message;
    g_active = lc::AttachInterposer(message);

    if (!message.empty())
        QMM_WRITEQMMLOG(g_active ? QMMLOG_NOTICE : QMMLOG_ERROR, "%s\n", message.c_str());

    return g_active ? 1 : 0;
}

C_DLLEXPORT void QMM_Detach()
{
    // Nothing: the Interposer stays attached for the next map (see the top).
}

C_DLLEXPORT intptr_t QMM_vmMain(result_vars* results, intptr_t cmd, intptr_t* args)
{
    if (!g_active)
        QMM_RET_IGNORED(0);

    if (cmd == GAME_CLIENT_DISCONNECT)
        Core().OnClientDisconnect(ClientOfArg(args[0], false));
    else if (cmd == GAME_CLIENT_COMMAND)
        OnClientCommand(ClientOfArg(args[0], false));
    else if (cmd == GAME_CONSOLE_COMMAND)
    {
        std::vector<std::string> argv = CommandArgs();

#if defined(LC_FAMILY_Q2)
        // Quake II passes server commands as "sv <command> ...".
        if (!argv.empty() && lc::ToLower(argv[0]) == "sv")
            argv.erase(argv.begin());
#endif

        if (Core().OnConsoleCommand(argv))
            QMM_RET_SUPERCEDE(1);
    }
    else if (cmd == GAME_SHUTDOWN)
    {
#if defined(LC_FAMILY_Q3)
        Core().OnShutdown(args[0] != 0);
#else
        Core().OnShutdown(false);
#endif
    }

    QMM_RET_IGNORED(0);
}

C_DLLEXPORT intptr_t QMM_vmMain_Post(result_vars* results, intptr_t cmd, intptr_t* args)
{
    if (!g_active)
        QMM_RET_IGNORED(0);

    if (cmd == GAME_INIT)
    {
#if defined(LC_FAMILY_Q3)
        // GAME_INIT(levelTime, randomSeed, restart)
        Core().OnInit(args[2] != 0);
#else
        Core().OnInit(false);
#endif
    }
    else if (cmd == GAME_RUN_FRAME)
    {
#if defined(LC_FAMILY_Q2)
        Core().OnFrame(-1);
#else
        Core().OnFrame(static_cast<long long>(args[0]));
#endif
    }
    else if (cmd == GAME_CLIENT_CONNECT)
    {
        // Refused connections (a ban, a full server) are not players.
#if defined(LC_FAMILY_Q2)
        const bool allowed = QMM_VAR_FINAL_RETURN(intptr_t) != 0;
#else
        const bool allowed = QMM_VAR_FINAL_RETURN(intptr_t) == 0;
#endif

#if defined(LC_CONNECT_ISBOT_ARG)
        const bool bot = args[LC_CONNECT_ISBOT_ARG] != 0;
#else
        const bool bot = false;
#endif

        if (allowed)
            Core().OnClientConnect(ClientOfArg(args[0], true), bot);
    }
    else if (cmd == GAME_CLIENT_USERINFO_CHANGED)
        Core().OnClientUserinfoChanged(ClientOfArg(args[0], false));
    else if (cmd == GAME_CLIENT_BEGIN)
        Core().OnClientBegin(ClientOfArg(args[0], false));

    QMM_RET_IGNORED(0);
}

C_DLLEXPORT intptr_t QMM_syscall(result_vars* results, intptr_t cmd, intptr_t* args)
{
    if (cmd == G_LOCATE_GAME_DATA)
    {
        g_gents      = reinterpret_cast<gentity_t*>(args[0]);
        g_numgents   = args[1];
        g_gentsize   = args[2];
        g_clients    = reinterpret_cast<gclient_t*>(args[3]);
        g_clientsize = args[4];
    }

#if defined(LC_LOG_FROM_FILE)
    // The log lines, as the mod writes them to g_log's file (sdk.h). The mod
    // opens it in its GAME_INIT, which QMM passes on after loading plugins.
    else if (cmd == G_FS_FOPEN_FILE && g_active && args[1] && IsLogFile(reinterpret_cast<const char*>(args[0])))
        g_logFileOpening = reinterpret_cast<int*>(args[1]);
    else if (cmd == G_FS_WRITE && g_active && g_logFile > 0 && args[2] == g_logFile && args[0] && args[1] > 0)
        Core().OnLogWrite(std::string_view(reinterpret_cast<const char*>(args[0]), static_cast<size_t>(args[1])));
    else if (cmd == G_FS_FCLOSE_FILE && g_logFile > 0 && args[0] == g_logFile)
        g_logFile = 0;
#elif defined(LC_FAMILY_Q3)
    // G_LogPrintf's console echo (sdk.h): the log lines.
    else if (cmd == G_PRINT && g_active && args[0])
    {
        const char* text = reinterpret_cast<const char*>(args[0]);

#if defined(GAME_JAMP)
        // OpenJK's GetModuleAPI mods call Print(fmt, ...); its G_LogPrintf echo
        // is Print("%s", line). Our fork's QMM2 formats that before dispatching
        // (fix/openjk-print-format), so text is the real line. This keeps the
        // plugin working on an older QMM2 that still passes it on unformatted.
        if (std::strcmp(text, "%s") == 0)
            text = reinterpret_cast<const char*>(args[1]);
#endif

        if (text)
            Core().OnPrint(text);
    }
#endif

    QMM_RET_IGNORED(0);
}

C_DLLEXPORT intptr_t QMM_syscall_Post(result_vars* results, intptr_t cmd, intptr_t* /*args*/)
{
#if defined(LC_LOG_FROM_FILE)
    if (cmd == G_FS_FOPEN_FILE && g_logFileOpening)
    {
        g_logFile        = *g_logFileOpening;
        g_logFileOpening = nullptr;
    }
#endif

#if defined(LC_HAS_BOTLIB)
    // The mod adds a bot by calling its own ClientConnect, which QMM never
    // sees; the slot the engine allocates for it is the one sign (sdk.h).
    if (cmd == G_BOT_ALLOCATE_CLIENT && g_active)
    {
        const intptr_t client = QMM_VAR_REAL_RETURN(intptr_t);

        if (client >= 0)
            Core().OnBotAllocated(static_cast<int>(client));
    }
#else
    (void)cmd;
#endif

    QMM_RET_IGNORED(0);
}

C_DLLEXPORT void QMM_PluginMessage(plugin_id /*from_plid*/, const char* /*message*/, void* /*buf*/, intptr_t /*buflen*/,
                                   int /*is_broadcast*/)
{
}
