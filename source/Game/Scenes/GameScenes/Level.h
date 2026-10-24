#pragma once

#include "MapScene.h"

class Level : public MapScene
{
public:
    explicit Level(int index, float viewScale = 1.0f);
    void init() override;
    void cleanup() override;
    int getIndex() const noexcept;

protected:
    std::unique_ptr<Scene> createExitScene() const override;

private:
    int _index;
};