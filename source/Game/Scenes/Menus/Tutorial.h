#pragma once

#include "MenuScene.h"
#include "Core/Renderer.h"

class Tutorial : public MenuScene
{
public:
    Tutorial(std::string title = "Controls", std::string sceneName = "Tutorial");
    void cleanup() override;

protected:
    void setup() override;
    void activate(std::size_t index) override;
    void back() override;
    void renderContent(const SDL_FRect& bounds) const override;
    std::string _escapeLabel = "Pause game";

private:
    void drawKey(const SDL_FRect& source, const SDL_FPoint& origin, float height) const;

    Renderer::Texture _keyboard;
    Renderer::Texture _arrows;
};