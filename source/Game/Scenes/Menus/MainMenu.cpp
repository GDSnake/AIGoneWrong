#include "MainMenu.h"

#include "Credits.h"
#include "SettingsMenu.h"
#include "Tutorial.h"
#include "AssetManagers/AudioManager.h"

MainMenu::MainMenu() : MenuScene("h-AI-lucinations", "MainMenu") {}

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
        case 0: transitionTo(std::make_unique<Tutorial>()); break;
        case 1: transitionTo(std::make_unique<SettingsMenu>()); break;
        case 2: transitionTo(std::make_unique<Credits>()); break;
        case 3: requestExit(); break;
    }
}