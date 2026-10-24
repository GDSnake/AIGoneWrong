#include "Room.h"

#include "Level.h"

Room::Room()
    : MapScene("assets/Maps/room.json", "Room")
{
}

std::unique_ptr<Scene> Room::createExitScene() const
{
    return std::make_unique<Level>(1, 2.0f);
}
