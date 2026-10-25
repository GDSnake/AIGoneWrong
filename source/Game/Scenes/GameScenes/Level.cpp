#include "Level.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

#include "AssetManagers/AudioManager.h"
#include "Entities/Entities.h"
#include "Game/Scenes/Menus/MainMenu.h"

namespace
{
    std::filesystem::path levelMapPath(int index)
    {
        if (index <= 0)
        {
            throw std::invalid_argument("Level index must be positive");
        }
        return std::filesystem::path("assets/Maps") / std::format("Level_{:02}.json", index);
    }
}

Level::Level(int index, float viewScale)
    : MapScene(levelMapPath(index), std::format("Level_{:02}", index), viewScale), _index(index)
{
}

void Level::init()
{
    _dialogDelay = 0.0f;
    _introDialogShown = false;
    MapScene::init();
    if (_index == 1 && !shouldQuit())
    {
        Entities::GetInstance().SetRobotPersonalityInterval(1.0f, 1.0f);
        AudioManager::getInstance().playMusic(AudioTrack::Level1);
    }
}

void Level::update(float deltaTime)
{
    if (_index == 1 && !shouldQuit() && !isPaused() && !isTransitioning() && !_introDialogShown &&
        std::isfinite(deltaTime) && deltaTime > 0.0f)
    {
        _dialogDelay += std::min(deltaTime, 0.25f);
        if (_dialogDelay >= 1.0f)
        {
            _introDialogShown = true;
            showDialog({ { "#generate# Ivalez", "Test text" } });
        }
    }
    MapScene::update(deltaTime);
}

void Level::cleanup()
{
    if (_index == 1)
    {
        AudioManager::getInstance().stopMusic();
    }
    MapScene::cleanup();
}

int Level::getIndex() const noexcept
{
    return _index;
}

bool Level::isPlayerControlEnabled() const
{
    return _index != 1 || _introDialogShown;
}

std::unique_ptr<Scene> Level::createExitScene() const
{
    return _index == 1 ? std::make_unique<MainMenu>() : MapScene::createExitScene();
}

std::unique_ptr<Scene> Level::createRestartScene() const
{
    return std::make_unique<Level>(_index, getViewScale());
}