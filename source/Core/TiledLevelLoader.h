#pragma once

#include <SDL3/SDL_render.h>

#include <filesystem>
#include <memory>
#include <vector>

class TiledLevelLoader
{
public:
    bool load(const std::filesystem::path& mapPath, SDL_Renderer* renderer);
    void render(SDL_Renderer* renderer) const;
    void clear();

private:
    struct Tile
    {
        SDL_Texture* texture = nullptr;
        SDL_FRect source{};
        SDL_FRect destination{};
        float opacity = 1.0f;
        double angle = 0.0;
        SDL_FlipMode flip = SDL_FLIP_NONE;
    };

    using Texture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

    std::vector<Texture> _textures;
    std::vector<Tile> _tiles;
    float _width = 0.0f;
    float _height = 0.0f;
};