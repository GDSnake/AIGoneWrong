#pragma once

#include "MenuScene.h"

class MainMenu : public MenuScene
{
public:
    MainMenu();
    void cleanup() override;

private:
    void setup() override;
    void activate(std::size_t index) override;
};