#pragma once

#include <utility>
#include <vector>

#include "MenuScene.h"

class SettingsMenu : public MenuScene
{
public:
    SettingsMenu();

private:
    void setup() override;
    void activate(std::size_t index) override;
    void adjust(std::size_t index, int direction) override;
    std::string value(std::size_t index) const override;
    void back() override;
    void apply();

    std::vector<std::pair<int, int>> _resolutions;
    std::size_t _resolution = 0;
    bool _fullscreen = false;
};