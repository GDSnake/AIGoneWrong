#pragma once

#include "MapScene.h"

class Room : public MapScene
{
public:
    Room();
    void init() override;
    void cleanup() override;

protected:
    std::unique_ptr<Scene> createExitScene() const override;
};