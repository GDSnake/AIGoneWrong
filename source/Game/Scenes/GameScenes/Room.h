#pragma once

#include "MapScene.h"

class Room : public MapScene
{
public:
    Room();
    void init() override;
    void update(float deltaTime) override;
    void cleanup() override;

protected:
    std::unique_ptr<Scene> createExitScene() const override;
    std::unique_ptr<Scene> createRestartScene() const override;
    bool isPlayerControlEnabled() const override;

private:
    float _dialogDelay = 0.0f;
    bool _testDialogShown = false;
};