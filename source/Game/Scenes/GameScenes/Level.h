#pragma once

#include "MapScene.h"

class Level : public MapScene
{
public:
    explicit Level(int index, float viewScale = 1.0f);
    void init() override;
    void update(float deltaTime) override;
    void cleanup() override;
    int getIndex() const noexcept;

protected:
    std::unique_ptr<Scene> createExitScene() const override;
    bool isPlayerControlEnabled() const override;

private:
    int _index;
    float _dialogDelay = 0.0f;
    bool _introDialogShown = false;
};