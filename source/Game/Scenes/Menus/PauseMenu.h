#pragma once

#include "SettingsMenu.h"
#include "Tutorial.h"

enum class PauseAction
{
    None,
    Resume,
    MainMenu,
    Exit,
#ifdef DEBUG
    DebugLevel1,
    DebugLevel2,
    DebugRoom,
#endif
};

class PauseMenu : public Tutorial
{
public:
    PauseMenu();
    void handleEvents(const SDL_Event& event) override;
    void handleInput() override;
    void update(float deltaTime) override;
    void render() override;
    void cleanup() override;
    PauseAction consumeAction() noexcept;

private:
    void setup() override;
    void activate(std::size_t index) override;
    void adjust(std::size_t index, int direction) override;
    void back() override;
    SDL_FRect itemBounds(std::size_t index) const override;
    SDL_FRect contentBounds(int width, int height) const override;

    PauseAction _action = PauseAction::None;
    std::unique_ptr<SettingsMenu> _settings;
};