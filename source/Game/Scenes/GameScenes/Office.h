#pragma once

#include "MapScene.h"

class Office : public MapScene
{
public:
    Office();
    void init() override;
    void cleanup() override;

protected:
    std::unique_ptr<Scene> createExitScene() const override;
    std::unique_ptr<Scene> createRestartScene() const override;

private:
    mutable bool _victoryMusicHandoff = false;
};