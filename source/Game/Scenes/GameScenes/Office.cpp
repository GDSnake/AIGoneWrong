#include "Office.h"

#include <algorithm>
#include <cmath>

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
    _dialogDelay = 0.0f;
    _introDialogShown = false;
    MapScene::init();
    if (!shouldQuit())
    {
        AudioManager::getInstance().playMusic(AudioTrack::Office);
    }
}

void Office::update(float deltaTime)
{
    if (!shouldQuit() && !isPaused() && !isTransitioning() && !_introDialogShown &&
        std::isfinite(deltaTime) && deltaTime > 0.0f)
    {
        _dialogDelay += std::min(deltaTime, 0.25f);
        if (_dialogDelay >= 1.0f)
        {
            _introDialogShown = true;
            const std::string characterName = _pizzaEaten ? "Anthony Ivalez" : "Anders Issakson";
            showDialog({
                { characterName, "Finally I've arrived...well I left a good music playing, reminds me that time at the beach...after that cruise" },
                { characterName, "Well let's finish this up, just need to find my computer and shut the robots down" } });
        }
    }
    MapScene::update(deltaTime);
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
    scene->setPizzaEaten(_pizzaEaten);
    return scene;
}

bool Office::isPlayerControlEnabled() const
{
    return _introDialogShown;
}