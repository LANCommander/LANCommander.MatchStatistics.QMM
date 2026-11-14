// The game's SDK, picked by the GAME_<CODE> define each build passes (see
// CMakeLists.txt), as stub_qmm's include/game.h picks it. Only src/plugin/
// and src/games/ include this; the core never sees an SDK.
//
// And how each engine family calls the game, from the SDKs' game_export_t:
//
//   LC_FAMILY_Q3   vmMain games. Client numbers are ints; ClientConnect(client,
//                  firstTime, isBot); RunFrame(levelTime); Shutdown(restart).
//                  G_LogPrintf echoes the log lines to the console.
//   LC_FAMILY_UBER Ritual's Übertools games (MOHAA and its expansions, Elite
//                  Force II): ClientConnect(int client, ...), the other client
//                  calls take the gentity_t* (entity number = client number);
//                  RunFrame(levelTime, frameTime); Shutdown(void).
//   LC_FAMILY_Q2   Quake II and its descendants: every client call takes the
//                  edict_t* (entity number = client number + 1);
//                  ClientConnect(ent, userinfo); RunFrame(void); Shutdown(void).
//
// LC_HAS_BOTLIB marks the games with Q3's botlib (G_BOT_ALLOCATE_CLIENT),
// whose mods connect bots themselves, out of QMM's sight (main.cpp).
//
// LC_LOG_FROM_FILE marks the games whose G_LogPrintf writes only to the log
// file (Call of Duty: a dedicated 1.5 server echoes nothing to the console).
// The plugin reads the lines from the mod's G_FS_WRITE calls on g_log's file.
//
// Single-player games (JASP, JK2SP, RTCWSP, SOF2SP, STVOYSP) are not built:
// a match there means nothing to LANCommander.

#pragma once

#include <qmmapi.h>

#if defined(GAME_COD11MP)
    #include <cod11mp/bgame/bg_local.h>
    #include <cod11mp/bgame/bg_public.h>
    #include <cod11mp/game/g_public.h>
    #include <game_cod11mp.h>
    #define LC_FAMILY_Q3
    #define LC_LOG_FROM_FILE
#elif defined(GAME_CODMP)
    #include <codmp/bgame/bg_local.h>
    #include <codmp/bgame/bg_public.h>
    #include <codmp/game/g_public.h>
    #include <game_codmp.h>
    #define LC_FAMILY_Q3
    #define LC_LOG_FROM_FILE
#elif defined(GAME_CODUOMP)
    #include <coduomp/bgame/bg_local.h>
    #include <coduomp/bgame/bg_public.h>
    #include <coduomp/game/g_public.h>
    #include <game_coduomp.h>
    #define LC_FAMILY_Q3
    #define LC_LOG_FROM_FILE
#elif defined(GAME_JAMP)
    #include <jamp/game/g_local.h>
    #include <game_jamp.h>
    #define LC_FAMILY_Q3
    #define LC_HAS_BOTLIB
#elif defined(GAME_JK2MP)
    #include <jk2mp/game/g_local.h>
    #include <game_jk2mp.h>
    #define LC_FAMILY_Q3
    #define LC_HAS_BOTLIB
#elif defined(GAME_KINGPIN)
    #include <kingpin/q_shared.h>
    #include <kingpin/game.h>
    #include <game_kingpin.h>
    #define LC_FAMILY_Q2
#elif defined(GAME_MOHAA)
    #include <mohaa/fgame/g_local.h>
    #include <mohaa/fgame/bg_public.h>
    #include <game_mohaa.h>
    #define LC_FAMILY_UBER
#elif defined(GAME_MOHBT)
    #include <mohaa/fgame/g_local.h>
    #include <mohaa/fgame/bg_public.h>
    #include <game_mohbt.h>
    #define LC_FAMILY_UBER
#elif defined(GAME_MOHSH)
    #include <mohsh/fgame/g_local.h>
    #include <mohsh/fgame/bg_public.h>
    #include <game_mohsh.h>
    #define LC_FAMILY_UBER
#elif defined(GAME_QUAKE2)
    #include <quake2/game/g_local.h>
    #include <game_quake2.h>
    #define LC_FAMILY_Q2
#elif defined(GAME_Q2R)
    #include <q2r/rerelease/g_local.h>
    #include <game_q2r.h>
    #define LC_FAMILY_Q2
    #define LC_CONNECT_ISBOT_ARG 3
#elif defined(GAME_Q3A)
    #include <q3a/game/g_local.h>
    #include <game_q3a.h>
    #define LC_FAMILY_Q3
    #define LC_HAS_BOTLIB
#elif defined(GAME_RTCWMP)
    #include <rtcwmp/game/g_local.h>
    #include <game_rtcwmp.h>
    #define LC_FAMILY_Q3
    #define LC_HAS_BOTLIB
#elif defined(GAME_SIN)
    #include <sin/game/g_local.h>
    #include <game_sin.h>
    #define LC_FAMILY_Q2
#elif defined(GAME_SOF)
    #include <sof/gamecpp/g_local.h>
    #include <game_sof.h>
    #define LC_FAMILY_Q2
#elif defined(GAME_SOF2MP)
    #include <sof2mp/game/g_local.h>
    #include <game_sof2mp.h>
    #define LC_FAMILY_Q3
    #define LC_HAS_BOTLIB
#elif defined(GAME_STEF2)
    #include <stef2/game/g_local.h>
    #include <stef2/game/bg_public.h>
    #include <game_stef2.h>
    #define LC_FAMILY_UBER
    #define LC_CONNECT_ISBOT_ARG 2
#elif defined(GAME_STVOYHM)
    #include <stvoyhm/game/g_local.h>
    #include <game_stvoyhm.h>
    #define LC_FAMILY_Q3
    #define LC_HAS_BOTLIB
#elif defined(GAME_WET)
    #include <wet/game/g_local.h>
    #include <game_wet.h>
    #define LC_FAMILY_Q3
    #define LC_HAS_BOTLIB
#else
    #error "Build with one GAME_<CODE> define (see CMakeLists.txt)"
#endif

#if defined(LC_FAMILY_Q3) && !defined(LC_CONNECT_ISBOT_ARG)
    #define LC_CONNECT_ISBOT_ARG 2
#endif

// The entity and client arrays, from G_LOCATE_GAME_DATA (src/plugin/main.cpp),
// under the names qmmapi.h's ENT_FROM_NUM/CLIENT_FROM_NUM expect.
extern gentity_t* g_gents;
extern intptr_t   g_numgents;
extern intptr_t   g_gentsize;
extern gclient_t* g_clients;
extern intptr_t   g_clientsize;

namespace lc
{
class Game;

// The game module this build links (src/games/<module>.cpp).
Game& TheGame();
}
