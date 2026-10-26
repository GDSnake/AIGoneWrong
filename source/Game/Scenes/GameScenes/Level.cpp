#include "Level.h"
#include "Office.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

#include "AssetManagers/AudioManager.h"
#include "Config/Config.h"
#include "Entities/Entities.h"

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

Level::Level(int index)
    : Level(index, Config::getInstance().getViewScale(std::format("Level_{:02}", index)))
{
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
    else if (_index == 2 && !shouldQuit())
    {
        AudioManager::getInstance().playMusic(AudioTrack::Level2);
    }
    else if (_index == 3 && !shouldQuit())
    {
        AudioManager::getInstance().playMusic(AudioTrack::Level3);
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
            const std::string characterName = _pizzaEaten ? "Anthony Ivalez" : "#generate# Ivalez";
            showDialog({
                { characterName, _pizzaEaten
                    ? "That pizza tasted good...Oh I've remember my name, well nevermind..."
                    : "I'm hungry...I should have ate that pizza... I still don't remember fully my name" },
                { characterName, "Something odd looks like it's happening with the robots.\nThey are outside of their usual place." },
                { characterName, "Did something went wrong with their AI?" } });
        }
    }
    MapScene::update(deltaTime);
}

void Level::cleanup()
{
    if (_index == 1 || _index == 2 || _index == 3)
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
    if (std::filesystem::is_regular_file(levelMapPath(_index + 1)))
    {
        auto scene = std::make_unique<Level>(_index + 1);
        scene->setPizzaEaten(_pizzaEaten);
        return scene;
    }
    return std::make_unique<Office>();
}

std::unique_ptr<Scene> Level::createRestartScene() const
{
    auto scene = std::make_unique<Level>(_index, getViewScale());
    scene->setPizzaEaten(_pizzaEaten);
    return scene;
}