#pragma once

#include "MapScene.h"

class Office : public MapScene
{
public:
    Office();
    void init() override;
    void update(float deltaTime) override;
    void cleanup() override;
    void setPizzaEaten(bool eaten) noexcept { _pizzaEaten = eaten; }

protected:
    std::unique_ptr<Scene> createExitScene() const override;
    std::unique_ptr<Scene> createRestartScene() const override;
    bool isPlayerControlEnabled() const override;

private:
    mutable bool _victoryMusicHandoff = false;
    float _dialogDelay = 0.0f;
    bool _introDialogShown = false;
    bool _pizzaEaten = false;
};