#include "Entities.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include <box2d/box2d.h>

#include "Config/Config.h"
#include "Core/Navigation/Navigation.h"
#include "Entities/Sprite.h"

namespace
{
    constexpr std::array<SDL_FPoint, 4> Directions{ {
        { 0.0f, -1.0f }, { 0.0f, 1.0f }, { -1.0f, 0.0f }, { 1.0f, 0.0f }
    } };
}

Entities::RobotRuntimeState Entities::MakeRobotState(const EntityData& entity)
{
    RobotRuntimeState state;
    state.axisDirection = entity.robot.direction;
    state.home = { entity.x, entity.y };
    state.patrolEnd = state.home;
    state.previousPosition = state.home;
    if (entity.robot.type != RobotType::None)
    {
        state.idleSprite = entity.sprite;
        state.movingSprite = entity.robotMovingSprite ? entity.robotMovingSprite : entity.sprite;
    }
    return state;
}

RobotPersonality Entities::GetRobotPersonality(std::size_t index) const
{
    ValidateIndex(index);
    return _robotTypes[index] == RobotType::None ? RobotPersonality::Count : _robotStates[index].personality;
}

void Entities::SetRobotPersonality(std::size_t index, RobotPersonality personality)
{
    auto& state = _robotStates[index];
    state.personality = personality;
    state.initialized = true;
    state.blockedTime = 0.0f;
    state.attemptedMovement = false;
    state.chaseMemory = 0.0f;
    state.repathTime = 0.0f;
    state.path.clear();
    state.nextWaypoint = 0;
    SetVelocity(index, 0.0f, 0.0f);
    b2Body_SetType(_bodyIds[index], personality == RobotPersonality::FriendlyStopped ? b2_staticBody : b2_kinematicBody);
}

void Entities::UpdateRobot(std::size_t index, float deltaTime, bool personalityChangesEnabled)
{
    auto& state = _robotStates[index];
    const auto& behavior = Config::getInstance().getRobotProfile(_robotTypes[index]).behavior;
    if (!state.initialized)
    {
        SetRobotPersonality(index, behavior.initialPersonality);
    }
    if (personalityChangesEnabled)
    {
        state.elapsed += deltaTime;
    }
    if (personalityChangesEnabled && state.elapsed >= behavior.switchSeconds)
    {
        state.elapsed = std::fmod(state.elapsed, behavior.switchSeconds);
        std::array<double, static_cast<std::size_t>(RobotPersonality::Count)> weights{};
        bool canSwitch = false;
        for (const auto& choice : behavior.personalities)
        {
            if (choice.personality != state.personality)
            {
                weights[static_cast<std::size_t>(choice.personality)] = choice.weight;
                canSwitch = true;
            }
        }
        if (canSwitch)
        {
            std::discrete_distribution<std::size_t> choose(weights.begin(), weights.end());
            SetRobotPersonality(index, static_cast<RobotPersonality>(choose(_robotRandom)));
        }
    }
    SetVelocity(index, 0.0f, 0.0f);
    _visionStates[index] = VisionState::Unaware;
    using PersonalityUpdate = void (Entities::*)(std::size_t, float);
    static constexpr std::array<PersonalityUpdate, 3> updates{
        &Entities::UpdateFriendlyMoving,
        &Entities::UpdateFriendlyStopped,
        &Entities::UpdateHostile
    };
    static_assert(updates.size() == static_cast<std::size_t>(RobotPersonality::Count));
    (this->*updates[static_cast<std::size_t>(state.personality)])(index, deltaTime);
    state.attemptedMovement = _velocityXEntities[index] * _velocityXEntities[index] +
        _velocityYEntities[index] * _velocityYEntities[index] > 1.0f;
    state.previousPosition = { _xEntities[index], _yEntities[index] };
    UpdateRobotAnimation(index);
}

float Entities::PatrolClearance(std::size_t index, const SDL_FPoint& origin, const SDL_FPoint& translation,
    std::uint64_t maskBits) const
{
    const float halfWidth = _colliderSizes[index].x * 0.5f;
    const float height = _colliderSizes[index].y;
    const std::array<b2Vec2, 4> points{ {
        { (origin.x - halfWidth) / PixelsPerMeter, (origin.y - height) / PixelsPerMeter },
        { (origin.x + halfWidth) / PixelsPerMeter, (origin.y - height) / PixelsPerMeter },
        { (origin.x + halfWidth) / PixelsPerMeter, origin.y / PixelsPerMeter },
        { (origin.x - halfWidth) / PixelsPerMeter, origin.y / PixelsPerMeter }
    } };
    b2ShapeProxy proxy{};
    std::copy(points.begin(), points.end(), proxy.points);
    proxy.count = static_cast<int>(points.size());
    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.maskBits = maskBits;
    struct SweepContext
    {
        b2BodyId ignoredBody;
        float fraction = 1.0f;
    };
    SweepContext context{ _bodyIds[index] };
    b2World_CastShape(_worldId, &proxy, { translation.x / PixelsPerMeter, translation.y / PixelsPerMeter }, filter,
        [](b2ShapeId shape, b2Vec2, b2Vec2, float hitFraction, void* userData)
        {
            auto& sweep = *static_cast<SweepContext*>(userData);
            const auto body = b2Shape_GetBody(shape);
            if (B2_ID_EQUALS(body, sweep.ignoredBody))
            {
                return -1.0f;
            }
            sweep.fraction = std::min(sweep.fraction, hitFraction);
            return sweep.fraction;
        }, &context);
    return context.fraction;
}

bool Entities::PlanRobotPatrol(std::size_t index)
{
    auto& state = _robotStates[index];
    const auto& behavior = Config::getInstance().getRobotProfile(_robotTypes[index]).behavior;
    const SDL_FPoint forward = Directions[static_cast<std::size_t>(state.axisDirection)];
    const float tileLength = forward.x == 0.0f ? _tileSize.y : _tileSize.x;
    const int tiles = std::uniform_int_distribution<int>(behavior.patrolMinTiles, behavior.patrolMaxTiles)(_robotRandom);
    const float distance = tileLength * static_cast<float>(tiles);
    SDL_FPoint translation{ forward.x * distance, forward.y * distance };
    float fraction = PatrolClearance(index, state.home, translation);
    if (distance * fraction < 1.0f)
    {
        translation = { -translation.x, -translation.y };
        fraction = PatrolClearance(index, state.home, translation);
    }
    state.patrolDistance = std::max(0.0f, distance * fraction - (fraction < 1.0f ? 0.5f : 0.0f));
    state.patrolEnd = { state.home.x + translation.x * state.patrolDistance / distance,
        state.home.y + translation.y * state.patrolDistance / distance };
    state.returning = false;
    return state.patrolDistance >= 1.0f;
}

void Entities::UpdateFriendlyMoving(std::size_t index, float deltaTime)
{
    auto& state = _robotStates[index];
    if (state.patrolDistance < 1.0f && !PlanRobotPatrol(index))
    {
        return;
    }
    const float traveledX = _xEntities[index] - state.previousPosition.x;
    const float traveledY = _yEntities[index] - state.previousPosition.y;
    state.blockedTime = state.attemptedMovement && traveledX * traveledX + traveledY * traveledY < 0.0025f ?
        state.blockedTime + deltaTime : 0.0f;
    if (state.blockedTime >= 0.3f && !state.returning)
    {
        state.returning = true;
        state.blockedTime = 0.0f;
    }
    SDL_FPoint target = state.returning ? state.home : state.patrolEnd;
    float distanceX = target.x - _xEntities[index];
    float distanceY = target.y - _yEntities[index];
    if (distanceX * distanceX + distanceY * distanceY <= 0.0625f)
    {
        if (state.returning)
        {
            if (!PlanRobotPatrol(index))
            {
                return;
            }
        }
        else
        {
            state.returning = true;
        }
        target = state.returning ? state.home : state.patrolEnd;
        distanceX = target.x - _xEntities[index];
        distanceY = target.y - _yEntities[index];
    }
    MoveRobotTowards(index, target, Config::getInstance().getRobotProfile(_robotTypes[index]).behavior.movementSpeed, deltaTime);
}

void Entities::UpdateFriendlyStopped(std::size_t, float)
{
}

void Entities::UpdateHostile(std::size_t index, float deltaTime)
{
    auto& state = _robotStates[index];
    UpdateRobotVision(index, state.chaseMemory > 0.0f);
    const auto& behavior = Config::getInstance().getRobotProfile(_robotTypes[index]).behavior;
    if (_visionStates[index] == VisionState::Detected ||
        (state.chaseMemory > 0.0f && _visionStates[index] == VisionState::Peripheral))
    {
        state.lastSeenPlayer = { _xEntities[PlayerIndex], _yEntities[PlayerIndex] };
        state.chaseMemory = behavior.chaseMemorySeconds;
    }
    else
    {
        state.chaseMemory = std::max(0.0f, state.chaseMemory - deltaTime);
    }
    if (state.chaseMemory <= 0.0f)
    {
        state.path.clear();
        state.nextWaypoint = 0;
        state.repathTime = 0.0f;
        return;
    }
    _visionStates[index] = VisionState::Detected;
    const SDL_FPoint position{ _xEntities[index], _yEntities[index] };
    const float distanceX = state.lastSeenPlayer.x - position.x;
    const float distanceY = state.lastSeenPlayer.y - position.y;
    if (distanceX * distanceX + distanceY * distanceY <= behavior.chaseStopDistance * behavior.chaseStopDistance)
    {
        return;
    }
    state.repathTime -= deltaTime;
    if (state.repathTime <= 0.0f)
    {
        state.path = Navigation::findPath({ _worldId, _mapBounds, { _tileSize.x * 0.5f, _tileSize.y * 0.5f },
            _colliderSizes[index], position, state.lastSeenPlayer });
        state.nextWaypoint = 0;
        state.repathTime = behavior.chaseRepathSeconds;
    }
    while (state.nextWaypoint < state.path.size())
    {
        const auto& waypoint = state.path[state.nextWaypoint];
        const float deltaX = waypoint.x - position.x;
        const float deltaY = waypoint.y - position.y;
        if (deltaX * deltaX + deltaY * deltaY > 0.0625f)
        {
            break;
        }
        ++state.nextWaypoint;
    }
    if (state.nextWaypoint >= state.path.size())
    {
        return;
    }
    const SDL_FPoint target = state.path[state.nextWaypoint];
    if (PatrolClearance(index, position, { target.x - position.x, target.y - position.y }) < 0.999f)
    {
        state.repathTime = 0.0f;
        return;
    }
    MoveRobotTowards(index, target, behavior.chaseSpeed, deltaTime);
}

void Entities::MoveRobotTowards(std::size_t index, const SDL_FPoint& target, float speed, float deltaTime)
{
    const float distanceX = target.x - _xEntities[index];
    const float distanceY = target.y - _yEntities[index];
    const float distance = std::sqrt(distanceX * distanceX + distanceY * distanceY);
    if (distance < 0.0001f)
    {
        return;
    }
    speed = std::min(speed, distance / deltaTime);
    const float step = speed * deltaTime;
    const float clearance = PatrolClearance(index, { _xEntities[index], _yEntities[index] },
        { distanceX * step / distance, distanceY * step / distance }, 3u);
    if (clearance < 1.0f)
    {
        speed = std::max(0.0f, step * clearance - 0.1f) / deltaTime;
    }
    SetVelocity(index, distanceX * speed / distance, distanceY * speed / distance);
    _sightDirections[index] = std::abs(distanceX) > std::abs(distanceY) ?
        (distanceX < 0.0f ? SightDirection::Left : SightDirection::Right) :
        (distanceY < 0.0f ? SightDirection::Up : SightDirection::Down);
}

void Entities::UpdateRobotAnimation(std::size_t index)
{
    const auto& state = _robotStates[index];
    const bool moving = _velocityXEntities[index] * _velocityXEntities[index] + _velocityYEntities[index] * _velocityYEntities[index] > 1.0f;
    const auto& sprite = moving ? state.movingSprite : state.idleSprite;
    const std::size_t firstFrame = RobotIdleRows[static_cast<std::size_t>(_sightDirections[index])] * 8;
    if (_spriteEntities[index] != sprite || _animations[index].idleFirstFrame != firstFrame)
    {
        _spriteEntities[index] = sprite;
        _animations[index] = { firstFrame, 8, firstFrame, 8, 0.12f, 0.1f };
        _animationTimes[index] = 0.0f;
        _frames[index] = firstFrame;
    }
    _facingLeft[index] = false;
}

float Entities::SightFraction(const SDL_FPoint& origin, const SDL_FPoint& target) const
{
    if (!b2World_IsValid(_worldId) || (origin.x == target.x && origin.y == target.y))
    {
        return 1.0f;
    }
    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.maskBits = 1u;
    const b2RayResult result = b2World_CastRayClosest(_worldId,
        { origin.x / PixelsPerMeter, origin.y / PixelsPerMeter },
        { (target.x - origin.x) / PixelsPerMeter, (target.y - origin.y) / PixelsPerMeter }, filter);
    return result.hit ? result.fraction : 1.0f;
}

void Entities::UpdateRobotVision(std::size_t index, bool trackingPlayer)
{
    if (!_nameEntities.contains(PlayerName))
    {
        return;
    }
    const RobotProfile& profile = Config::getInstance().getRobotProfile(_robotTypes[index]);
    const SDL_FPoint origin{ _xEntities[index], _yEntities[index] - _colliderSizes[index].y * 0.5f };
    const SDL_FPoint target{ _xEntities[PlayerIndex], _yEntities[PlayerIndex] - _colliderSizes[PlayerIndex].y * 0.5f };
    const float distanceX = target.x - origin.x;
    const float distanceY = target.y - origin.y;
    const float distanceSquared = distanceX * distanceX + distanceY * distanceY;
    if (distanceSquared > profile.visionRadius * profile.visionRadius)
    {
        return;
    }
    if (distanceSquared < 0.0001f)
    {
        _visionStates[index] = VisionState::Detected;
        return;
    }
    const float distance = std::sqrt(distanceSquared);
    const SDL_FPoint forward = Directions[static_cast<std::size_t>(_sightDirections[index])];
    const float alignment = (distanceX * forward.x + distanceY * forward.y) / distance;
    const float halfAngle = profile.visionAngleDegrees * std::numbers::pi_v<float> / 360.0f;
    if ((!trackingPlayer && alignment < std::cos(halfAngle)) || SightFraction(origin, target) < 0.999f)
    {
        return;
    }
    const bool edge = (!trackingPlayer && alignment < std::cos(halfAngle * profile.edgeFraction)) ||
        distance > profile.visionRadius * profile.edgeFraction;
    _visionStates[index] = edge ? VisionState::Peripheral : VisionState::Detected;
}