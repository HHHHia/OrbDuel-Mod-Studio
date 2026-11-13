#include "orbduel/game/Game.hpp"
namespace orbduel {
int runStudio(const Options &);
Game::Game() = default;
Game::~Game() = default;
int Game::run(const Options &options) {
    return runStudio(options);
}
} // namespace orbduel
