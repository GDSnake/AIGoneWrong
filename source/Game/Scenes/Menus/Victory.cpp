#include "Victory.h"

#include <algorithm>
#include <exception>

#include "AssetManagers/AudioManager.h"
#include "AssetManagers/FontsManager.h"
#include "MainMenu.h"

Victory::Victory() : MenuScene("Congratulations", "Victory") {}

void Victory::setup()
{
    _items = { { "Main Menu" } };
    _itemsAtBottom = true;
    try
    {
        _artwork = Renderer::getInstance().loadTexture("assets/Sprites/UI/victory-robots.png");
        Renderer::getInstance().configureTexture(_artwork.get());
    }
    catch (const std::exception&)
    {
        setStatus("Victory artwork unavailable.", true);
    }
}

void Victory::activate(std::size_t)
{
    back();
}

void Victory::back()
{
    transitionTo(std::make_unique<MainMenu>());
}

void Victory::drawBackdrop(int, int) const
{
    auto& renderer = Renderer::getInstance();
    renderer.setDrawColor({ 21, 23, 24, 255 });
    renderer.clear();
}

void Victory::renderContent(const SDL_FRect& bounds) const
{
    auto& renderer = Renderer::getInstance();
    const float artHeight = std::min({ 240.0f, bounds.h * 0.55f, bounds.w * 0.3f });
    if (_artwork)
    {
        const float artWidth = artHeight / 0.3f;
        renderer.drawTexture(_artwork.get(),
            { bounds.x + (bounds.w - artWidth) * 0.5f, bounds.y, artWidth, artHeight });
    }
    const float textTop = bounds.y + artHeight + 16.0f;
    const float paragraphHeight = std::max(1.0f, (bounds.h - artHeight - 24.0f) * 0.5f);
    auto* font = FontsManager::getInstance().getFont(FontNames::DefaultFont);
    renderer.drawTextWrapped(font, "You've defeated the robots AI and their hallucinations...for now.",
        { bounds.x, textTop, bounds.w, paragraphHeight }, { 236, 239, 235, 255 }, 0.8f);
    renderer.drawTextWrapped(font, "So tomorrow is another day...get back to work!",
        { bounds.x, textTop + paragraphHeight + 8.0f, bounds.w, paragraphHeight },
        { 152, 158, 155, 255 }, 0.8f);
}

void Victory::cleanup()
{
    AudioManager::getInstance().stopMusic();
    _artwork.reset();
    MenuScene::cleanup();
}