#include "Renderer.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_properties.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <memory>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "Config/Config.h"

namespace
{
    constexpr const char* PixelDataProperty = "hAIlucinations.texture_pixels";

    struct TexturePixels
    {
        int width = 0;
        int height = 0;
        std::vector<Uint8> rgba;
        Renderer::Texture masked;
        int maskedWidth = 0;
        int maskedHeight = 0;
    };

    SDL_FPoint rotatePoint(SDL_FPoint point, const SDL_FRect& bounds, double angle)
    {
        const float centreX = bounds.x + bounds.w * 0.5f;
        const float centreY = bounds.y + bounds.h * 0.5f;
        const float radians = static_cast<float>(angle) * std::numbers::pi_v<float> / 180.0f;
        const float deltaX = point.x - centreX, deltaY = point.y - centreY;
        return { centreX + std::cos(radians) * deltaX - std::sin(radians) * deltaY,
            centreY + std::sin(radians) * deltaX + std::cos(radians) * deltaY };
    }

    SDL_FRect spriteBounds(const WorldSprite& sprite)
    {
        const auto& bounds = sprite.destination;
        if (sprite.angle == 0.0) return bounds;
        const std::array corners{ SDL_FPoint{ bounds.x, bounds.y }, SDL_FPoint{ bounds.x + bounds.w, bounds.y },
            SDL_FPoint{ bounds.x + bounds.w, bounds.y + bounds.h }, SDL_FPoint{ bounds.x, bounds.y + bounds.h } };
        float minX = INFINITY, minY = INFINITY, maxX = -INFINITY, maxY = -INFINITY;
        for (const auto corner : corners)
        {
            const auto point = rotatePoint(corner, bounds, sprite.angle);
            minX = std::min(minX, point.x);minY = std::min(minY, point.y);
            maxX = std::max(maxX, point.x);maxY = std::max(maxY, point.y);
        }
        return { minX, minY, maxX - minX, maxY - minY };
    }

    Uint8 spriteAlpha(const WorldSprite& sprite, const TexturePixels& pixels, SDL_FPoint point)
    {
        if (sprite.destination.w <= 0.0f || sprite.destination.h <= 0.0f) return 0;
        if (sprite.angle != 0.0) point = rotatePoint(point, sprite.destination, -sprite.angle);
        float unitX = (point.x - sprite.destination.x) / sprite.destination.w;
        float unitY = (point.y - sprite.destination.y) / sprite.destination.h;
        if (unitX < 0.0f || unitY < 0.0f || unitX >= 1.0f || unitY >= 1.0f) return 0;
        if (sprite.flip & SDL_FLIP_HORIZONTAL) unitX = 1.0f - unitX;
        if (sprite.flip & SDL_FLIP_VERTICAL) unitY = 1.0f - unitY;
        const int sourceX = std::clamp(static_cast<int>(sprite.source.x + unitX * sprite.source.w), 0, pixels.width - 1);
        const int sourceY = std::clamp(static_cast<int>(sprite.source.y + unitY * sprite.source.h), 0, pixels.height - 1);
        return static_cast<Uint8>(pixels.rgba[(static_cast<std::size_t>(sourceY) * pixels.width + sourceX) * 4 + 3] * sprite.opacity);
    }
}

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

    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> icon(
        IMG_Load("assets/Icons/game-icon.png"), SDL_DestroySurface);
    if (!icon || !SDL_SetWindowIcon(_window, icon.get()))
    {
        SDL_Log("Window icon could not be set: %s", SDL_GetError());
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
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> original(IMG_Load(path.string().c_str()), SDL_DestroySurface);
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
        original ? SDL_ConvertSurface(original.get(), SDL_PIXELFORMAT_RGBA32) : nullptr, SDL_DestroySurface);
    Texture texture(surface ? SDL_CreateTextureFromSurface(_renderer, surface.get()) : nullptr);
    if (!texture)
    {
        throw std::runtime_error("Cannot load texture '" + path.string() + "': " + SDL_GetError());
    }
    auto pixels = std::make_unique<TexturePixels>();
    pixels->width = surface->w;pixels->height = surface->h;
    pixels->rgba.resize(static_cast<std::size_t>(surface->w) * surface->h * 4);
    for (int row = 0; row < surface->h; ++row)
    {
        std::memcpy(pixels->rgba.data() + static_cast<std::size_t>(row) * surface->w * 4,
            static_cast<const Uint8*>(surface->pixels) + row * surface->pitch, surface->w * 4);
    }
    if (!SDL_SetPointerPropertyWithCleanup(SDL_GetTextureProperties(texture.get()), PixelDataProperty, pixels.get(),
        [](void*, void* value) { delete static_cast<TexturePixels*>(value); }, nullptr))
    {
        throw std::runtime_error("Cannot cache texture pixels: " + std::string(SDL_GetError()));
    }
    pixels.release();
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

void Renderer::drawWorldSprite(const WorldSprite& sprite, const RenderView& view,
    std::span<const WorldSprite> actorsBehind) const
{
    if (!_renderer || !sprite.texture || sprite.source.w <= 0.0f || sprite.source.h <= 0.0f)
    {
        return;
    }
    struct ActorMask
    {
        const WorldSprite* sprite;
        const TexturePixels* pixels;
    };
    std::vector<ActorMask> masks;
    const auto bounds = spriteBounds(sprite);
    for (const auto& actor : actorsBehind)
    {
        const auto actorBounds = spriteBounds(actor);
        if (!actor.texture || actor.depth >= sprite.depth || !SDL_HasRectIntersectionFloat(&bounds, &actorBounds)) continue;
        const auto* pixels = static_cast<TexturePixels*>(SDL_GetPointerProperty(
            SDL_GetTextureProperties(actor.texture), PixelDataProperty, nullptr));
        if (pixels) masks.push_back({ &actor, pixels });
    }
    SDL_Texture* texture = sprite.texture;
    SDL_FRect source = sprite.source;
    auto* pixels = static_cast<TexturePixels*>(SDL_GetPointerProperty(
        SDL_GetTextureProperties(sprite.texture), PixelDataProperty, nullptr));
    if (pixels && !masks.empty())
    {
        const int width = static_cast<int>(std::ceil(source.w));
        const int height = static_cast<int>(std::ceil(source.h));
        if (!pixels->masked || pixels->maskedWidth != width || pixels->maskedHeight != height)
        {
            pixels->masked.reset(SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height));
            pixels->maskedWidth = width;pixels->maskedHeight = height;
            configureTexture(pixels->masked.get());
        }
        void* buffer = nullptr;
        int pitch = 0;
        if (pixels->masked && SDL_LockTexture(pixels->masked.get(), nullptr, &buffer, &pitch))
        {
            for (int row = 0; row < height; ++row)
            {
                auto* output = static_cast<Uint8*>(buffer) + row * pitch;
                for (int column = 0; column < width; ++column)
                {
                    const int sourceX = std::clamp(static_cast<int>(source.x) + column, 0, pixels->width - 1);
                    const int sourceY = std::clamp(static_cast<int>(source.y) + row, 0, pixels->height - 1);
                    const auto* original = pixels->rgba.data() + (static_cast<std::size_t>(sourceY) * pixels->width + sourceX) * 4;
                    std::memcpy(output + column * 4, original, 4);
                    if (original[3] == 0) continue;
                    float unitX = (column + 0.5f) / width, unitY = (row + 0.5f) / height;
                    if (sprite.flip & SDL_FLIP_HORIZONTAL) unitX = 1.0f - unitX;
                    if (sprite.flip & SDL_FLIP_VERTICAL) unitY = 1.0f - unitY;
                    SDL_FPoint world{ sprite.destination.x + unitX * sprite.destination.w,
                        sprite.destination.y + unitY * sprite.destination.h };
                    if (sprite.angle != 0.0) world = rotatePoint(world, sprite.destination, sprite.angle);
                    Uint8 coverage = 0;
                    for (const auto& mask : masks)
                    {
                        coverage = std::max(coverage, spriteAlpha(*mask.sprite, *mask.pixels, world));
                    }
                    output[column * 4 + 3] = static_cast<Uint8>((static_cast<unsigned>(original[3]) * (510 - coverage) + 255) / 510);
                }
            }
            SDL_UnlockTexture(pixels->masked.get());
            texture = pixels->masked.get();
            source = { 0, 0, static_cast<float>(width), static_cast<float>(height) };
        }
    }
    const SDL_FRect destination{ view.offsetX + sprite.destination.x * view.scale,
        view.offsetY + sprite.destination.y * view.scale, sprite.destination.w * view.scale, sprite.destination.h * view.scale };
    drawTextureRotated(texture, destination, &source, sprite.angle, sprite.flip, sprite.opacity);
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

void Renderer::drawTextWrapped(TTF_Font* font, const std::string& text, const SDL_FRect& bounds,
    const SDL_Color& color, float scale) const
{
    if (!_renderer || !font || text.empty() || !std::isfinite(scale) || scale <= 0.0f ||
        bounds.w <= 0.0f || bounds.h <= 0.0f)
    {
        return;
    }
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
        TTF_RenderText_Blended_Wrapped(font, text.c_str(), text.size(), color,
            static_cast<int>(std::max(1.0f, bounds.w / scale))), SDL_DestroySurface);
    if (!surface)
    {
        return;
    }
    Texture texture(SDL_CreateTextureFromSurface(_renderer, surface.get()));
    if (!texture)
    {
        return;
    }
    float width = 0.0f;
    float height = 0.0f;
    if (getTextureSize(texture.get(), width, height) && width > 0.0f && height > 0.0f)
    {
        scale = std::min({ scale, bounds.w / width, bounds.h / height });
        drawTexture(texture.get(), { bounds.x, bounds.y, width * scale, height * scale });
    }
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
