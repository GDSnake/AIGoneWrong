#pragma once

#include "MapScene.h"

class Room : public MapScene
{
public:
    Room();

protected:
    std::unique_ptr<Scene> createExitScene() const override;
};