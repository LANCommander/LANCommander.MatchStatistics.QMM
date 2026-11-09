// Every QMM game without a module of its own: the generic layer only. Players
// join and leave, rename and chat, and a match runs from its start to the end
// of the map. Nothing is read from the game's structures, so there are no
// kills, scores or standings.

#include "../plugin/qmm_game.h"

namespace lc
{

namespace
{

class GenericGame final : public QmmGame
{
};

} // namespace

Game& TheGame()
{
    static GenericGame game;
    return game;
}

} // namespace lc
