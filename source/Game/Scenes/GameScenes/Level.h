#pragma once

#include "MapScene.h"

class Level : public MapScene
{
public:
    explicit Level(int index);
    int getIndex() const noexcept;

private:
    int _index;
};