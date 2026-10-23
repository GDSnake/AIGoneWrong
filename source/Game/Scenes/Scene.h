#pragma once

#include <cstdint>
#include <memory>
#include <SDL3/SDL_events.h>

#include <string>

enum class SceneAnimation : std::uint8_t
{
    None,
    FadeIn,
    FadeOut,
    Count
};

class Scene
{
public:
    struct AnimationSettings
    {
        SceneAnimation entryAnimation = SceneAnimation::FadeIn;
        SceneAnimation exitAnimation = SceneAnimation::FadeOut;
        float entryDuration = 0.75f;
        float exitDuration = 0.35f;
    };

    virtual ~Scene() = default;

    virtual void init() = 0;
    virtual void handleEvents(const SDL_Event& event) = 0;
    virtual void handleInput() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render() = 0;
    virtual void cleanup() = 0;
    virtual std::string getName() const = 0;
    virtual bool shouldQuit() const = 0;
    void setAnimationSettings(const AnimationSettings& settings);
    bool playAnimation(SceneAnimation animation, float duration = 0.35f);
    void startEntryAnimation();
    bool transitionTo(std::unique_ptr<Scene> nextScene);
    void updateAnimations(float deltaTime);
    void renderAnimations() const;
    bool isTransitioning() const noexcept;
    std::unique_ptr<Scene> getNextScene();

private:
    AnimationSettings _animationSettings;
    SceneAnimation _animation = SceneAnimation::None;
    float _animationTime = 0.0f;
    float _animationDuration = 0.0f;
    bool _animationComplete = true;
    std::unique_ptr<Scene> _nextScene;
};