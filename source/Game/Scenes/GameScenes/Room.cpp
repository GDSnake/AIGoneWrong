#include "Room.h"

#include <algorithm>
#include <cmath>

#include "Level.h"
#include "AssetManagers/AudioManager.h"

Room::Room()
    : MapScene("assets/Maps/room.json", "Room")
{
}

void Room::init()
{
    _dialogDelay = 0.0f;
    _testDialogShown = false;
    MapScene::init();
    if (!shouldQuit())
    {
        AudioManager::getInstance().playMusic(AudioTrack::Room);
    }
}

void Room::update(float deltaTime)
{
    if (!shouldQuit() && !isTransitioning() && !_testDialogShown && std::isfinite(deltaTime) && deltaTime > 0.0f)
    {
        _dialogDelay += std::min(deltaTime, 0.25f);
        if (_dialogDelay >= 2.0f)
        {
            _testDialogShown = true;
            showDialog({ { "#generate_character_name#", "Test text" } });
        }
    }
    MapScene::update(deltaTime);
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
    return std::make_unique<Level>(1, 2.0f);
}
