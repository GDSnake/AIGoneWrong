#include "Office.h"

#include "AssetManagers/AudioManager.h"
#include "Config/Config.h"
#include "Game/Scenes/Menus/MainMenu.h"

Office::Office()
    : MapScene("assets/Maps/Office.json", "Office", Config::getInstance().getViewScale("Office"))
{
}

void Office::init()
{
    MapScene::init();
    if (!shouldQuit())
    {
        AudioManager::getInstance().playMusic(AudioTrack::Office);
    }
}

void Office::cleanup()
{
    AudioManager::getInstance().stopMusic();
    MapScene::cleanup();
}

std::unique_ptr<Scene> Office::createExitScene() const
{
    return std::make_unique<MainMenu>();
}

std::unique_ptr<Scene> Office::createRestartScene() const
{
    auto scene = std::make_unique<Office>();
    scene->setViewScale(getViewScale());
    return scene;
}