#pragma once

#include "Core/Renderer.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct TiledSpawnPoint
{
    std::string name;
    SDL_FPoint position{};
    std::size_t renderLayer = 0;
};

class TiledLevelLoader
{
public:
    bool load(const std::filesystem::path& mapPath, const Renderer& renderer);
    void render(const Renderer& renderer,
        const std::function<void(std::size_t, float, float, float)>& renderEntities = {}) const;
    std::optional<TiledSpawnPoint> getSpawn(std::string_view name) const;
    std::span<const TiledSpawnPoint> getSpawns() const noexcept;
    std::span<const SDL_FRect> getCollisions() const noexcept;
    SDL_FRect getBounds() const noexcept;
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

    std::vector<Renderer::Texture> _textures;
    std::vector<Tile> _tiles;
    std::vector<SDL_FRect> _collisions;
    std::vector<TiledSpawnPoint> _spawns;
    std::vector<std::size_t> _entityLayers;
    float _width = 0.0f;
    float _height = 0.0f;
};