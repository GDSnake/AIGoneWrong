#include "TiledLevelLoader.h"

#include <nlohmann/json.hpp>
#include <tinyxml2.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    constexpr std::uint32_t FlipHorizontal = 0x80000000u;
    constexpr std::uint32_t FlipVertical = 0x40000000u;
    constexpr std::uint32_t FlipDiagonal = 0x20000000u;
    constexpr std::uint32_t TileIdMask = 0x0fffffffu;

    struct Tileset
    {
        std::uint32_t firstGid;
        int tileWidth;
        int tileHeight;
        int tileCount;
        int columns;
        int spacing;
        int margin;
        int offsetX = 0;
        int offsetY = 0;
        SDL_FPoint objectAlignment{ 0.0f, 1.0f };
        float imageWidth = 0.0f;
        float imageHeight = 0.0f;
        Renderer::Texture texture;
    };

    Tileset loadTileset(const nlohmann::json& entry, const std::filesystem::path& mapPath,
        const Renderer& renderer)
    {
        const auto tilesetPath = (mapPath.parent_path() / entry.at("source").get<std::string>()).lexically_normal();
        tinyxml2::XMLDocument document;
        if (document.LoadFile(tilesetPath.string().c_str()) != tinyxml2::XML_SUCCESS)
        {
            throw std::runtime_error("Cannot read tileset '" + tilesetPath.string() + "': " + document.ErrorStr());
        }

        const auto* root = document.FirstChildElement("tileset");
        const auto* image = root ? root->FirstChildElement("image") : nullptr;
        if (!root || !image || !image->Attribute("source"))
        {
            throw std::runtime_error("Tileset must contain a single atlas image: " + tilesetPath.string());
        }

        Tileset tileset;
        tileset.firstGid = entry.at("firstgid").get<std::uint32_t>();
        tileset.tileWidth = root->IntAttribute("tilewidth");
        tileset.tileHeight = root->IntAttribute("tileheight");
        tileset.tileCount = root->IntAttribute("tilecount");
        tileset.columns = root->IntAttribute("columns");
        tileset.spacing = root->IntAttribute("spacing", 0);
        tileset.margin = root->IntAttribute("margin", 0);
        if (tileset.firstGid == 0 || tileset.firstGid > TileIdMask || tileset.tileWidth <= 0 ||
            tileset.tileHeight <= 0 || tileset.tileCount <= 0 || tileset.columns <= 0 ||
            tileset.spacing < 0 || tileset.margin < 0)
        {
            throw std::runtime_error("Invalid tileset dimensions or firstgid: " + tilesetPath.string());
        }

        if (const auto* offset = root->FirstChildElement("tileoffset"))
        {
            tileset.offsetX = offset->IntAttribute("x", 0);
            tileset.offsetY = offset->IntAttribute("y", 0);
        }

        if (const char* alignment = root->Attribute("objectalignment"))
        {
            static constexpr std::array<std::pair<std::string_view, SDL_FPoint>, 10> alignments{ {
                { "unspecified", { 0.0f, 1.0f } }, { "topleft", { 0.0f, 0.0f } },
                { "top", { 0.5f, 0.0f } }, { "topright", { 1.0f, 0.0f } },
                { "left", { 0.0f, 0.5f } }, { "center", { 0.5f, 0.5f } },
                { "right", { 1.0f, 0.5f } }, { "bottomleft", { 0.0f, 1.0f } },
                { "bottom", { 0.5f, 1.0f } }, { "bottomright", { 1.0f, 1.0f } }
            } };
            const auto found = std::find_if(alignments.begin(), alignments.end(), [alignment](const auto& entry)
            {
                return entry.first == alignment;
            });
            if (found == alignments.end())
            {
                throw std::runtime_error("Unsupported tileset object alignment: " + std::string(alignment));
            }
            tileset.objectAlignment = found->second;
        }

        const auto imagePath = (tilesetPath.parent_path() / image->Attribute("source")).lexically_normal();
        tileset.texture = renderer.loadTexture(imagePath);
        if (!renderer.getTextureSize(tileset.texture.get(), tileset.imageWidth, tileset.imageHeight))
        {
            throw std::runtime_error("Cannot read tileset image dimensions: " + imagePath.string());
        }
        return tileset;
    }
}

bool TiledLevelLoader::load(const std::filesystem::path& mapPath, const Renderer& renderer)
{
    clear();
    try
    {
        int outputWidth = 0;
        int outputHeight = 0;
        if (!renderer.getOutputSize(outputWidth, outputHeight))
        {
            throw std::runtime_error("A renderer is required to load a level");
        }

        std::ifstream input(mapPath);
        if (!input)
        {
            throw std::runtime_error("Cannot open map");
        }
        const auto map = nlohmann::json::parse(input);
        if (map.at("orientation") != "orthogonal" || map.value("infinite", false))
        {
            throw std::runtime_error("Only finite orthogonal Tiled maps are supported");
        }

        const int mapWidth = map.at("width").get<int>();
        const int mapHeight = map.at("height").get<int>();
        const int tileWidth = map.at("tilewidth").get<int>();
        const int tileHeight = map.at("tileheight").get<int>();
        if (mapWidth <= 0 || mapHeight <= 0 || tileWidth <= 0 || tileHeight <= 0)
        {
            throw std::runtime_error("Invalid map dimensions");
        }

        const auto renderOrder = map.value("renderorder", std::string("right-down"));
        if (renderOrder != "right-down" && renderOrder != "right-up" &&
            renderOrder != "left-down" && renderOrder != "left-up")
        {
            throw std::runtime_error("Unsupported map render order");
        }
        const bool renderLeft = renderOrder.starts_with("left");
        const bool renderUp = renderOrder.ends_with("up");

        std::vector<Tileset> tilesets;
        for (const auto& entry : map.at("tilesets"))
        {
            tilesets.push_back(loadTileset(entry, mapPath, renderer));
        }
        std::sort(tilesets.begin(), tilesets.end(), [](const Tileset& left, const Tileset& right)
        {
            return left.firstGid < right.firstGid;
        });

        const auto makeTile = [&tilesets](std::uint32_t rawGid)
        {
            const auto gid = rawGid & TileIdMask;
            auto found = std::upper_bound(tilesets.begin(), tilesets.end(), gid,
                [](std::uint32_t tileGid, const Tileset& tileset) { return tileGid < tileset.firstGid; });
            if (found == tilesets.begin())
            {
                throw std::runtime_error("Tile GID has no matching tileset");
            }
            const Tileset& tileset = *--found;
            const auto localId = gid - tileset.firstGid;
            if (localId >= static_cast<std::uint32_t>(tileset.tileCount))
            {
                throw std::runtime_error("Tile GID exceeds its tileset tile count");
            }
            const float sourceX = static_cast<float>(tileset.margin) +
                (localId % tileset.columns) * (static_cast<float>(tileset.tileWidth) + tileset.spacing);
            const float sourceY = static_cast<float>(tileset.margin) +
                (localId / tileset.columns) * (static_cast<float>(tileset.tileHeight) + tileset.spacing);
            if (sourceX + tileset.tileWidth > tileset.imageWidth || sourceY + tileset.tileHeight > tileset.imageHeight)
            {
                throw std::runtime_error("Tile source rectangle exceeds its atlas image");
            }
            Tile tile;
            tile.texture = tileset.texture.get();
            tile.source = { sourceX, sourceY, static_cast<float>(tileset.tileWidth), static_cast<float>(tileset.tileHeight) };
            return std::pair{ tile, &tileset };
        };
        const auto appendTile = [this](Tile tile, std::uint32_t rawGid)
        {
            const bool horizontal = (rawGid & FlipHorizontal) != 0;
            const bool vertical = (rawGid & FlipVertical) != 0;
            if (rawGid & FlipDiagonal)
            {
                tile.angle += vertical && !horizontal ? 270.0 : 90.0;
                if (horizontal && vertical)
                {
                    tile.flip = SDL_FLIP_HORIZONTAL;
                }
                else if (!horizontal && !vertical)
                {
                    tile.flip = SDL_FLIP_VERTICAL;
                }
                tile.destination.x += (tile.destination.w - tile.destination.h) * 0.5f;
                tile.destination.y += (tile.destination.h - tile.destination.w) * 0.5f;
                std::swap(tile.destination.w, tile.destination.h);
            }
            else
            {
                tile.flip = static_cast<SDL_FlipMode>((horizontal ? SDL_FLIP_HORIZONTAL : 0) |
                    (vertical ? SDL_FLIP_VERTICAL : 0));
            }
            _tiles.push_back(tile);
        };

        for (const auto& layer : map.at("layers"))
        {
            if (layer.at("type") == "objectgroup")
            {
                const bool visible = layer.value("visible", true);
                const float opacity = layer.value("opacity", 1.0f);
                if (!(opacity >= 0.0f && opacity <= 1.0f))
                {
                    throw std::runtime_error("Invalid object layer opacity");
                }
                const float offsetX = layer.value("offsetx", 0.0f) + layer.value("x", 0.0f) * tileWidth;
                const float offsetY = layer.value("offsety", 0.0f) + layer.value("y", 0.0f) * tileHeight;
                const std::size_t renderLayer = _entityLayers.size();
                _entityLayers.push_back(_tiles.size());
                std::vector<const nlohmann::json*> objects;
                for (const auto& object : layer.at("objects"))
                {
                    objects.push_back(&object);
                }
                const auto drawOrder = layer.value("draworder", std::string("topdown"));
                if (drawOrder == "topdown")
                {
                    std::stable_sort(objects.begin(), objects.end(), [](const auto* left, const auto* right)
                    {
                        return left->value("y", 0.0f) < right->value("y", 0.0f);
                    });
                }
                else if (drawOrder != "index")
                {
                    throw std::runtime_error("Unsupported object layer draw order");
                }
                for (const auto* entry : objects)
                {
                    const auto& object = *entry;
                    if (object.contains("gid"))
                    {
                        if (!visible || !object.value("visible", true))
                        {
                            continue;
                        }
                        const auto value = object.at("gid").get<std::int64_t>();
                        if (value < 0 || value > UINT32_MAX)
                        {
                            throw std::runtime_error("Tile object GID is out of range");
                        }
                        const auto rawGid = static_cast<std::uint32_t>(value);
                        if ((rawGid & TileIdMask) == 0)
                        {
                            continue;
                        }
                        auto [tile, tileset] = makeTile(rawGid);
                        const float width = object.value("width", tile.source.w);
                        const float height = object.value("height", tile.source.h);
                        const float originX = object.at("x").get<float>() + offsetX;
                        const float originY = object.at("y").get<float>() + offsetY;
                        const float rotation = object.value("rotation", 0.0f);
                        const float objectOpacity = object.value("opacity", 1.0f);
                        if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0f || height <= 0.0f ||
                            !std::isfinite(originX) || !std::isfinite(originY) || !std::isfinite(rotation) ||
                            !(objectOpacity >= 0.0f && objectOpacity <= 1.0f))
                        {
                            throw std::runtime_error("Invalid tile object geometry or opacity");
                        }
                        tile.destination = {
                            originX - tileset->objectAlignment.x * width + tileset->offsetX * width / tile.source.w,
                            originY - tileset->objectAlignment.y * height + tileset->offsetY * height / tile.source.h,
                            width, height
                        };
                        const float radians = rotation * std::numbers::pi_v<float> / 180.0f;
                        const float centerX = tile.destination.x + width * 0.5f - originX;
                        const float centerY = tile.destination.y + height * 0.5f - originY;
                        tile.destination.x = originX + std::cos(radians) * centerX - std::sin(radians) * centerY - width * 0.5f;
                        tile.destination.y = originY + std::sin(radians) * centerX + std::cos(radians) * centerY - height * 0.5f;
                        tile.opacity = opacity * objectOpacity;
                        tile.angle = rotation;
                        appendTile(tile, rawGid);
                        continue;
                    }
                    const auto name = object.value("name", std::string{});
                    if (name.empty() || !object.value("point", false))
                    {
                        continue;
                    }
                    if (std::any_of(_spawns.begin(), _spawns.end(), [&name](const TiledSpawnPoint& spawn)
                        { return spawn.name == name; }))
                    {
                        throw std::runtime_error("Spawn point names must be unique: " + name);
                    }
                    const SDL_FPoint position{ object.at("x").get<float>() + offsetX,
                        object.at("y").get<float>() + offsetY };
                    if (!std::isfinite(position.x) || !std::isfinite(position.y))
                    {
                        throw std::runtime_error("Spawn point positions must be finite: " + name);
                    }
                    _spawns.push_back({ name, position, renderLayer });
                }
                continue;
            }

            const bool visible = layer.value("visible", true);
            const bool collisionLayer = layer.value("name", std::string{}) == "collision";
            if (!visible && !collisionLayer)
            {
                continue;
            }
            if (layer.at("type") == "group")
            {
                throw std::runtime_error("Group layers are not supported");
            }
            if (layer.at("type") != "tilelayer")
            {
                continue;
            }

            const int width = layer.at("width").get<int>();
            const int height = layer.at("height").get<int>();
            const auto& data = layer.at("data");
            if (width <= 0 || height <= 0 || !data.is_array() ||
                data.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
            {
                throw std::runtime_error("Tile layers must contain an uncompressed JSON array matching their dimensions");
            }

            const float opacity = layer.value("opacity", 1.0f);
            if (!(opacity >= 0.0f && opacity <= 1.0f))
            {
                throw std::runtime_error("Invalid layer opacity");
            }
            const float offsetX = layer.value("offsetx", 0.0f) + layer.value("x", 0.0f) * tileWidth;
            const float offsetY = layer.value("offsety", 0.0f) + layer.value("y", 0.0f) * tileHeight;

            for (int row = 0; row < height; ++row)
            {
                const int tileY = renderUp ? height - row - 1 : row;
                for (int column = 0; column < width; ++column)
                {
                    const int tileX = renderLeft ? width - column - 1 : column;
                    const auto value = data.at(static_cast<std::size_t>(tileY) * width + tileX).get<std::int64_t>();
                    if (value < 0 || value > UINT32_MAX)
                    {
                        throw std::runtime_error("Tile GID is out of range");
                    }
                    const auto rawGid = static_cast<std::uint32_t>(value);
                    const auto gid = rawGid & TileIdMask;
                    if (gid == 0)
                    {
                        continue;
                    }

                    if (collisionLayer)
                    {
                        _collisions.push_back({ static_cast<float>(tileX) * tileWidth + offsetX,
                            static_cast<float>(tileY) * tileHeight + offsetY,
                            static_cast<float>(tileWidth), static_cast<float>(tileHeight) });
                    }
                    if (!visible)
                    {
                        continue;
                    }

                    auto [tile, tileset] = makeTile(rawGid);
                    const float destinationX = static_cast<float>(tileX) * tileWidth + offsetX + tileset->offsetX;
                    const float destinationY = (static_cast<float>(tileY) + 1.0f) * tileHeight + offsetY +
                        tileset->offsetY - tileset->tileHeight;
                    tile.destination = { destinationX, destinationY, tile.source.w, tile.source.h };
                    tile.opacity = opacity;
                    appendTile(tile, rawGid);
                }
            }
        }

        for (auto& tileset : tilesets)
        {
            _textures.push_back(std::move(tileset.texture));
        }
        _width = static_cast<float>(mapWidth) * tileWidth;
        _height = static_cast<float>(mapHeight) * tileHeight;
        return true;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Unable to load Tiled level '" << mapPath.string() << "': " << error.what() << '\n';
        clear();
        return false;
    }
}

void TiledLevelLoader::render(const Renderer& renderer,
    const std::function<void(std::size_t, float, float, float)>& renderEntities) const
{
    if (_width <= 0.0f || _height <= 0.0f)
    {
        return;
    }

    int outputWidth = 0;
    int outputHeight = 0;
    if (!renderer.getOutputSize(outputWidth, outputHeight) || outputWidth <= 0 || outputHeight <= 0)
    {
        return;
    }
    const float scale = std::min(static_cast<float>(outputWidth) / _width, static_cast<float>(outputHeight) / _height);
    const float offsetX = (outputWidth - _width * scale) * 0.5f;
    const float offsetY = (outputHeight - _height * scale) * 0.5f;

    std::size_t renderLayer = 0;
    for (std::size_t tileIndex = 0; tileIndex <= _tiles.size(); ++tileIndex)
    {
        while (renderLayer < _entityLayers.size() && tileIndex == _entityLayers[renderLayer])
        {
            if (renderEntities)
            {
                renderEntities(renderLayer, scale, offsetX, offsetY);
            }
            ++renderLayer;
        }
        if (tileIndex == _tiles.size())
        {
            break;
        }
        const Tile& tile = _tiles[tileIndex];
        const SDL_FRect destination{
            offsetX + tile.destination.x * scale,
            offsetY + tile.destination.y * scale,
            tile.destination.w * scale,
            tile.destination.h * scale
        };
        if (tile.angle == 0.0 && tile.flip == SDL_FLIP_NONE)
        {
            renderer.drawTexture(tile.texture, destination, &tile.source, tile.opacity);
        }
        else
        {
            renderer.drawTextureRotated(tile.texture, destination, &tile.source, tile.angle, tile.flip, tile.opacity);
        }
    }
}

std::optional<TiledSpawnPoint> TiledLevelLoader::getSpawn(std::string_view name) const
{
    const auto spawn = std::find_if(_spawns.begin(), _spawns.end(), [name](const TiledSpawnPoint& point)
    {
        return point.name == name;
    });
    if (spawn == _spawns.end())
    {
        return std::nullopt;
    }
    return *spawn;
}

std::span<const TiledSpawnPoint> TiledLevelLoader::getSpawns() const noexcept
{
    return _spawns;
}

std::span<const SDL_FRect> TiledLevelLoader::getCollisions() const noexcept
{
    return _collisions;
}

SDL_FRect TiledLevelLoader::getBounds() const noexcept
{
    return { 0.0f, 0.0f, _width, _height };
}

void TiledLevelLoader::clear()
{
    _tiles.clear();
    _textures.clear();
    _collisions.clear();
    _spawns.clear();
    _entityLayers.clear();
    _width = 0.0f;
    _height = 0.0f;
}