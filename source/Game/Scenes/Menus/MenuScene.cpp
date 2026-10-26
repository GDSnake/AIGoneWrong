#include "MenuScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include "AssetManagers/FontsManager.h"
#include "AssetManagers/SpriteManager.h"
#include "Config/Config.h"
#include "Core/Renderer.h"
#include "Entities/Robot.h"
#include "Entities/Sprite.h"

namespace
{
    constexpr SDL_Color Text{ 236, 239, 235, 255 };
    constexpr SDL_Color Muted{ 152, 158, 155, 255 };
    constexpr SDL_Color Mint{ 137, 235, 188, 255 };
    constexpr SDL_Color Pink{ 241, 126, 183, 255 };
}

MenuScene::MenuScene(std::string title, std::string sceneName)
    : _title(std::move(title)), _sceneName(std::move(sceneName))
{
}

void MenuScene::init()
{
    _input = {};
    _selected = 0;
    _clicked.reset();
    _time = 0.0f;
    _font = FontsManager::getInstance().getFont(FontNames::DefaultFont);
    if (!_font)
    {
        _input.quit();
        return;
    }
    const auto& config = Config::getInstance();
    _scout = SpriteManager::getInstance().getSprite(config.getRobotProfile(RobotType::ScoutBot).spriteName);
    _trooper = SpriteManager::getInstance().getSprite(config.getRobotProfile(RobotType::TrooperBot).spriteName);
    for (const auto& sprite : { _scout, _trooper })
    {
        if (sprite)
        {
            Renderer::getInstance().configureTexture(sprite->getTexture().get());
        }
    }
    setup();
}

SDL_FRect MenuScene::itemBounds(std::size_t index) const
{
    int width = 0;
    int height = 0;
    Renderer::getInstance().getOutputSize(width, height);
    const float margin = width < 700 ? 24.0f : 64.0f;
    const float contentWidth = std::max(1.0f, std::min(440.0f, width - 2.0f * margin));
    const float count = static_cast<float>(std::max<std::size_t>(_items.size(), 1));
    const float bottom = height - margin - (_status.empty() ? 0.0f : 36.0f);
    float top = margin + 110.0f;
    float rowHeight = std::clamp((bottom - top - (count - 1.0f) * 12.0f) / count, 24.0f, 52.0f);
    const float totalHeight = rowHeight * count + (count - 1.0f) * 12.0f;
    if (!_details.empty() || _itemsAtBottom)
    {
        top = bottom - totalHeight;
    }
    else
    {
        top = std::max(top, (height - totalHeight) * 0.5f);
    }
    return { margin, top + static_cast<float>(index) * (rowHeight + 12.0f), contentWidth, rowHeight };
}

std::optional<std::size_t> MenuScene::hitTest(const SDL_FPoint& point) const
{
    for (std::size_t index = 0; index < _items.size(); ++index)
    {
        const SDL_FRect bounds = itemBounds(index);
        if (point.x >= bounds.x && point.x < bounds.x + bounds.w &&
            point.y >= bounds.y && point.y < bounds.y + bounds.h)
        {
            return index;
        }
    }
    return {};
}

SDL_FRect MenuScene::contentBounds(int width, int) const
{
    const float margin = width < 700 ? 24.0f : 64.0f;
    const float top = margin + 110.0f;
    return { margin, top, width - margin * 2.0f, std::max(1.0f, itemBounds(0).y - top - 24.0f) };
}

void MenuScene::selectItem(std::size_t index)
{
    if (!_items.empty())
    {
        _selected = index % _items.size();
    }
}

void MenuScene::resetInputState()
{
    _input = {};
    _clicked.reset();
}

void MenuScene::handleEvents(const SDL_Event& event)
{
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.repeat)
    {
        return;
    }
    _input.handleEvent(event);
    if (_detailsScrollable && event.type == SDL_EVENT_MOUSE_WHEEL)
    {
        scrollDetails(-event.wheel.y * 42.0f);
        return;
    }
    if (isTransitioning())
    {
        return;
    }
    if (event.type == SDL_EVENT_MOUSE_MOTION ||
        (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT))
    {
        const SDL_FPoint point = Renderer::getInstance().windowToRender({
            static_cast<float>(_input.getMouseX()), static_cast<float>(_input.getMouseY()) });
        if (const auto hit = hitTest(point))
        {
            _selected = *hit;
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                _clicked = *hit;
                const SDL_FRect bounds = itemBounds(*hit);
                _clickDirection = point.x >= bounds.x + bounds.w - 180.0f && point.x < bounds.x + bounds.w - 144.0f ? -1 : 1;
            }
        }
    }
}

void MenuScene::handleInput()
{
    if (isTransitioning() || _items.empty())
    {
        return;
    }
    if (_input.isKeyPressed(SDL_SCANCODE_ESCAPE))
    {
        back();
        return;
    }
    if (_detailsScrollable)
    {
        if (_input.isKeyPressed(SDL_SCANCODE_PAGEUP))
        {
            scrollDetails(-180.0f);
        }
        else if (_input.isKeyPressed(SDL_SCANCODE_PAGEDOWN))
        {
            scrollDetails(180.0f);
        }
    }
    const bool previous = _input.isKeyPressed(SDL_SCANCODE_UP) || _input.isKeyPressed(SDL_SCANCODE_W);
    const bool next = _input.isKeyPressed(SDL_SCANCODE_DOWN) || _input.isKeyPressed(SDL_SCANCODE_S) || _input.isKeyPressed(SDL_SCANCODE_TAB);
    if (previous)
    {
        _selected = (_selected + _items.size() - 1) % _items.size();
    }
    else if (next)
    {
        _selected = (_selected + 1) % _items.size();
    }
    if (_clicked)
    {
        const std::size_t index = *_clicked;
        _clicked.reset();
        if (_items[index].kind == ItemKind::Choice)
        {
            adjust(index, _clickDirection);
        }
        else
        {
            activate(index);
        }
        return;
    }
    if (_input.isKeyPressed(SDL_SCANCODE_LEFT) || _input.isKeyPressed(SDL_SCANCODE_A))
    {
        adjust(_selected, -1);
    }
    else if (_input.isKeyPressed(SDL_SCANCODE_RIGHT) || _input.isKeyPressed(SDL_SCANCODE_D))
    {
        adjust(_selected, 1);
    }
    else if (_input.isKeyPressed(SDL_SCANCODE_RETURN) || _input.isKeyPressed(SDL_SCANCODE_KP_ENTER) ||
        _input.isKeyPressed(SDL_SCANCODE_SPACE))
    {
        activate(_selected);
    }
}

void MenuScene::update(float deltaTime)
{
    if (std::isfinite(deltaTime) && deltaTime > 0.0f)
    {
        _time += std::min(deltaTime, 0.25f);
    }
    _input.update();
}

void MenuScene::drawLabel(const std::string& text, const SDL_FRect& bounds, const SDL_Color& color, float scale) const
{
    float width = 0.0f;
    float height = 0.0f;
    auto& renderer = Renderer::getInstance();
    if (renderer.measureText(_font, text, width, height) && width > 0.0f && height > 0.0f)
    {
        scale = std::min({ scale, bounds.w / width, bounds.h / height });
        renderer.drawText(_font, text, { bounds.x, bounds.y + (bounds.h - height * scale) * 0.5f }, color, scale);
    }
}

void MenuScene::drawBackdrop(int width, int height) const
{
    auto& renderer = Renderer::getInstance();
    renderer.setDrawColor({ 21, 23, 24, 255 });
    renderer.clear();
    for (float row = height * 0.55f; row < height; row += 32.0f)
    {
        const std::array<SDL_FPoint, 2> line{ { { 0.0f, row }, { static_cast<float>(width), row } } };
        renderer.drawLines(line, { 31, 35, 34, 255 });
    }
    if (width >= 1000)
    {
        const auto frame = static_cast<std::size_t>(_time / 0.12f) % 8;
        if (_trooper)
        {
            const auto& source = _trooper->getSourceRect(RobotIdleRows[2] * 8 + frame);
            renderer.drawTexture(_trooper->getTexture().get(), { width - 430.0f, height * 0.5f - 125.0f, 256.0f, 256.0f }, &source, 0.75f);
        }
        if (_scout)
        {
            const auto& source = _scout->getSourceRect(RobotIdleRows[2] * 8 + frame);
            renderer.drawTexture(_scout->getTexture().get(), { width - 370.0f, height * 0.5f - 45.0f, 320.0f, 320.0f }, &source);
        }
    }
}

void MenuScene::render()
{
    auto& renderer = Renderer::getInstance();
    int width = 0;
    int height = 0;
    if (!renderer.getOutputSize(width, height) || width <= 0 || height <= 0)
    {
        return;
    }
    if (_isOverlay)
    {
        renderer.drawOverlay({ 12, 15, 14, 225 });
    }
    else
    {
        drawBackdrop(width, height);
    }
    const float margin = width < 700 ? 24.0f : 64.0f;
    drawLabel(_title, { margin, margin, width - margin * 2.0f, 68.0f }, Text, 1.65f);
    renderer.drawFilledRect({ margin, margin + 84.0f, 64.0f, 3.0f }, Pink);
    if (_itemsAtBottom && !_items.empty())
    {
        renderContent(contentBounds(width, height));
    }
    for (std::size_t index = 0; index < _items.size(); ++index)
    {
        const auto& item = _items[index];
        const auto bounds = itemBounds(index);
        const bool selected = index == _selected;
        renderer.drawFilledRect(bounds, selected ? SDL_Color{ 137, 235, 188, 255 } : SDL_Color{ 35, 40, 38, 255 });
        const SDL_Color color = selected ? SDL_Color{ 20, 33, 27, 255 } : Text;
        const float labelWidth = item.kind == ItemKind::Choice ? bounds.w - 204.0f : bounds.w - 64.0f;
        drawLabel(item.label, { bounds.x + 20.0f, bounds.y + 6.0f, labelWidth, bounds.h - 12.0f }, color, 0.85f);
        if (item.kind == ItemKind::Choice)
        {
            drawLabel(value(index), { bounds.x + bounds.w - 144.0f, bounds.y + 6.0f, 108.0f, bounds.h - 12.0f }, color, 0.65f);
            for (const int direction : { -1, 1 })
            {
                const float centerX = bounds.x + bounds.w - (direction < 0 ? 164.0f : 18.0f);
                const float centerY = bounds.y + bounds.h * 0.5f;
                const float tip = direction * 5.0f;
                const std::array<SDL_FPoint, 3> arrow{ { { centerX - tip, centerY - 6.0f }, { centerX + tip, centerY }, { centerX - tip, centerY + 6.0f } } };
                renderer.drawLines(arrow, color);
            }
        }
        else if (item.kind == ItemKind::Toggle)
        {
            const float boxX = bounds.x + bounds.w - 42.0f;
            const float boxY = bounds.y + (bounds.h - 22.0f) * 0.5f;
            const std::array<SDL_FPoint, 5> box{ { { boxX, boxY }, { boxX + 22.0f, boxY }, { boxX + 22.0f, boxY + 22.0f }, { boxX, boxY + 22.0f }, { boxX, boxY } } };
            renderer.drawLines(box, color);
            if (value(index) == "on")
            {
                const std::array<SDL_FPoint, 3> check{ { { boxX + 4.0f, boxY + 11.0f }, { boxX + 9.0f, boxY + 17.0f }, { boxX + 18.0f, boxY + 5.0f } } };
                renderer.drawLines(check, color);
            }
        }
    }
    if (!_details.empty() && !_items.empty())
    {
        const float top = margin + 110.0f;
        const float viewportHeight = std::max(1.0f, itemBounds(0).y - top - 20.0f);
        const float step = _detailsScrollable ? 42.0f : std::min(34.0f, viewportHeight / _details.size());
        const float maxScroll = std::max(0.0f, step * _details.size() - viewportHeight);
        const float scroll = std::clamp(_detailsScroll, 0.0f, maxScroll);
        SDL_Rect clip{ static_cast<int>(margin), static_cast<int>(top),
            static_cast<int>(std::max(1.0f, width - margin * 2.0f)), static_cast<int>(viewportHeight) };
        if (_detailsScrollable)
        {
            SDL_SetRenderClipRect(renderer.getRenderer(), &clip);
        }
        for (std::size_t index = 0; index < _details.size(); ++index)
        {
            drawLabel(_details[index], { margin, top + step * index - scroll,
                std::max(1.0f, std::min(560.0f, width - margin * 2.0f)), step }, Muted,
                _detailsScrollable ? (index == 0 ? 0.88f : 0.8f) : 0.7f);
        }
        if (_detailsScrollable)
        {
            SDL_SetRenderClipRect(renderer.getRenderer(), nullptr);
        }
    }
    if (!_status.empty() && !_items.empty())
    {
        const auto last = itemBounds(_items.size() - 1);
        drawLabel(_status, { margin, last.y + last.h + 12.0f, width - margin * 2.0f, 26.0f }, _statusError ? Pink : Mint, 0.65f);
    }
}

void MenuScene::cleanup()
{
    _clicked.reset();
    _scout.reset();
    _trooper.reset();
    _font = nullptr;
}

void MenuScene::scrollDetails(float amount)
{
    if (_detailsScrollable && std::isfinite(amount))
    {
        _detailsScroll += amount;
    }
}

std::string MenuScene::getName() const { return _sceneName; }
bool MenuScene::shouldQuit() const { return _input.shouldQuit(); }
void MenuScene::adjust(std::size_t, int) {}
std::string MenuScene::value(std::size_t) const { return {}; }
void MenuScene::back() { requestExit(); }
void MenuScene::renderContent(const SDL_FRect&) const {}
void MenuScene::setStatus(std::string message, bool error)
{
    _status = std::move(message);
    _statusError = error;
}