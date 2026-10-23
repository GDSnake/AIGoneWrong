#include "Scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "Core/Renderer.h"

namespace
{
    using AnimationRenderer = void (*)(const Renderer&, float);

    void renderFadeIn(const Renderer& renderer, float progress)
    {
        renderer.drawOverlay({ 0, 0, 0, static_cast<Uint8>(std::lround((1.0f - progress) * 255.0f)) });
    }

    void renderFadeOut(const Renderer& renderer, float progress)
    {
        renderer.drawOverlay({ 0, 0, 0, static_cast<Uint8>(std::lround(progress * 255.0f)) });
    }

    constexpr std::array AnimationLibrary{
        AnimationRenderer{ nullptr },
        &renderFadeIn,
        &renderFadeOut
    };
    static_assert(AnimationLibrary.size() == static_cast<std::size_t>(SceneAnimation::Count));
}

void Scene::setAnimationSettings(const AnimationSettings& settings)
{
    if (settings.entryAnimation >= SceneAnimation::Count || settings.exitAnimation >= SceneAnimation::Count ||
        !std::isfinite(settings.entryDuration) || !std::isfinite(settings.exitDuration) ||
        settings.entryDuration < 0.0f || settings.exitDuration < 0.0f)
    {
        throw std::invalid_argument("Scene animations must be valid with finite nonnegative durations");
    }
    _animationSettings = settings;
}

bool Scene::playAnimation(SceneAnimation animation, float duration)
{
    if (animation >= SceneAnimation::Count || !std::isfinite(duration) || duration < 0.0f)
    {
        throw std::invalid_argument("Scene animation must be valid with a finite nonnegative duration");
    }
    if (isTransitioning() || _nextScene)
    {
        return false;
    }
    _animation = animation;
    _animationTime = 0.0f;
    _animationDuration = duration;
    _animationComplete = animation == SceneAnimation::None || duration == 0.0f;
    return true;
}

void Scene::startEntryAnimation()
{
    playAnimation(_animationSettings.entryAnimation, _animationSettings.entryDuration);
}

bool Scene::transitionTo(std::unique_ptr<Scene> nextScene)
{
    if (!nextScene || isTransitioning() || _nextScene)
    {
        return false;
    }
    if (!playAnimation(_animationSettings.exitAnimation, _animationSettings.exitDuration))
    {
        return false;
    }
    _nextScene = std::move(nextScene);
    return true;
}

bool Scene::requestExit()
{
    if (_exitRequested || _nextScene || !playAnimation(_animationSettings.exitAnimation, _animationSettings.exitDuration))
    {
        return false;
    }
    _exitRequested = true;
    return true;
}

bool Scene::isExitReady() const noexcept
{
    return _exitRequested && _animationComplete;
}

void Scene::updateAnimations(float deltaTime)
{
    if (_animationComplete || !std::isfinite(deltaTime) || deltaTime < 0.0f)
    {
        return;
    }
    _animationTime = std::min(_animationTime + deltaTime, _animationDuration);
    _animationComplete = _animationTime >= _animationDuration;
}

void Scene::renderAnimations() const
{
    if (const auto render = AnimationLibrary[static_cast<std::size_t>(_animation)])
    {
        const float progress = _animationDuration > 0.0f
            ? std::clamp(_animationTime / _animationDuration, 0.0f, 1.0f) : 1.0f;
        render(Renderer::getInstance(), progress);
    }
}

bool Scene::isTransitioning() const noexcept
{
    return !_animationComplete || _exitRequested;
}

std::unique_ptr<Scene> Scene::getNextScene()
{
    if (!_nextScene || !_animationComplete)
    {
        return {};
    }
    return std::move(_nextScene);
}