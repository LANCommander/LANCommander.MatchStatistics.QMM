// The engine half of lc::Game, through QMM: cvars, the console, userinfo. Every
// game module derives from QmmGame and adds what it knows of its game's rules.

#pragma once

// The core's headers before the SDK's, whose macros (STEF2's Round, ...) would
// otherwise rewrite them.
#include "../core/game.h"
#include "../core/stats.h"
#include "../core/text.h"

#include <algorithm>
#include <string>

#include "sdk.h"

namespace lc
{

class QmmGame : public Game
{
public:
    const char* Code() const override { return GAME_STR; }

    std::string Cvar(const char* name) override
    {
        const char* value = QMM_GETSTRCVAR(name);
        return value ? value : "";
    }

    void RegisterCvar(const char* name, const char* defaultValue) override
    {
        // NULL vmCvar_t: the engine only creates the cvar (keeping a value
        // server.cfg set) and the plugin reads it by name.
        QMM_SYSCALL(G_CVAR_REGISTER, nullptr, name, defaultValue, 0);
    }

    void Print(const std::string& text) override
    {
        QMM_WRITEQMMLOG(QMMLOG_INFO, "%s\n", text.c_str());
    }

    std::string Userinfo(int client) override
    {
        char buffer[MAX_INFO_STRING];
        buffer[0] = '\0';
#if defined(LC_FAMILY_Q2)
        // QMM's G_GET_USERINFO for these games takes the client's edict.
        if (!g_gents || g_gentsize <= 0)
            return {};

        QMM_SYSCALL(G_GET_USERINFO, ENT_FROM_NUM(client + 1), buffer, static_cast<int>(sizeof(buffer)));
#else
        QMM_SYSCALL(G_GET_USERINFO, client, buffer, static_cast<int>(sizeof(buffer)));
#endif
        buffer[sizeof(buffer) - 1] = '\0';
        return buffer;
    }

    int MaxClients() override
    {
        intptr_t count = QMM_GETINTCVAR("sv_maxclients");

        if (count <= 0)
            count = QMM_GETINTCVAR("maxclients");

        return static_cast<int>(std::clamp<intptr_t>(count, 1, kMaxClients));
    }

    std::string ModDir() override
    {
        const char* dir = QMM_MODDIR();
        return dir ? dir : "";
    }

protected:
    std::string ConfigString(int index)
    {
        const char* value = QMM_GETCONFIGSTRING2(index);
        return value ? value : "";
    }

    int IntCvar(const char* name)
    {
        return static_cast<int>(QMM_GETINTCVAR(name));
    }

    // The client's game structure, or null before the game has located it
    // (or past the clients it has).
    gclient_t* Client(int client)
    {
        if (!g_clients || g_clientsize <= 0 || client < 0 || client >= MaxClients())
            return nullptr;

        return CLIENT_FROM_NUM(client);
    }
};

} // namespace lc
