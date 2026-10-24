#include "TiledLevelLoader.h"

#include <nlohmann/json.hpp>
#include <tinyxml2.h>
#include <mapbox/earcut.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <unordered_map>

namespace
{
    constexpr std::uint32_t FlipHorizontal = 0x80000000u;
    constexpr std::uint32_t FlipVertical = 0x40000000u;
    constexpr std::uint32_t FlipDiagonal = 0x20000000u;
    constexpr std::uint32_t TileIdMask = 0x0fffffffu;

    using CollisionPolygon = std::vector<SDL_FPoint>;
    using CollisionParts = std::vector<CollisionPolygon>;

    float collisionAttribute(const tinyxml2::XMLElement& object, const char* name)
    {
        float value = 0.0f;
        if (object.Attribute(name) &&
            (object.QueryFloatAttribute(name, &value) != tinyxml2::XML_SUCCESS || !std::isfinite(value)))
        {
            throw std::runtime_error("Invalid tile collision attribute: " + std::string(name));
        }
        return value;
    }

    CollisionParts loadCollisionParts(const tinyxml2::XMLElement& object)
    {
        if (object.FirstChildElement("ellipse") || object.FirstChildElement("point") ||
            object.FirstChildElement("polyline") || object.FirstChildElement("text") || object.Attribute("gid"))
        {
            throw std::runtime_error("Tile collision shapes must be rectangles or simple polygons");
        }

        CollisionPolygon vertices;
        const auto* polygon = object.FirstChildElement("polygon");
        if (polygon)
        {
            const char* coordinates = polygon->Attribute("points");
            if (!coordinates)
            {
                throw std::runtime_error("Tile collision polygon is missing its points");
            }
            std::istringstream points(coordinates);
            while (points >> std::ws && !points.eof())
            {
                SDL_FPoint vertex;
                char separator = 0;
                if (!(points >> vertex.x >> separator >> vertex.y) || separator != ',' ||
                    !std::isfinite(vertex.x) || !std::isfinite(vertex.y))
                {
                    throw std::runtime_error("Invalid tile collision polygon coordinates");
                }
                vertices.push_back(vertex);
            }
            if (vertices.size() > 3 && vertices.front().x == vertices.back().x && vertices.front().y == vertices.back().y)
            {
                vertices.pop_back();
            }
            if (vertices.size() < 3)
            {
                throw std::runtime_error("Tile collision polygon needs at least three points");
            }
        }
        else
        {
            const float width = collisionAttribute(object, "width");
            const float height = collisionAttribute(object, "height");
            if (width <= 0.0f || height <= 0.0f)
            {
                throw std::runtime_error("Tile collision rectangle dimensions must be positive");
            }
            vertices = { { 0.0f, 0.0f }, { width, 0.0f }, { width, height }, { 0.0f, height } };
        }

        const float offsetX = collisionAttribute(object, "x");
        const float offsetY = collisionAttribute(object, "y");
        const float angle = collisionAttribute(object, "rotation") * std::numbers::pi_v<float> / 180.0f;
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        for (SDL_FPoint& vertex : vertices)
        {
            const float localX = vertex.x;
            const float localY = vertex.y;
            vertex = { offsetX + cosine * localX - sine * localY, offsetY + sine * localX + cosine * localY };
            if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y))
            {
                throw std::runtime_error("Tile collision coordinates are out of range");
            }
        }
        if (!polygon)
        {
            return { std::move(vertices) };
        }

        std::vector<std::vector<std::array<double, 2>>> rings(1);
        double polygonArea = 0.0;
        for (std::size_t index = 0; index < vertices.size(); ++index)
        {
            const SDL_FPoint& vertex = vertices[index];
            const SDL_FPoint& next = vertices[(index + 1) % vertices.size()];
            rings.front().push_back({ vertex.x, vertex.y });
            polygonArea += static_cast<double>(vertex.x) * next.y - static_cast<double>(vertex.y) * next.x;
        }
        polygonArea = std::abs(polygonArea) * 0.5;
        const auto indices = mapbox::earcut<std::uint32_t>(rings);
        if (indices.empty() || indices.size() % 3 != 0 || polygonArea <= 0.0001)
        {
            throw std::runtime_error("Tile collision polygon must be simple and have a positive area");
        }
        CollisionParts parts;
        double triangleArea = 0.0;
        for (std::size_t index = 0; index < indices.size(); index += 3)
        {
            CollisionPolygon triangle{ vertices.at(indices[index]), vertices.at(indices[index + 1]), vertices.at(indices[index + 2]) };
            const double area = std::abs((static_cast<double>(triangle[1].x) - triangle[0].x) *
                (static_cast<double>(triangle[2].y) - triangle[0].y) -
                (static_cast<double>(triangle[1].y) - triangle[0].y) *
                (static_cast<double>(triangle[2].x) - triangle[0].x)) * 0.5;
            if (area > 0.0001)
            {
                triangleArea += area;
                parts.push_back(std::move(triangle));
            }
        }
        if (parts.empty() || std::abs(triangleArea - polygonArea) > std::max(0.0001, polygonArea * 0.0001))
        {
            throw std::runtime_error("Tile collision polygon could not be triangulated without changing its area");
        }
        return parts;
    }

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
        std::unordered_map<std::uint32_t, CollisionParts> collisions;
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

        for (const auto* tile = root->FirstChildElement("tile"); tile; tile = tile->NextSiblingElement("tile"))
        {
            const auto* objects = tile->FirstChildElement("objectgroup");
            if (!objects)
            {
                continue;
            }
            int tileId = -1;
            if (tile->QueryIntAttribute("id", &tileId) != tinyxml2::XML_SUCCESS || tileId < 0 || tileId >= tileset.tileCount)
            {
                throw std::runtime_error("Invalid collision tile ID in tileset: " + tilesetPath.string());
            }
            auto& parts = tileset.collisions[static_cast<std::uint32_t>(tileId)];
            for (const auto* object = objects->FirstChildElement("object"); object; object = object->NextSiblingElement("object"))
            {
                for (auto& part : loadCollisionParts(*object))
                {
                    parts.push_back(std::move(part));
                }
            }
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

        const auto propertyValue = [](const nlohmann::json& object, std::string_view name) -> const nlohmann::json*
        {
            const auto properties = object.find("properties");
            if (properties == object.end())
            {
                return nullptr;
            }
            if (!properties->is_array())
            {
                throw std::runtime_error("Object properties must be an array");
            }
            for (const auto& property : *properties)
            {
                if (property.at("name").get_ref<const std::string&>() == name)
                {
                    return &property.at("value");
                }
            }
            return nullptr;
        };
        std::vector<std::pair<std::string, SDL_FRect>> doorEntries;

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
                    const auto objectClass = object.value("class", object.value("type", std::string{}));
                    if (objectClass == "DoorEntry")
                    {
                        const auto* door = propertyValue(object, "door");
                        const SDL_FRect bounds{ object.at("x").get<float>() + offsetX,
                            object.at("y").get<float>() + offsetY,
                            object.at("width").get<float>(), object.at("height").get<float>() };
                        if (!door || !door->is_string() || door->get_ref<const std::string&>().empty() ||
                            !std::isfinite(bounds.x) || !std::isfinite(bounds.y) ||
                            !std::isfinite(bounds.w) || !std::isfinite(bounds.h) || bounds.w <= 0.0f || bounds.h <= 0.0f ||
                            object.value("rotation", 0.0f) != 0.0f || object.contains("gid") ||
                            object.value("ellipse", false) || object.value("point", false) ||
                            object.contains("polygon") || object.contains("polyline"))
                        {
                            throw std::runtime_error("DoorEntry must be an unrotated rectangle with a nonempty door property");
                        }
                        doorEntries.emplace_back(door->get<std::string>(), bounds);
                        continue;
                    }
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
                        if (objectClass == "Door")
                        {
                            const auto name = object.value("name", std::string{});
                            const auto* radius = propertyValue(object, "openRadius");
                            const float openRadius = radius ? radius->get<float>() : 64.0f;
                            if (name.empty() || !std::isfinite(openRadius) || openRadius <= 0.0f || rotation != 0.0f ||
                                (rawGid & (FlipHorizontal | FlipVertical | FlipDiagonal)) != 0 ||
                                std::any_of(_doors.begin(), _doors.end(), [&name](const TiledDoor& door)
                                    { return door.name == name; }))
                            {
                                throw std::runtime_error("Door must have a unique name, positive openRadius, and no rotation or flips");
                            }
                            _doors.push_back({ name, tile.destination, openRadius, std::nullopt, renderLayer });
                            _doorTileIndices.push_back(_tiles.size());
                        }
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
            const bool frameCollisionLayer = layer.value("name", std::string{}) == "collision_in_frame";
            if (!visible && !collisionLayer && !frameCollisionLayer)
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
                    if (!visible && !frameCollisionLayer)
                    {
                        continue;
                    }

                    auto [tile, tileset] = makeTile(rawGid);
                    const float destinationX = static_cast<float>(tileX) * tileWidth + offsetX + tileset->offsetX;
                    const float destinationY = (static_cast<float>(tileY) + 1.0f) * tileHeight + offsetY +
                        tileset->offsetY - tileset->tileHeight;
                    if (frameCollisionLayer)
                    {
                        const auto shapes = tileset->collisions.find(gid - tileset->firstGid);
                        if (shapes != tileset->collisions.end())
                        {
                            for (const auto& shape : shapes->second)
                            {
                                CollisionPolygon vertices;
                                vertices.reserve(shape.size());
                                for (const SDL_FPoint& vertex : shape)
                                {
                                    float unitX = vertex.x / static_cast<float>(tileset->tileWidth);
                                    float unitY = vertex.y / static_cast<float>(tileset->tileHeight);
                                    if (rawGid & FlipDiagonal)
                                    {
                                        std::swap(unitX, unitY);
                                    }
                                    if (rawGid & FlipHorizontal)
                                    {
                                        unitX = 1.0f - unitX;
                                    }
                                    if (rawGid & FlipVertical)
                                    {
                                        unitY = 1.0f - unitY;
                                    }
                                    vertices.push_back({ destinationX + unitX * tileset->tileWidth,
                                        destinationY + unitY * tileset->tileHeight });
                                }
                                _frameCollisions.push_back(std::move(vertices));
                            }
                        }
                    }
                    if (!visible)
                    {
                        continue;
                    }
                    tile.destination = { destinationX, destinationY, tile.source.w, tile.source.h };
                    tile.opacity = opacity;
                    appendTile(tile, rawGid);
                }
            }
        }

        for (const auto& [name, bounds] : doorEntries)
        {
            const auto door = std::find_if(_doors.begin(), _doors.end(), [&name](const TiledDoor& entry)
            {
                return entry.name == name;
            });
            if (door == _doors.end() || door->entryBounds)
            {
                throw std::runtime_error("DoorEntry references an unknown door or duplicates its entry: " + name);
            }
            door->entryBounds = bounds;
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

std::span<const TiledDoor> TiledLevelLoader::getDoors() const noexcept
{
    return _doors;
}

void TiledLevelLoader::setDoorFrame(std::size_t index, SDL_Texture* texture, const SDL_FRect& source)
{
    if (index >= _doorTileIndices.size() || !texture ||
        !std::isfinite(source.x) || !std::isfinite(source.y) || !std::isfinite(source.w) || !std::isfinite(source.h) ||
        source.x < 0.0f || source.y < 0.0f || source.w <= 0.0f || source.h <= 0.0f)
    {
        throw std::invalid_argument("Invalid door frame");
    }
    Tile& tile = _tiles[_doorTileIndices[index]];
    tile.texture = texture;
    tile.source = source;
}

std::span<const SDL_FRect> TiledLevelLoader::getCollisions() const noexcept
{
    return _collisions;
}

std::span<const std::vector<SDL_FPoint>> TiledLevelLoader::getFrameCollisions() const noexcept
{
    return _frameCollisions;
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
    _frameCollisions.clear();
    _spawns.clear();
    _doors.clear();
    _doorTileIndices.clear();
    _entityLayers.clear();
    _width = 0.0f;
    _height = 0.0f;
}