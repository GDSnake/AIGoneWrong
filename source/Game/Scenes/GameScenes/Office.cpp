#include "Office.h"

#include "AssetManagers/AudioManager.h"
#include "Config/Config.h"
#include "Game/Scenes/Menus/Victory.h"

Office::Office()
    : MapScene("assets/Maps/Office.json", "Office", Config::getInstance().getViewScale("Office"))
{
}

void Office::init()
{
    _victoryMusicHandoff = false;
    MapScene::init();
    if (!shouldQuit())
    {
        AudioManager::getInstance().playMusic(AudioTrack::Office);
    }
}

void Office::cleanup()
{
    if (!_victoryMusicHandoff)
    {
        AudioManager::getInstance().stopMusic();
    }
    MapScene::cleanup();
}

std::unique_ptr<Scene> Office::createExitScene() const
{
    auto scene = std::make_unique<Victory>();
    _victoryMusicHandoff = true;
    return scene;
}

std::unique_ptr<Scene> Office::createRestartScene() const
{
    auto scene = std::make_unique<Office>();
    scene->setViewScale(getViewScale());
    return scene;
}