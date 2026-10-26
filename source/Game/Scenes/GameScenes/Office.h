#pragma once

#include "MapScene.h"

class Office : public MapScene
{
public:
    Office();

protected:
    std::unique_ptr<Scene> createExitScene() const override;
    std::unique_ptr<Scene> createRestartScene() const override;
};