#include "Room.h"

#include <stdexcept>
#include <utility>

#include "AssetManagers/AudioManager.h"
#include "AssetManagers/FontsManager.h"
#include "AssetManagers/SpriteManager.h"

#include "Core/Renderer.h"
#include "Entities/Entities.h"
#include "Entities/Sprite.h"

void Room::init()
{
    _input = std::make_unique<InputManager>();
    SpriteManager::getInstance();
    Renderer::getInstance();

    if (!_level.load("assets/Maps/room.json", Renderer::getInstance()))
    {
        _input->quit();
        return;
    }

    const auto spawn = _level.getSpawn("PlayerSpawn");
    if (!spawn)
    {
        SDL_Log("Room map is missing the PlayerSpawn point");
        _input->quit();
        return;
    }
    try
    {
        Entities::GetInstance().InitializePhysics(_level.getCollisions(), _level.getBounds());
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
    }
    catch (const std::exception& error)
    {
        SDL_Log("Unable to initialize room entities: %s", error.what());
        _input->quit();
        return;
    }

    _titleText = std::format("{}{}",TitleFontInfo.baseText, "Title");
    //Add all the texts you need for the scene

    _titleFont = FontsManager::getInstance().getFont(TitleFontInfo.fontName);
    //Add all the fonts you need for the scene
}

void Room::handleEvents(const SDL_Event& event)
{
    _input->handleEvent(event);
#ifdef DEBUG
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_M && !event.key.repeat)
    {
        _drawCollisions = !_drawCollisions;
    }
#endif
}

bool Room::shouldQuit() const
{
    return _input->shouldQuit();
}

void Room::handleInput()
{
    if (_input->isKeyPressed(SDL_SCANCODE_ESCAPE))
    {
        _input->quit();
    }
}

void Room::update(float deltaTime)
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
    }
    Entities::GetInstance().Update(deltaTime);
    _input->update();
}

void Room::render()
{
    Renderer& renderer = Renderer::getInstance();
    renderer.setDrawColor({ 0, 0, 0, 255 });
    renderer.clear();
    _level.render(renderer, [&renderer](std::size_t layer, float scale, float offsetX, float offsetY)
    {
        Entities::GetInstance().RenderLayer(layer, renderer, scale, offsetX, offsetY);
    });
#ifdef DEBUG
    if (_drawCollisions)
    {
        Entities::GetInstance().DrawCollisions(renderer, _level.getBounds());
    }
#endif
    renderTexts();
}

void Room::renderTexts()
{
    //Renderer::getInstance().drawText(_textFont, _textText, TextInfo, _stringsThatUpdated);
    // Render all the texts that need to be updated


    //_stringsThatUpdated = false;
}

void Room::updateStrings()
{
  /** Update the strings you need, verify if they change to save on performance 
    if (_oldString != _newString) {
        _oldString = _newString;
        _stringText = std::format("{}{}", Text.baseText, stringValue);
        _stringsThatUpdated = true;
    }
  */
}

void Room::cleanup()
{
    Entities::GetInstance().ClearEntities();
    _level.clear();
#ifdef DEBUG
    _drawCollisions = false;
#endif
}

std::string Room::getName() const
{
    return "Room";
}
