#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Game/Scenes/Scene.h"
#include "Input/InputManager.h"

class Sprite;
struct TTF_Font;

class MenuScene : public Scene
{
public:
    void init() override;
    void handleEvents(const SDL_Event& event) override;
    void handleInput() override;
    void update(float deltaTime) override;
    void render() override;
    void cleanup() override;
    std::string getName() const override;
    bool shouldQuit() const override;

protected:
    enum class ItemKind { Action, Choice, Toggle };
    struct Item
    {
        std::string label;
        ItemKind kind = ItemKind::Action;
    };

    MenuScene(std::string title, std::string sceneName);
    virtual void setup() = 0;
    virtual void activate(std::size_t index) = 0;
    virtual void adjust(std::size_t index, int direction);
    virtual std::string value(std::size_t index) const;
    virtual void back();
    virtual SDL_FRect itemBounds(std::size_t index) const;
    virtual SDL_FRect contentBounds(int width, int height) const;
    virtual void renderContent(const SDL_FRect& bounds) const;
    void selectItem(std::size_t index);
    void resetInputState();
    void drawLabel(const std::string& text, const SDL_FRect& bounds, const SDL_Color& color, float scale) const;
    void setStatus(std::string message, bool error = false);

    std::vector<Item> _items;
    std::vector<std::string> _details;
    bool _itemsAtBottom = false;
    bool _isOverlay = false;

private:
    std::optional<std::size_t> hitTest(const SDL_FPoint& point) const;
    void drawBackdrop(int width, int height) const;

    std::string _title;
    std::string _sceneName;
    InputManager _input;
    TTF_Font* _font = nullptr;
    std::shared_ptr<Sprite> _scout;
    std::shared_ptr<Sprite> _trooper;
    std::size_t _selected = 0;
    std::optional<std::size_t> _clicked;
    int _clickDirection = 1;
    float _time = 0.0f;
    std::string _status;
    bool _statusError = false;
};