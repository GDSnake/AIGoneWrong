#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Core/Renderer.h"

struct TTF_Font;

struct DialogLine
{
    std::string characterName;
    std::string text;
};

class DialogSystem
{
public:
    bool init(const Renderer& renderer, TTF_Font* font);
    void show(std::vector<DialogLine> lines);
    void advance();
    bool isActive() const noexcept;
    void render(const Renderer& renderer) const;
    void cleanup();

private:
    std::vector<DialogLine> _lines;
    std::size_t _currentLine = 0;
    TTF_Font* _font = nullptr;
    Renderer::Texture _box;
    Renderer::Texture _nameTab;
    Renderer::Texture _arrow;
};