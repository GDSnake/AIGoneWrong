#include "Tutorial.h"

#include <algorithm>
#include <exception>
#include <utility>

#include "MainMenu.h"
#include "Game/Scenes/GameScenes/Room.h"

namespace
{
    constexpr SDL_FRect KeyA{ 10.0f, 10.0f, 27.0f, 29.0f };
    constexpr SDL_FRect KeyD{ 91.0f, 10.0f, 27.0f, 29.0f };
    constexpr SDL_FRect KeyS{ 145.0f, 38.0f, 27.0f, 29.0f };
    constexpr SDL_FRect KeyW{ 253.0f, 38.0f, 27.0f, 29.0f };
    constexpr SDL_FRect KeySpace{ 280.0f, 70.0f, 80.0f, 30.0f };
    constexpr SDL_FRect KeyEscape{ 10.0f, 102.0f, 28.0f, 31.0f };
    constexpr SDL_FRect ArrowKeys{ 0.0f, 0.0f, 82.0f, 56.0f };
    constexpr SDL_Color Text{ 236, 239, 235, 255 };
    constexpr SDL_Color Muted{ 152, 158, 155, 255 };
}

Tutorial::Tutorial(std::string title, std::string sceneName) : MenuScene(std::move(title), std::move(sceneName)) {}

void Tutorial::setup()
{
    _items = { { "Continue" }, { "Back" } };
    _itemsAtBottom = true;
    try
    {
        _keyboard = Renderer::getInstance().loadTexture("assets/Sprites/Controllers/keyboard_grey.png");
        Renderer::getInstance().configureTexture(_keyboard.get());
        _arrows = Renderer::getInstance().loadTexture("assets/Sprites/Controllers/keyboard_arrows.png");
        Renderer::getInstance().configureTexture(_arrows.get());
    }
    catch (const std::exception&)
    {
        setStatus("Keyboard artwork unavailable.", true);
    }
}

void Tutorial::activate(std::size_t index)
{
    if (index == 0)
    {
        transitionTo(std::make_unique<Room>());
    }
    else
    {
        back();
    }
}

void Tutorial::back()
{
    transitionTo(std::make_unique<MainMenu>());
}

void Tutorial::drawKey(const SDL_FRect& source, const SDL_FPoint& origin, float height) const
{
    if (_keyboard)
    {
        Renderer::getInstance().drawTexture(_keyboard.get(),
            { origin.x, origin.y, source.w * height / source.h, height }, &source);
    }
}

void Tutorial::renderContent(const SDL_FRect& bounds) const
{
    const float width = std::min(620.0f, bounds.w);
    const float rowHeight = std::min(100.0f, bounds.h / 3.0f);
    const float keyHeight = std::clamp((rowHeight - 8.0f) * 0.5f, 16.0f, 32.0f);
    const float labelX = bounds.x + std::min(280.0f, width * 0.6f);
    const float labelWidth = std::max(1.0f, bounds.x + width - labelX);
    const float keysY = bounds.y + (rowHeight - keyHeight * 2.0f - 4.0f) * 0.5f;
    const float keyStep = keyHeight + 4.0f;
    drawKey(KeyW, { bounds.x + keyStep, keysY }, keyHeight);
    drawKey(KeyA, { bounds.x, keysY + keyStep }, keyHeight);
    drawKey(KeyS, { bounds.x + keyStep, keysY + keyStep }, keyHeight);
    drawKey(KeyD, { bounds.x + keyStep * 2.0f, keysY + keyStep }, keyHeight);
    if (_arrows)
    {
        const float clusterHeight = keyHeight * 2.0f + 4.0f;
        Renderer::getInstance().drawTexture(_arrows.get(),
            { bounds.x + keyStep * 3.0f + 16.0f, keysY, ArrowKeys.w * clusterHeight / ArrowKeys.h, clusterHeight }, &ArrowKeys);
    }
    drawLabel("Move", { labelX, bounds.y, labelWidth, rowHeight * 0.5f }, Text, 0.8f);
    drawLabel("WASD / arrow keys", { labelX, bounds.y + rowHeight * 0.5f, labelWidth, rowHeight * 0.5f }, Muted, 0.55f);

    const float spaceY = bounds.y + rowHeight + (rowHeight - keyHeight) * 0.5f;
    drawKey(KeySpace, { bounds.x, spaceY }, keyHeight);
    drawLabel("Advance dialog", { labelX, bounds.y + rowHeight, labelWidth, rowHeight }, Text, 0.8f);

    const float escapeY = bounds.y + rowHeight * 2.0f + (rowHeight - keyHeight) * 0.5f;
    drawKey(KeyEscape, { bounds.x, escapeY }, keyHeight);
    drawLabel(_escapeLabel, { labelX, bounds.y + rowHeight * 2.0f, labelWidth, rowHeight }, Text, 0.8f);
}

void Tutorial::cleanup()
{
    _keyboard.reset();
    _arrows.reset();
    MenuScene::cleanup();
}