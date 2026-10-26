#pragma once

#include <memory>
#include <string>

#include "Config/Config.h"
#include "Core/TiledLevelLoader.h"
#include "Game/Scenes/Scene.h"
#include "Game/Scenes/Menus/GameOverMenu.h"
#include "Game/Scenes/Menus/PauseMenu.h"
#include "Game/UI/DialogSystem.h"
#include "Input/InputManager.h"

struct TTF_Font;
class Sprite;

class MapScene : public Scene
{
public:
    void init() override;
    void handleInput() override;
    void update(float deltaTime) override;
    void cleanup() override;
    std::string getName() const override;
    void setViewScale(float scale);
    float getViewScale() const noexcept;
    
    bool shouldQuit() const;
    bool isPaused() const noexcept override;
    void handleEvents(const SDL_Event& event) override;

protected:
    MapScene(std::filesystem::path mapPath, std::string sceneName, float viewScale = 1.0f);
    virtual std::unique_ptr<Scene> createExitScene() const;
    virtual std::unique_ptr<Scene> createRestartScene() const = 0;
    virtual bool isPlayerControlEnabled() const;
    bool isGameplayActive() const;
    TiledLevelLoader& getMap() noexcept { return _level; }
    void showDialog(std::vector<DialogLine> lines);

private:
    void render() override;
    void renderTexts();
    
    void updateStrings();

    std::unique_ptr<InputManager> _input;
    TiledLevelLoader _level;
    DialogSystem _dialog;
    std::unique_ptr<PauseMenu> _pause;
    std::unique_ptr<GameOverMenu> _gameOver;
    bool _resumeMusicAfterPause = false;
    std::shared_ptr<Sprite> _doorSprite;
    std::filesystem::path _mapPath;
    std::string _sceneName;
    float _viewScale = 1.0f;
    const ConfigData& _configData = Config::getInstance().getConfigData();

#ifdef DEBUG
    bool _drawCollisions = false;
#endif

    // Text
    TTF_Font* _titleFont = nullptr;
    TTF_Font* _dialogFont = nullptr;
    std::string _titleText;
};