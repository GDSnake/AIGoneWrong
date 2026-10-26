#include "Room.h"

#include <algorithm>
#include <cmath>

#include "Level.h"
#include "AssetManagers/AudioManager.h"
#include "Config/Config.h"
#include "Entities/Entities.h"

Room::Room()
    : MapScene("assets/Maps/room.json", "Room", Config::getInstance().getViewScale("Room"))
{
}

void Room::init()
{
    _dialogDelay = 0.0f;
    _testDialogShown = false;
    MapScene::init();
    if (!shouldQuit())
    {
        if (_pizzaEaten)
        {
            if (const auto pizza = getMap().getSpawn("pizzaEat")) getMap().hideTileAt(pizza->position);
        }
        Entities::GetInstance().SetPlayerSpeech("Zzz", 1.0f);
        AudioManager::getInstance().playMusic(AudioTrack::Room);
    }
}

void Room::update(float deltaTime)
{
    if (!shouldQuit() && !isPaused() && !isTransitioning() && !_testDialogShown && std::isfinite(deltaTime) && deltaTime > 0.0f)
    {
        _dialogDelay += std::min(deltaTime, 0.25f);
        if (_dialogDelay >= 1.0f)
        {
            _testDialogShown = true;
            Entities::GetInstance().ClearPlayerSpeech();
            showDialog({
                { "#generate_name#", "Ummm... ahhh...\nI feel sleepy today" },
                { "#generate_name#", "That was such an odd dream...\nAI taking control of things and making me..." },
                { "#generate_name#", "...forget my name...oh boy...\nI don't remember..." },
                { "#generate_name#", "Odd thing...well...just grab yesterday's\nPizza and go to work..." }
            });
        }
    }
    MapScene::update(deltaTime);
    if (!_pizzaEaten && isGameplayActive())
    {
        if (const auto pizza = getMap().getSpawn("pizzaEat"))
        {
            const auto& entities = Entities::GetInstance();
            const float distanceX = entities.GetXPositions()[Entities::PlayerIndex] - pizza->position.x;
            const float distanceY = entities.GetYPositions()[Entities::PlayerIndex] - pizza->position.y;
            constexpr float eatRadius = 16.0f;
            if (distanceX * distanceX + distanceY * distanceY <= eatRadius * eatRadius && getMap().hideTileAt(pizza->position))
            {
                _pizzaEaten = true;
                Entities::GetInstance().SetPlayerSpeech("Yummy!", 1.0f);
            }
        }
    }
}

void Room::cleanup()
{
    AudioManager::getInstance().stopMusic();
    MapScene::cleanup();
}

bool Room::isPlayerControlEnabled() const
{
    return _testDialogShown;
}

std::unique_ptr<Scene> Room::createExitScene() const
{
    auto scene = std::make_unique<Level>(1);
    scene->setPizzaEaten(_pizzaEaten);
    return scene;
}

std::unique_ptr<Scene> Room::createRestartScene() const
{
    auto scene = std::make_unique<Room>();
    scene->setViewScale(getViewScale());
    scene->_pizzaEaten = _pizzaEaten;
    return scene;
}
