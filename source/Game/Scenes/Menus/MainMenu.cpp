#include "MainMenu.h"

#include "Credits.h"
#include "SettingsMenu.h"
#include "AssetManagers/AudioManager.h"
#include "Game/Scenes/GameScenes/Room.h"

MainMenu::MainMenu() : MenuScene("AI GONE WRONG", "MainMenu") {}

void MainMenu::setup()
{
    _items = { { "Start Game" }, { "Settings" }, { "Credits" }, { "Exit" } };
    AudioManager::getInstance().playMusic();
}

void MainMenu::cleanup()
{
    AudioManager::getInstance().stopMusic();
    MenuScene::cleanup();
}

void MainMenu::activate(std::size_t index)
{
    switch (index)
    {
        case 0: transitionTo(std::make_unique<Room>()); break;
        case 1: transitionTo(std::make_unique<SettingsMenu>()); break;
        case 2: transitionTo(std::make_unique<Credits>()); break;
        case 3: requestExit(); break;
    }
}