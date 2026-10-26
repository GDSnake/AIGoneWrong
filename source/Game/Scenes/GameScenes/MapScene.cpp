#include "MapScene.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "AssetManagers/AudioManager.h"
#include "AssetManagers/FontsManager.h"
#include "AssetManagers/SpriteManager.h"

#include "Config/Config.h"
#include "Core/Renderer.h"
#include "Entities/Entities.h"
#include "Entities/Sprite.h"
#include "Game/Scenes/Menus/MainMenu.h"

#ifdef DEBUG
#include "Level.h"
#include "Room.h"
#endif

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
        Entities::GetInstance().InitializePhysics(_level.getCollisions(), _level.getBounds(), _level.getFrameCollisions(),
            _level.getTileSize(), _level.getFloorTiles());
        std::vector<std::shared_ptr<Sprite>> personalityFrames;
        for (int frame = 1; frame <= 7; ++frame)
        {
            auto effect = SpriteManager::getInstance().getSprite(std::format("robot-change-{}", frame));
            if (!effect)
            {
                throw std::runtime_error("Robot personality-change animation is missing");
            }
            Renderer::getInstance().configureTexture(effect->getTexture().get());
            personalityFrames.push_back(std::move(effect));
        }
        Entities::GetInstance().SetRobotPersonalityEffect(std::move(personalityFrames));
        std::vector<std::shared_ptr<Sprite>> teleportFrames;
        for (int frame = 1; frame <= 7; ++frame)
        {
            auto effect = SpriteManager::getInstance().getSprite(std::format("robot-teleport-{}", frame));
            if (!effect)
            {
                throw std::runtime_error("Robot teleport animation is missing");
            }
            Renderer::getInstance().configureTexture(effect->getTexture().get());
            teleportFrames.push_back(std::move(effect));
        }
        Entities::GetInstance().SetRobotTeleportEffect(std::move(teleportFrames));
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
            .colliderWidth = 12.0f * Entities::PlayerScale,
            .colliderHeight = 6.0f * Entities::PlayerScale,
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
            const RobotProfile& profile = Config::getInstance().getRobotProfile(robot.robot.type);
            auto sprite = SpriteManager::getInstance().getSprite(profile.spriteName);
            auto movingSprite = SpriteManager::getInstance().getSprite(profile.movingSpriteName);
            if (!sprite || !movingSprite)
            {
                throw std::runtime_error("Missing robot sprite: " + std::string(profile.spriteName));
            }
            Renderer::getInstance().configureTexture(sprite->getTexture().get());
            Renderer::getInstance().configureTexture(movingSprite->getTexture().get());
            const std::size_t firstFrame = RobotIdleRows[static_cast<std::size_t>(robot.robot.direction)] * 8;
            Entities::GetInstance().AddEntity({
                .x = robot.position.x,
                .y = robot.position.y,
                .sprite = std::move(sprite),
                .visible = true,
                .name = robot.name,
                .collidable = true,
                .colliderWidth = profile.colliderWidth * profile.spriteScale,
                .colliderHeight = profile.colliderHeight * profile.spriteScale,
                .animation = { firstFrame, 8, firstFrame, 8, 0.12f, 0.12f },
                .behavior = EntityBehavior::Robot,
                .movementSpeed = 0.0f,
                .renderLayer = robot.renderLayer,
                .robot = robot.robot,
                .robotMovingSprite = std::move(movingSprite)
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
    if (!_dialog.init(Renderer::getInstance(), _titleFont))
    {
        _input->quit();
    }
    //Add all the fonts you need for the scene
}

void MapScene::handleEvents(const SDL_Event& event)
{
    if (_gameOver)
    {
        if (!isTransitioning())
        {
            _gameOver->handleEvents(event);
        }
        return;
    }
    if (_pause)
    {
        if (!isTransitioning())
        {
            _pause->handleEvents(event);
        }
        return;
    }
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.scancode == SDL_SCANCODE_ESCAPE)
    {
        if (!isTransitioning())
        {
            _pause = std::make_unique<PauseMenu>();
            _pause->init();
            auto& audio = AudioManager::getInstance();
            _resumeMusicAfterPause = audio.isMusicPlaying();
            audio.pauseMusic();
            *_input = {};
        }
        return;
    }
    _input->handleEvent(event);
    if (_dialog.isActive() && !isTransitioning() && event.type == SDL_EVENT_KEY_DOWN &&
        !event.key.repeat && event.key.scancode == SDL_SCANCODE_SPACE)
    {
        _dialog.advance();
    }
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

bool MapScene::isPaused() const noexcept
{
    return static_cast<bool>(_pause) || static_cast<bool>(_gameOver);
}

void MapScene::handleInput()
{
    if (_gameOver && !isTransitioning())
    {
        _gameOver->handleInput();
    }
    else if (_pause && !isTransitioning())
    {
        _pause->handleInput();
    }
}

void MapScene::update(float deltaTime)
{
    if (_input->shouldQuit())
    {
        return;
    }
    if (_gameOver)
    {
        if (!isTransitioning())
        {
            _gameOver->update(deltaTime);
            switch (_gameOver->consumeAction())
            {
                case GameOverAction::Restart: transitionTo(createRestartScene()); break;
                case GameOverAction::MainMenu: transitionTo(std::make_unique<MainMenu>()); break;
                case GameOverAction::None: break;
            }
        }
        return;
    }
    if (_pause)
    {
        if (!isTransitioning())
        {
            _pause->update(deltaTime);
            switch (_pause->consumeAction())
            {
                case PauseAction::Resume:
                    _pause->cleanup();
                    _pause.reset();
                    if (_resumeMusicAfterPause)
                    {
                        AudioManager::getInstance().resumeMusic();
                    }
                    _resumeMusicAfterPause = false;
                    *_input = {};
                    break;
                case PauseAction::MainMenu:
                    transitionTo(std::make_unique<MainMenu>());
                    break;
                case PauseAction::Exit:
                    requestExit();
                    break;
#ifdef DEBUG
                case PauseAction::DebugLevel1:
                    transitionTo(std::make_unique<Level>(1));
                    break;
                case PauseAction::DebugLevel2:
                    transitionTo(std::make_unique<Level>(2));
                    break;
                case PauseAction::DebugRoom:
                    transitionTo(std::make_unique<Room>());
                    break;
#endif
                case PauseAction::None:
                    break;
            }
        }
        return;
    }
    const bool playerInputEnabled = !isTransitioning() && isPlayerControlEnabled() && !_dialog.isActive();
    if (isTransitioning())
    {
        Entities::GetInstance().SetVelocity(Entities::PlayerIndex, 0.0f, 0.0f);
    }
    else
    {
        Entities::GetInstance().UpdateControllers(*_input, playerInputEnabled);
        for (const DoorVisualChange& change : Entities::GetInstance().GetDoorVisualChanges())
        {
            _level.setDoorFrame(change.visualIndex, _doorSprite->getTexture().get(),
                _doorSprite->getSourceRect(change.frameIndex));
        }
    }
    Entities::GetInstance().Update(deltaTime, playerInputEnabled);
    if (playerInputEnabled && Entities::GetInstance().ConsumePlayerCaught())
    {
        Entities::GetInstance().SetVelocity(Entities::PlayerIndex, 0.0f, 0.0f);
        AudioManager::getInstance().stopMusic();
        _gameOver = std::make_unique<GameOverMenu>();
        _gameOver->init();
        *_input = {};
    }
    else if (playerInputEnabled)
    {
        const auto endTrigger = _level.getSpawn("triggerEndLevel");
        bool reachedEnd = false;
        if (endTrigger)
        {
            const float distanceX = Entities::GetInstance().GetXPositions()[Entities::PlayerIndex] - endTrigger->position.x;
            const float distanceY = Entities::GetInstance().GetYPositions()[Entities::PlayerIndex] - endTrigger->position.y;
            constexpr float triggerRadius = 16.0f;
            reachedEnd = distanceX * distanceX + distanceY * distanceY <= triggerRadius * triggerRadius;
        }
        if (Entities::GetInstance().ConsumeSceneExitRequest() || reachedEnd)
        {
            auto nextScene = createExitScene();
            const bool requested = nextScene ? transitionTo(std::move(nextScene)) : requestExit();
            if (requested)
            {
                Entities::GetInstance().SetVelocity(Entities::PlayerIndex, 0.0f, 0.0f);
            }
        }
    }
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
    _level.render(renderer, view, [&renderer](std::size_t layer, float scale, float offsetX, float offsetY, std::span<const WorldSprite> props)
    {
        Entities::GetInstance().RenderLayer(layer, renderer, scale, offsetX, offsetY, props);
    });
#ifdef DEBUG
    if (_drawCollisions)
    {
        Entities::GetInstance().DrawCollisions(renderer, view);
    }
#endif
    renderTexts();
    _dialog.render(renderer);
    if (_pause)
    {
        _pause->render();
    }
    if (_gameOver)
    {
        _gameOver->render();
    }
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
    if (_gameOver)
    {
        _gameOver->cleanup();
        _gameOver.reset();
    }
    if (_pause)
    {
        _pause->cleanup();
        _pause.reset();
    }
    _resumeMusicAfterPause = false;
    _dialog.cleanup();
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

bool MapScene::isPlayerControlEnabled() const
{
    return true;
}

void MapScene::showDialog(std::vector<DialogLine> lines)
{
    _dialog.show(std::move(lines));
    Entities::GetInstance().SetVelocity(Entities::PlayerIndex, 0.0f, 0.0f);
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
