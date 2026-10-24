#pragma once

#include "MapScene.h"

class Level : public MapScene
{
public:
    explicit Level(int index, float viewScale = 1.0f);
    int getIndex() const noexcept;

private:
    int _index;
};