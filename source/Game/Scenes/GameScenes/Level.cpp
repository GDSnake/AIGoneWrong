#include "Level.h"

#include <format>
#include <stdexcept>

#include "AssetManagers/AudioManager.h"
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
    MapScene::init();
    if (_index == 1 && !shouldQuit())
    {
        AudioManager::getInstance().playMusic(AudioTrack::Level1);
    }
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

std::unique_ptr<Scene> Level::createExitScene() const
{
    return _index == 1 ? std::make_unique<MainMenu>() : MapScene::createExitScene();
}