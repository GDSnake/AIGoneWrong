#pragma once

#include "MenuScene.h"

class Credits : public MenuScene
{
public:
    Credits();
    void init() override;
    void cleanup() override;

private:
    void setup() override;
    void activate(std::size_t index) override;
    void back() override;
};