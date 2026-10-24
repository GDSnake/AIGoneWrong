#pragma once

#include "MenuScene.h"

class Credits : public MenuScene
{
public:
    Credits();

private:
    void setup() override;
    void activate(std::size_t index) override;
    void back() override;
};