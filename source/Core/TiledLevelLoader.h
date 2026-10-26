#pragma once

#include "Core/Renderer.h"
#include "Entities/Robot.h"

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

struct TiledDoor
{
    std::string name;
    SDL_FRect bounds{};
    float openRadius = 64.0f;
    std::optional<SDL_FRect> entryBounds;
    std::size_t renderLayer = 0;
};

struct TiledRobotSpawn
{
    std::string name;
    SDL_FPoint position{};
    EntityRobotSettings robot;
    std::size_t renderLayer = 0;
};

class TiledLevelLoader
{
public:
    bool load(const std::filesystem::path& mapPath, const Renderer& renderer);
    void render(const Renderer& renderer, const RenderView& view,
        const std::function<void(std::size_t, float, float, float)>& renderEntities = {}) const;
    std::optional<TiledSpawnPoint> getSpawn(std::string_view name) const;
    std::span<const TiledSpawnPoint> getSpawns() const noexcept;
    std::span<const TiledDoor> getDoors() const noexcept;
    std::span<const TiledRobotSpawn> getRobotSpawns() const noexcept;
    void setDoorFrame(std::size_t index, SDL_Texture* texture, const SDL_FRect& source);
    std::span<const SDL_FRect> getCollisions() const noexcept;
    std::span<const SDL_FRect> getFloorTiles() const noexcept;
    std::span<const std::vector<SDL_FPoint>> getFrameCollisions() const noexcept;
    SDL_FRect getBounds() const noexcept;
    SDL_FPoint getTileSize() const noexcept;
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
    std::vector<SDL_FRect> _floorTiles;
    std::vector<std::vector<SDL_FPoint>> _frameCollisions;
    std::vector<TiledSpawnPoint> _spawns;
    std::vector<TiledDoor> _doors;
    std::vector<TiledRobotSpawn> _robotSpawns;
    std::vector<std::size_t> _doorTileIndices;
    std::vector<std::size_t> _entityLayers;
    float _width = 0.0f;
    float _height = 0.0f;
    SDL_FPoint _tileSize{};
};