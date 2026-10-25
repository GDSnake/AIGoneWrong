#include "GameOverMenu.h"

#include <utility>

GameOverMenu::GameOverMenu() : MenuScene("Game Over", "GameOver")
{
    _isOverlay = true;
}

void GameOverMenu::setup()
{
    _items = { { "Restart Level" }, { "Main Menu" } };
    _action = GameOverAction::None;
}

void GameOverMenu::activate(std::size_t index)
{
    _action = index == 0 ? GameOverAction::Restart : GameOverAction::MainMenu;
}

void GameOverMenu::back()
{
    _action = GameOverAction::MainMenu;
}

GameOverAction GameOverMenu::consumeAction() noexcept
{
    return std::exchange(_action, GameOverAction::None);
}