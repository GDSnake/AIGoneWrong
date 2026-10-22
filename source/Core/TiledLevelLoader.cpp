#include "TiledLevelLoader.h"

#include <SDL3/SDL_log.h>
#include <SDL3_image/SDL_image.h>
#include <nlohmann/json.hpp>
#include <tinyxml2.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
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
        float imageWidth = 0.0f;
        float imageHeight = 0.0f;
        std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture{ nullptr, SDL_DestroyTexture };
    };

    Tileset loadTileset(const nlohmann::json& entry, const std::filesystem::path& mapPath,
        SDL_Renderer* renderer)
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

        const auto imagePath = (tilesetPath.parent_path() / image->Attribute("source")).lexically_normal();
        tileset.texture.reset(IMG_LoadTexture(renderer, imagePath.string().c_str()));
        if (!tileset.texture || !SDL_GetTextureSize(tileset.texture.get(), &tileset.imageWidth, &tileset.imageHeight))
        {
            throw std::runtime_error("Cannot load tileset image '" + imagePath.string() + "': " + SDL_GetError());
        }
        SDL_SetTextureBlendMode(tileset.texture.get(), SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(tileset.texture.get(), SDL_SCALEMODE_NEAREST);
        return tileset;
    }
}

bool TiledLevelLoader::load(const std::filesystem::path& mapPath, SDL_Renderer* renderer)
{
    clear();
    try
    {
        if (!renderer)
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

        for (const auto& layer : map.at("layers"))
        {
            if (!layer.value("visible", true))
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

                    const float destinationX = static_cast<float>(tileX) * tileWidth + offsetX + tileset.offsetX;
                    const float destinationY = (static_cast<float>(tileY) + 1.0f) * tileHeight + offsetY +
                        tileset.offsetY - tileset.tileHeight;
                    Tile tile;
                    tile.texture = tileset.texture.get();
                    tile.source = { sourceX, sourceY, static_cast<float>(tileset.tileWidth), static_cast<float>(tileset.tileHeight) };
                    tile.destination = { destinationX, destinationY, tile.source.w, tile.source.h };
                    tile.opacity = opacity;
                    const bool horizontal = (rawGid & FlipHorizontal) != 0;
                    const bool vertical = (rawGid & FlipVertical) != 0;
                    if (rawGid & FlipDiagonal)
                    {
                        tile.angle = vertical && !horizontal ? 270.0 : 90.0;
                        if (horizontal && vertical)
                        {
                            tile.flip = SDL_FLIP_HORIZONTAL;
                        }
                        else if (!horizontal && !vertical)
                        {
                            tile.flip = SDL_FLIP_VERTICAL;
                        }
                        tile.destination.x += (tile.source.w - tile.source.h) * 0.5f;
                        tile.destination.y += (tile.source.h - tile.source.w) * 0.5f;
                        std::swap(tile.destination.w, tile.destination.h);
                    }
                    else
                    {
                        tile.flip = static_cast<SDL_FlipMode>((horizontal ? SDL_FLIP_HORIZONTAL : 0) |
                            (vertical ? SDL_FLIP_VERTICAL : 0));
                    }
                    _tiles.push_back(tile);
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
        SDL_Log("Unable to load Tiled level '%s': %s", mapPath.string().c_str(), error.what());
        clear();
        return false;
    }
}

void TiledLevelLoader::render(SDL_Renderer* renderer) const
{
    if (!renderer || _width <= 0.0f || _height <= 0.0f)
    {
        return;
    }

    int outputWidth = 0;
    int outputHeight = 0;
    if (!SDL_GetRenderOutputSize(renderer, &outputWidth, &outputHeight) || outputWidth <= 0 || outputHeight <= 0)
    {
        return;
    }
    const float scale = std::min(static_cast<float>(outputWidth) / _width, static_cast<float>(outputHeight) / _height);
    const float offsetX = (outputWidth - _width * scale) * 0.5f;
    const float offsetY = (outputHeight - _height * scale) * 0.5f;

    for (const Tile& tile : _tiles)
    {
        const SDL_FRect destination{
            offsetX + tile.destination.x * scale,
            offsetY + tile.destination.y * scale,
            tile.destination.w * scale,
            tile.destination.h * scale
        };
        SDL_SetTextureAlphaModFloat(tile.texture, tile.opacity);
        if (tile.angle == 0.0 && tile.flip == SDL_FLIP_NONE)
        {
            SDL_RenderTexture(renderer, tile.texture, &tile.source, &destination);
        }
        else
        {
            SDL_RenderTextureRotated(renderer, tile.texture, &tile.source, &destination, tile.angle, nullptr, tile.flip);
        }
    }
}

void TiledLevelLoader::clear()
{
    _tiles.clear();
    _textures.clear();
    _width = 0.0f;
    _height = 0.0f;
}