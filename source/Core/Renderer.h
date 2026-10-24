#pragma once

#include <SDL3/SDL_render.h>

#include <filesystem>
#include <memory>
#include <span>
#include <string>

struct SDL_Window;
struct TTF_Font;

struct RenderView
{
    float scale = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
};

class Renderer
{
public:
    struct TextureDeleter
    {
        void operator()(SDL_Texture* texture) const noexcept;
    };
    using Texture = std::unique_ptr<SDL_Texture, TextureDeleter>;

    static Renderer& getInstance()
    {
        static Renderer instance;
        return instance;
    }

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init();
    void clear() const;
    void present() const;
    void setDrawColor(const SDL_Color& color) const;
    Texture loadTexture(const std::filesystem::path& path) const;
    void configureTexture(SDL_Texture* texture, SDL_ScaleMode scaleMode = SDL_SCALEMODE_NEAREST,
        SDL_BlendMode blendMode = SDL_BLENDMODE_BLEND) const;
    bool getTextureSize(SDL_Texture* texture, float& width, float& height) const;
    void drawTexture(SDL_Texture* texture, const SDL_FRect& destination, const SDL_FRect* source = nullptr,
        float opacity = 1.0f) const;
    void drawTextureRotated(SDL_Texture* texture, const SDL_FRect& destination,
        const SDL_FRect* source, double angle, SDL_FlipMode flip = SDL_FLIP_NONE, float opacity = 1.0f) const;
    void drawLines(std::span<const SDL_FPoint> points, const SDL_Color& color) const;
    void drawTriangleFan(std::span<const SDL_FPoint> points, const SDL_Color& color) const;
    void drawOverlay(const SDL_Color& color) const;
    void drawText(TTF_Font* font, const std::string& text, const SDL_FPoint& origin, const SDL_Color& color) const;
    bool getOutputSize(int& width, int& height) const;
    SDL_Window* getWindow() const noexcept;
    SDL_Renderer* getRenderer() const noexcept;
    bool applyWindowSettings() const;
    void shutdown();

#if DEBUG_BUILD
	void writeFPSOnWindowTitle(Uint64 fps);
#endif

private:
    Renderer() = default;
    ~Renderer();

    SDL_Window* _window = nullptr;
    SDL_Renderer* _renderer = nullptr;
};