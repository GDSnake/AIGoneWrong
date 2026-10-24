#include "Renderer.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_video.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <memory>
#include <stdexcept>
#include <vector>

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

void Renderer::drawTriangleFan(std::span<const SDL_FPoint> points, const SDL_Color& color) const
{
    if (!_renderer || points.size() < 3 || color.a == 0)
    {
        return;
    }
    SDL_BlendMode previousBlendMode;
    if (!SDL_GetRenderDrawBlendMode(_renderer, &previousBlendMode))
    {
        return;
    }
    const SDL_FColor vertexColor{ color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };
    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
    vertices.reserve(points.size());
    indices.reserve((points.size() - 2) * 3);
    for (const SDL_FPoint& point : points)
    {
        vertices.push_back({ point, vertexColor, { 0.0f, 0.0f } });
    }
    for (std::size_t index = 1; index + 1 < points.size(); ++index)
    {
        indices.insert(indices.end(), { 0, static_cast<int>(index), static_cast<int>(index + 1) });
    }
    SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(_renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()),
        indices.data(), static_cast<int>(indices.size()));
    SDL_SetRenderDrawBlendMode(_renderer, previousBlendMode);
}

void Renderer::drawOverlay(const SDL_Color& color) const
{
    if (!_renderer || color.a == 0)
    {
        return;
    }
    SDL_Color previousColor{};
    SDL_BlendMode previousBlendMode;
    if (!SDL_GetRenderDrawColor(_renderer, &previousColor.r, &previousColor.g, &previousColor.b, &previousColor.a) ||
        !SDL_GetRenderDrawBlendMode(_renderer, &previousBlendMode))
    {
        return;
    }
    SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
    setDrawColor(color);
    SDL_RenderFillRect(_renderer, nullptr);
    setDrawColor(previousColor);
    SDL_SetRenderDrawBlendMode(_renderer, previousBlendMode);
}

void Renderer::drawFilledRect(const SDL_FRect& rectangle, const SDL_Color& color) const
{
    if (!_renderer)
    {
        return;
    }
    SDL_Color previousColor{};
    SDL_BlendMode previousBlendMode = SDL_BLENDMODE_NONE;
    if (!SDL_GetRenderDrawColor(_renderer, &previousColor.r, &previousColor.g, &previousColor.b, &previousColor.a) ||
        !SDL_GetRenderDrawBlendMode(_renderer, &previousBlendMode))
    {
        return;
    }
    SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
    setDrawColor(color);
    SDL_RenderFillRect(_renderer, &rectangle);
    setDrawColor(previousColor);
    SDL_SetRenderDrawBlendMode(_renderer, previousBlendMode);
}

bool Renderer::measureText(TTF_Font* font, const std::string& text, float& width, float& height) const
{
    int measuredWidth = 0;
    int measuredHeight = 0;
    if (!font || !TTF_GetStringSize(font, text.c_str(), text.size(), &measuredWidth, &measuredHeight))
    {
        return false;
    }
    width = static_cast<float>(measuredWidth);
    height = static_cast<float>(measuredHeight);
    return true;
}

void Renderer::drawText(TTF_Font* font, const std::string& text, const SDL_FPoint& origin, const SDL_Color& color, float scale) const
{
    if (!_renderer || !font || text.empty() || !std::isfinite(scale) || scale <= 0.0f)
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
        const SDL_FRect destination{origin.x, origin.y, width * scale, height * scale};
        SDL_RenderTexture(_renderer, texture.get(), nullptr, &destination);
    }
}

bool Renderer::getOutputSize(int& width, int& height) const
{
    return _renderer && SDL_GetRenderOutputSize(_renderer, &width, &height);
}

SDL_FPoint Renderer::windowToRender(const SDL_FPoint& point) const
{
    SDL_FPoint result = point;
    if (_renderer)
    {
        SDL_RenderCoordinatesFromWindow(_renderer, point.x, point.y, &result.x, &result.y);
    }
    return result;
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
