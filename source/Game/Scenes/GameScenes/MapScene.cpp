#include "MapScene.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "AssetManagers/AudioManager.h"
#include "AssetManagers/FontsManager.h"
#include "AssetManagers/SpriteManager.h"

#include "Core/Renderer.h"
#include "Entities/Entities.h"
#include "Entities/Sprite.h"

MapScene::MapScene(std::filesystem::path mapPath, std::string sceneName, float viewScale)
    : _mapPath(std::move(mapPath)), _sceneName(std::move(sceneName))
{
    setViewScale(viewScale);
}

void MapScene::init()
{
    _input = std::make_unique<InputManager>();
    SpriteManager::getInstance();
    Renderer::getInstance();

    if (!_level.load(_mapPath, Renderer::getInstance()))
    {
        _input->quit();
        return;
    }

    const auto spawn = _level.getSpawn("PlayerSpawn");
    if (!spawn)
    {
        SDL_Log("Map '%s' is missing the PlayerSpawn point", _mapPath.string().c_str());
        _input->quit();
        return;
    }
    try
    {
        Entities::GetInstance().InitializePhysics(_level.getCollisions(), _level.getBounds(), _level.getFrameCollisions());
        auto sprite = SpriteManager::getInstance().getSprite("player");
        if (!sprite)
        {
            throw std::runtime_error("Player sprite is missing");
        }
        const auto columns = static_cast<std::size_t>(sprite->getSourceRect().w / sprite->getSourceRect(0).w);
        if (columns < 6 || sprite->getFrameCount() < columns * 2 + 6)
        {
            throw std::runtime_error("Player sheet must contain four idle frames and six walk frames in row three");
        }
        Renderer::getInstance().configureTexture(sprite->getTexture().get());
        Entities::GetInstance().AddEntity({
            .x = spawn->position.x,
            .y = spawn->position.y,
            .sprite = std::move(sprite),
            .visible = true,
            .name = Entities::PlayerName,
            .collidable = true,
            .animation = { 0, 4, columns * 2, 6, 0.18f, 0.1f },
            .behavior = EntityBehavior::PlayerInput,
            .renderLayer = spawn->renderLayer
        });
        const auto doors = _level.getDoors();
        if (!doors.empty())
        {
            _doorSprite = SpriteManager::getInstance().getSprite("door");
            if (!_doorSprite || _doorSprite->getFrameCount() < 2)
            {
                throw std::runtime_error("Door sprite must contain closed and open frames");
            }
            Renderer::getInstance().configureTexture(_doorSprite->getTexture().get());
            for (std::size_t index = 0; index < doors.size(); ++index)
            {
                const TiledDoor& door = doors[index];
                const SDL_FRect entry = door.entryBounds.value_or(SDL_FRect{});
                Entities::GetInstance().AddEntity({
                    .x = door.bounds.x + door.bounds.w * 0.5f,
                    .y = door.bounds.y + door.bounds.h,
                    .sprite = _doorSprite,
                    .visible = false,
                    .name = door.name,
                    .collidable = true,
                    .colliderWidth = door.bounds.w,
                    .colliderHeight = door.bounds.h,
                    .behavior = EntityBehavior::Door,
                    .movementSpeed = 0.0f,
                    .renderLayer = door.renderLayer,
                    .door = { door.openRadius, entry, index }
                });
            }
        }
        for (const TiledRobotSpawn& robot : _level.getRobotSpawns())
        {
            const RobotProfile& profile = GetRobotProfile(robot.robot.type);
            auto sprite = SpriteManager::getInstance().getSprite(profile.spriteName);
            if (!sprite)
            {
                throw std::runtime_error("Missing robot sprite: " + std::string(profile.spriteName));
            }
            Renderer::getInstance().configureTexture(sprite->getTexture().get());
            const std::size_t firstFrame = RobotIdleRows[static_cast<std::size_t>(robot.robot.direction)] * 8;
            Entities::GetInstance().AddEntity({
                .x = robot.position.x,
                .y = robot.position.y,
                .sprite = std::move(sprite),
                .visible = true,
                .name = robot.name,
                .collidable = true,
                .colliderWidth = profile.colliderWidth,
                .colliderHeight = profile.colliderHeight,
                .animation = { firstFrame, 8, firstFrame, 8, 0.12f, 0.12f },
                .behavior = EntityBehavior::Robot,
                .movementSpeed = 0.0f,
                .renderLayer = robot.renderLayer,
                .robot = robot.robot
            });
        }
    }
    catch (const std::exception& error)
    {
        SDL_Log("Unable to initialize entities for '%s': %s", _mapPath.string().c_str(), error.what());
        _input->quit();
        return;
    }

    _titleText = std::format("{}{}",TitleFontInfo.baseText, "Title");
    //Add all the texts you need for the scene

    _titleFont = FontsManager::getInstance().getFont(TitleFontInfo.fontName);
    //Add all the fonts you need for the scene
}

void MapScene::handleEvents(const SDL_Event& event)
{
    _input->handleEvent(event);
#ifdef DEBUG
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_M && !event.key.repeat)
    {
        _drawCollisions = !_drawCollisions;
    }
#endif
}

bool MapScene::shouldQuit() const
{
    return _input->shouldQuit();
}

void MapScene::handleInput()
{
    if (_input->isKeyPressed(SDL_SCANCODE_ESCAPE))
    {
        _input->quit();
    }
}

void MapScene::update(float deltaTime)
{
    if (_input->shouldQuit())
    {
        return;
    }
    if (isTransitioning())
    {
        Entities::GetInstance().SetVelocity(Entities::PlayerIndex, 0.0f, 0.0f);
    }
    else
    {
        Entities::GetInstance().UpdateControllers(*_input);
        for (const DoorVisualChange& change : Entities::GetInstance().GetDoorVisualChanges())
        {
            _level.setDoorFrame(change.visualIndex, _doorSprite->getTexture().get(),
                _doorSprite->getSourceRect(change.frameIndex));
        }
        if (Entities::GetInstance().ConsumeSceneExitRequest())
        {
            auto nextScene = createExitScene();
            const bool requested = nextScene ? transitionTo(std::move(nextScene)) : requestExit();
            if (requested)
            {
                Entities::GetInstance().SetVelocity(Entities::PlayerIndex, 0.0f, 0.0f);
            }
        }
    }
    Entities::GetInstance().Update(deltaTime);
    _input->update();
}

void MapScene::render()
{
    Renderer& renderer = Renderer::getInstance();
    renderer.setDrawColor({ 0, 0, 0, 255 });
    renderer.clear();
    const SDL_FRect bounds = _level.getBounds();
    int outputWidth = 0;
    int outputHeight = 0;
    if (bounds.w <= 0.0f || bounds.h <= 0.0f ||
        !renderer.getOutputSize(outputWidth, outputHeight) || outputWidth <= 0 || outputHeight <= 0)
    {
        return;
    }
    const float fitScale = std::min(outputWidth / bounds.w, outputHeight / bounds.h);
    const float scale = fitScale * _viewScale;
    if (!std::isfinite(scale) || scale <= 0.0f)
    {
        return;
    }
    const auto positionsX = Entities::GetInstance().GetXPositions();
    const auto positionsY = Entities::GetInstance().GetYPositions();
    const float focusX = positionsX.size() > Entities::PlayerIndex
        ? positionsX[Entities::PlayerIndex] : bounds.x + bounds.w * 0.5f;
    const float focusY = positionsY.size() > Entities::PlayerIndex
        ? positionsY[Entities::PlayerIndex] : bounds.y + bounds.h * 0.5f;
    const auto offset = [scale](float origin, float length, float output, float focus)
    {
        const float visibleLength = output / scale;
        if (visibleLength >= length)
        {
            return (output - length * scale) * 0.5f - origin * scale;
        }
        const float cameraOrigin = std::clamp(focus - visibleLength * 0.5f,
            origin, origin + length - visibleLength);
        return -cameraOrigin * scale;
    };
    const RenderView view{ scale, offset(bounds.x, bounds.w, static_cast<float>(outputWidth), focusX),
        offset(bounds.y, bounds.h, static_cast<float>(outputHeight), focusY) };
    _level.render(renderer, view, [&renderer](std::size_t layer, float scale, float offsetX, float offsetY)
    {
        Entities::GetInstance().RenderLayer(layer, renderer, scale, offsetX, offsetY);
    });
#ifdef DEBUG
    if (_drawCollisions)
    {
        Entities::GetInstance().DrawCollisions(renderer, view);
    }
#endif
    renderTexts();
}

void MapScene::renderTexts()
{
    //Renderer::getInstance().drawText(_textFont, _textText, TextInfo, _stringsThatUpdated);
    // Render all the texts that need to be updated


    //_stringsThatUpdated = false;
}

void MapScene::updateStrings()
{
  /** Update the strings you need, verify if they change to save on performance 
    if (_oldString != _newString) {
        _oldString = _newString;
        _stringText = std::format("{}{}", Text.baseText, stringValue);
        _stringsThatUpdated = true;
    }
  */
}

void MapScene::cleanup()
{
    Entities::GetInstance().ClearEntities();
    _level.clear();
    _doorSprite.reset();
#ifdef DEBUG
    _drawCollisions = false;
#endif
}

std::string MapScene::getName() const
{
    return _sceneName;
}

void MapScene::setViewScale(float scale)
{
    if (!std::isfinite(scale) || scale <= 0.0f)
    {
        throw std::invalid_argument("Map view scale must be finite and positive");
    }
    _viewScale = scale;
}

float MapScene::getViewScale() const noexcept
{
    return _viewScale;
}

std::unique_ptr<Scene> MapScene::createExitScene() const
{
    return {};
}
