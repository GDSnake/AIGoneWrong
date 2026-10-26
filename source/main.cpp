#include <iostream>

#include <SDL3/SDL_main.h>

#include "Config/Config.h"
#include "Game/Game.h"
#include "Game/Scenes/Menus/MainMenu.h"

int main(int argc, char* argv[]) {

    Config::getInstance().loadConfig();

    if (Config::getInstance().getConfigData().hasError)
    {
        std::cerr << "Configuration file has an error!" << '\n';
#ifdef DEBUG_BUILD
        std::cerr << "Press Enter to exit...";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
#endif
        return 1;
    }

    Game game(std::make_unique<MainMenu>());
    if (game.init())
    {
        game.run();
    }
    return 0;
}