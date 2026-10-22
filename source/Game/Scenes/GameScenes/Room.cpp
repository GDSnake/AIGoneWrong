#include "Room.h"

#include "AssetManagers/AudioManager.h"
#include "AssetManagers/FontsManager.h"
#include "AssetManagers/SpriteManager.h"

#include "Core/Renderer.h"

void Room::init()
{
    _input = std::make_unique<InputManager>();
    SpriteManager::getInstance();
    Renderer::getInstance();

    if (!_level.load("assets/Maps/room.json", Renderer::getInstance().getRenderer()))
    {
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
    handleInput();
    _input->update();

    render();

    renderTexts();

    Renderer::getInstance().present();
}

void Room::render()
{
    Renderer& renderer = Renderer::getInstance();
    renderer.setDrawColor({ 0, 0, 0, 255 });
    renderer.clear();
    _level.render(renderer.getRenderer());
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
    _level.clear();
}

std::string Room::getName() const
{
    return "Room";
}

std::unique_ptr<Scene> Room::getNextScene()
{
    return {};
}