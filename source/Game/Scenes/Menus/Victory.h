#pragma once

#include "MenuScene.h"
#include "Core/Renderer.h"

class Victory : public MenuScene
{
public:
    Victory();
    void cleanup() override;

protected:
    void setup() override;
    void activate(std::size_t index) override;
    void back() override;
    void drawBackdrop(int width, int height) const override;
    void renderContent(const SDL_FRect& bounds) const override;

private:
    Renderer::Texture _artwork;
};