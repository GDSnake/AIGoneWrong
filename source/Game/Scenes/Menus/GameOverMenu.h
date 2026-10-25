#pragma once

#include "MenuScene.h"

enum class GameOverAction
{
    None,
    Restart,
    MainMenu
};

class GameOverMenu : public MenuScene
{
public:
    GameOverMenu();
    GameOverAction consumeAction() noexcept;

private:
    void setup() override;
    void activate(std::size_t index) override;
    void back() override;

    GameOverAction _action = GameOverAction::None;
};