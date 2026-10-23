#pragma once

#include <memory>
#include <string>

#include "Config/Config.h"
#include "Core/TiledLevelLoader.h"
#include "Game/Scenes/Scene.h"
#include "Input/InputManager.h"

struct TTF_Font;

class Room : public Scene
{
public:
    void init() override;
    void handleInput() override;
    void update(float deltaTime) override;
    void cleanup() override;
    std::string getName() const override;
    
    bool shouldQuit() const;
    std::unique_ptr<Scene> getNextScene() override;
    void handleEvents(const SDL_Event& event) override;

private:
    void render() override;
    void renderTexts();
    
    void updateStrings();

    std::unique_ptr<InputManager> _input;
    TiledLevelLoader _level;
    const ConfigData& _configData = Config::getInstance().getConfigData();

#ifdef DEBUG
    bool _drawCollisions = false;
#endif

    // Text
    TTF_Font* _titleFont = nullptr;
    std::string _titleText;
};