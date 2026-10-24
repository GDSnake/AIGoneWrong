#include "Room.h"

#include "Level.h"
#include "AssetManagers/AudioManager.h"

Room::Room()
    : MapScene("assets/Maps/room.json", "Room")
{
}

void Room::init()
{
    MapScene::init();
    if (!shouldQuit())
    {
        AudioManager::getInstance().playMusic(AudioTrack::Room);
    }
}

void Room::cleanup()
{
    AudioManager::getInstance().stopMusic();
    MapScene::cleanup();
}

std::unique_ptr<Scene> Room::createExitScene() const
{
    return std::make_unique<Level>(1, 2.0f);
}
