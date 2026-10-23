#include "Renderer.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_video.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <memory>
#include <stdexcept>

#include "Config/Config.h"

void Renderer::TextureDeleter::operator()(SDL_Texture* texture) const noexcept
{
    SDL_DestroyTexture(texture);
}

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::init()
{
    if (_window && _renderer)
    {
        return true;
    }

    const ConfigData& config = Config::getInstance().getConfigData();
    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | (config.fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
    if (!SDL_CreateWindowAndRenderer(config.title.c_str(), config.width, config.height, flags, &_window, &_renderer))
    {
        SDL_Log("Window/renderer creation failed: %s", SDL_GetError());
        shutdown();
        return false;
    }
    return true;
}

void Renderer::clear() const
{
    if (_renderer)
    {
        SDL_RenderClear(_renderer);
    }
}

void Renderer::present() const
{
    if (_renderer)
    {
        SDL_RenderPresent(_renderer);
    }
}

void Renderer::setDrawColor(const SDL_Color& color) const
{
    if (_renderer)
    {
        SDL_SetRenderDrawColor(_renderer, color.r, color.g, color.b, color.a);
    }
}

Renderer::Texture Renderer::loadTexture(const std::filesystem::path& path) const
{
    if (!_renderer)
    {
        throw std::runtime_error("Cannot load a texture before the renderer is initialized");
    }
    Texture texture(IMG_LoadTexture(_renderer, path.string().c_str()));
    if (!texture)
    {
        throw std::runtime_error("Cannot load texture '" + path.string() + "': " + SDL_GetError());
    }
    configureTexture(texture.get());
    return texture;
}

void Renderer::configureTexture(SDL_Texture* texture, SDL_ScaleMode scaleMode, SDL_BlendMode blendMode) const
{
    if (texture)
    {
        SDL_SetTextureScaleMode(texture, scaleMode);
        SDL_SetTextureBlendMode(texture, blendMode);
    }
}

bool Renderer::getTextureSize(SDL_Texture* texture, float& width, float& height) const
{
    return texture && SDL_GetTextureSize(texture, &width, &height);
}

void Renderer::drawTexture(SDL_Texture* texture, const SDL_FRect& destination, const SDL_FRect* source,
    float opacity) const
{
    if (_renderer && texture)
    {
        SDL_SetTextureAlphaModFloat(texture, opacity);
        SDL_RenderTexture(_renderer, texture, source, &destination);
    }
}

void Renderer::drawTextureRotated(SDL_Texture* texture, const SDL_FRect& destination,
    const SDL_FRect* source, const double angle, const SDL_FlipMode flip, float opacity) const
{
    if (_renderer && texture)
    {
        SDL_SetTextureAlphaModFloat(texture, opacity);
        SDL_RenderTextureRotated(_renderer, texture, source, &destination, angle, nullptr, flip);
    }
}

void Renderer::drawLines(std::span<const SDL_FPoint> points, const SDL_Color& color) const
{
    if (!_renderer || points.size() < 2)
    {
        return;
    }

    SDL_Color previousColor{};
    if (!SDL_GetRenderDrawColor(_renderer, &previousColor.r, &previousColor.g, &previousColor.b, &previousColor.a))
    {
        return;
    }
    setDrawColor(color);
    SDL_RenderLines(_renderer, points.data(), static_cast<int>(points.size()));
    setDrawColor(previousColor);
}

void Renderer::drawText(TTF_Font* font, const std::string& text, const SDL_FPoint& origin, const SDL_Color& color) const
{
    if (!_renderer || !font || text.empty())
    {
        return;
    }

    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
        TTF_RenderText_Blended(font, text.c_str(), text.size(), color), SDL_DestroySurface);
    if (!surface)
    {
        return;
    }

    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture(
        SDL_CreateTextureFromSurface(_renderer, surface.get()), SDL_DestroyTexture);
    if (!texture)
    {
        return;
    }

    float width = 0.0f;
    float height = 0.0f;
    if (SDL_GetTextureSize(texture.get(), &width, &height))
    {
        const SDL_FRect destination{origin.x, origin.y, width, height};
        SDL_RenderTexture(_renderer, texture.get(), nullptr, &destination);
    }
}

bool Renderer::getOutputSize(int& width, int& height) const
{
    return _renderer && SDL_GetRenderOutputSize(_renderer, &width, &height);
}

SDL_Window* Renderer::getWindow() const noexcept
{
    return _window;
}

SDL_Renderer* Renderer::getRenderer() const noexcept
{
    return _renderer;
}

bool Renderer::applyWindowSettings() const
{
    if (!_window)
    {
        return false;
    }

    const ConfigData& config = Config::getInstance().getConfigData();
    if (!SDL_SetWindowFullscreen(_window, config.fullscreen))
    {
        return false;
    }

    return config.fullscreen || SDL_SetWindowSize(_window, config.width, config.height);
}

void Renderer::shutdown()
{
    if (_renderer)
    {
        SDL_DestroyRenderer(_renderer);
        _renderer = nullptr;
    }
    if (_window)
    {
        SDL_DestroyWindow(_window);
        _window = nullptr;
    }
}

#if DEBUG_BUILD
void Renderer::writeFPSOnWindowTitle(Uint64 fps)
{
    std::string windowTitle = std::format("{} - FPS: {}", Config::getInstance().getConfigData().title, fps);
    SDL_SetWindowTitle(_window, windowTitle.c_str());
}
#endif
