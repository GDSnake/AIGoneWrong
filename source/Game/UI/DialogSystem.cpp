#include "DialogSystem.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <utility>

bool DialogSystem::init(const Renderer& renderer, TTF_Font* font)
{
    cleanup();
    if (!font)
    {
        return false;
    }
    try
    {
        _box = renderer.loadTexture("assets/Sprites/UI/dialogue_box_wide_640x160.png");
        _nameTab = renderer.loadTexture("assets/Sprites/UI/name_tab_left_160x40.png");
        _arrow = renderer.loadTexture("assets/Sprites/UI/continue_arrow_32x32.png");
        renderer.configureTexture(_box.get());
        renderer.configureTexture(_nameTab.get());
        renderer.configureTexture(_arrow.get());
        _font = font;
        return true;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Dialog UI initialization failed: " << error.what() << '\n';
        cleanup();
        return false;
    }
}

void DialogSystem::show(std::vector<DialogLine> lines)
{
    _lines = std::move(lines);
    _currentLine = 0;
}

void DialogSystem::advance()
{
    if (isActive())
    {
        ++_currentLine;
        if (!isActive())
        {
            _lines.clear();
            _currentLine = 0;
        }
    }
}

bool DialogSystem::isActive() const noexcept
{
    return _currentLine < _lines.size();
}

void DialogSystem::render(const Renderer& renderer) const
{
    int width = 0;
    int height = 0;
    if (!isActive() || !_font || !_box || !renderer.getOutputSize(width, height) || width <= 64 || height <= 96)
    {
        return;
    }
    const float boxWidth = std::min({ 900.0f, width - 48.0f, (height - 96.0f) * 4.0f });
    const auto& line = _lines[_currentLine];
    constexpr float textScale = 1.5f;
    float textWidth = 0.0f;
    float textHeight = 0.0f;
    float boxHeight = boxWidth * 0.25f;
    if (renderer.measureTextWrapped(_font, line.text, std::max(1.0f, boxWidth - 80.0f) / textScale, textWidth, textHeight))
    {
        boxHeight = std::min(height - 96.0f, std::max(boxHeight, textHeight * textScale + 48.0f));
    }
    const SDL_FRect box{ (width - boxWidth) * 0.5f, height - 24.0f - boxHeight, boxWidth, boxHeight };
    renderer.drawTexture(_box.get(), box);
    if (!line.characterName.empty())
    {
        float nameWidth = 0.0f;
        float nameHeight = 0.0f;
        if (renderer.measureText(_font, line.characterName, nameWidth, nameHeight))
        {
            const float tabWidth = std::min(box.w - 24.0f, std::max(160.0f, nameWidth * 0.5f + 48.0f));
            const SDL_FRect tab{ box.x + 12.0f, box.y - 34.0f, tabWidth, 40.0f };
            renderer.drawTexture(_nameTab.get(), tab);
            const float nameInset = std::max(32.0f, tab.w * 0.125f);
            const float nameScale = std::min({ 0.5f, (tab.w - nameInset - 12.0f) / nameWidth, 24.0f / nameHeight });
            renderer.drawText(_font, line.characterName,
                { tab.x + nameInset, tab.y + (tab.h - nameHeight * nameScale) * 0.5f }, { 235, 241, 250, 255 }, nameScale);
        }
    }
    renderer.drawTextWrapped(_font, line.text, { box.x + 24.0f, box.y + 24.0f, box.w - 80.0f, box.h - 48.0f },
        { 235, 241, 250, 255 }, textScale);
    renderer.drawTexture(_arrow.get(), { box.x + box.w - 48.0f, box.y + box.h - 44.0f, 32.0f, 32.0f });
}

void DialogSystem::cleanup()
{
    _lines.clear();
    _currentLine = 0;
    _font = nullptr;
    _box.reset();
    _nameTab.reset();
    _arrow.reset();
}