#include "PauseMenu.h"

#include <algorithm>
#include <utility>

PauseMenu::PauseMenu() : Tutorial("Paused", "PauseMenu")
{
    _isOverlay = true;
    _escapeLabel = "Resume game";
}

void PauseMenu::setup()
{
    Tutorial::setup();
    _items = { { "Continue" }, { "Settings" }, { "Main Menu" }, { "Exit Game" } };
    _action = PauseAction::None;
    _settings.reset();
}

void PauseMenu::handleEvents(const SDL_Event& event)
{
    if (_settings)
    {
        _settings->handleEvents(event);
    }
    else
    {
        Tutorial::handleEvents(event);
    }
}

void PauseMenu::handleInput()
{
    if (_settings)
    {
        _settings->handleInput();
        if (_settings->isClosed())
        {
            _settings->cleanup();
            _settings.reset();
            resetInputState();
            selectItem(0);
        }
    }
    else
    {
        Tutorial::handleInput();
    }
}

void PauseMenu::update(float deltaTime)
{
    if (_settings)
    {
        _settings->update(deltaTime);
    }
    else
    {
        Tutorial::update(deltaTime);
    }
}

void PauseMenu::render()
{
    if (_settings)
    {
        _settings->render();
    }
    else
    {
        Tutorial::render();
    }
}

void PauseMenu::activate(std::size_t index)
{
    switch (index)
    {
        case 0: _action = PauseAction::Resume; break;
        case 1:
            _settings = std::make_unique<SettingsMenu>(true);
            _settings->init();
            break;
        case 2: _action = PauseAction::MainMenu; break;
        case 3: _action = PauseAction::Exit; break;
    }
}

void PauseMenu::adjust(std::size_t index, int direction)
{
    selectItem(direction < 0 ? (index + _items.size() - 1) % _items.size() : (index + 1) % _items.size());
}

void PauseMenu::back()
{
    _action = PauseAction::Resume;
}

PauseAction PauseMenu::consumeAction() noexcept
{
    return std::exchange(_action, PauseAction::None);
}

SDL_FRect PauseMenu::itemBounds(std::size_t index) const
{
    int width = 0;
    int height = 0;
    Renderer::getInstance().getOutputSize(width, height);
    const float margin = width < 700 ? 24.0f : 64.0f;
    if (width >= 1100)
    {
        const float buttonWidth = std::min(340.0f, width * 0.3f);
        const float top = std::max(margin + 110.0f, (height - 244.0f) * 0.5f);
        return { width - margin - buttonWidth, top + static_cast<float>(index) * 64.0f, buttonWidth, 52.0f };
    }
    const float buttonWidth = (width - margin * 2.0f - 12.0f) * 0.5f;
    return { margin + static_cast<float>(index % 2) * (buttonWidth + 12.0f),
        height - margin - 108.0f + static_cast<float>(index / 2) * 60.0f, buttonWidth, 48.0f };
}

SDL_FRect PauseMenu::contentBounds(int width, int height) const
{
    const float margin = width < 700 ? 24.0f : 64.0f;
    const float top = margin + 110.0f;
    if (width >= 1100)
    {
        return { margin, top, std::min(620.0f, itemBounds(0).x - margin - 40.0f), height - top - margin };
    }
    return { margin, top, width - margin * 2.0f, std::max(1.0f, itemBounds(0).y - top - 24.0f) };
}

void PauseMenu::cleanup()
{
    if (_settings)
    {
        _settings->cleanup();
        _settings.reset();
    }
    Tutorial::cleanup();
    _action = PauseAction::None;
}