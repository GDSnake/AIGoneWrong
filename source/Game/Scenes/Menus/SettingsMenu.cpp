#include "SettingsMenu.h"

#include <algorithm>
#include <format>

#include "MainMenu.h"
#include "Config/Config.h"
#include "Core/Renderer.h"

SettingsMenu::SettingsMenu() : MenuScene("Settings", "SettingsMenu") {}

void SettingsMenu::setup()
{
    const auto& config = Config::getInstance().getConfigData();
    _fullscreen = config.fullscreen;
    _resolutions = { { 960, 540 }, { 1280, 720 }, { 1600, 900 }, { 1920, 1080 } };
    const std::pair current{ config.width, config.height };
    if (std::find(_resolutions.begin(), _resolutions.end(), current) == _resolutions.end())
    {
        _resolutions.push_back(current);
    }
    _resolution = static_cast<std::size_t>(std::find(_resolutions.begin(), _resolutions.end(), current) - _resolutions.begin());
    _items = { { "Resolution", ItemKind::Choice }, { "Fullscreen", ItemKind::Toggle }, { "Apply" }, { "Back" } };
}

void SettingsMenu::activate(std::size_t index)
{
    switch (index)
    {
        case 0: adjust(index, 1); break;
        case 1: adjust(index, 1); break;
        case 2: apply(); break;
        case 3: back(); break;
    }
}

void SettingsMenu::adjust(std::size_t index, int direction)
{
    if (index == 0)
    {
        _resolution = direction < 0 ? (_resolution + _resolutions.size() - 1) % _resolutions.size() :
            (_resolution + 1) % _resolutions.size();
    }
    else if (index == 1)
    {
        _fullscreen = !_fullscreen;
    }
    else
    {
        return;
    }
    setStatus({});
}

std::string SettingsMenu::value(std::size_t index) const
{
    if (index == 0)
    {
        const auto [width, height] = _resolutions[_resolution];
        return std::format("{} x {}", width, height);
    }
    return index == 1 && _fullscreen ? "on" : "off";
}

void SettingsMenu::apply()
{
    auto& config = Config::getInstance();
    const auto& previous = config.getConfigData();
    const int oldWidth = previous.width;
    const int oldHeight = previous.height;
    const bool oldFullscreen = previous.fullscreen;
    const auto [width, height] = _resolutions[_resolution];
    config.setWindowSettings(width, height, _fullscreen);
    if (!Renderer::getInstance().applyWindowSettings())
    {
        config.setWindowSettings(oldWidth, oldHeight, oldFullscreen);
        Renderer::getInstance().applyWindowSettings();
        setStatus("Unable to apply display settings.", true);
        return;
    }
    const bool saved = config.saveConfig();
    setStatus(saved ? "Settings saved." : "Settings applied; saving failed.", !saved);
}

void SettingsMenu::back()
{
    transitionTo(std::make_unique<MainMenu>());
}