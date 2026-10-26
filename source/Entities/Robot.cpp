#include "Entities.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

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
    state.patrolDirection = entity.robot.direction;
    state.home = { entity.x, entity.y };
    state.patrolOrigin = state.home;
    state.patrolEnd = state.home;
    state.previousPosition = state.home;
    if (entity.robot.type != RobotType::None)
    {
        state.idleSprite = entity.sprite;
        state.movingSprite = entity.robotMovingSprite ? entity.robotMovingSprite : entity.sprite;
    }
    return state;
}

bool Entities::CheckPlayerCapture() const
{
    if (!_nameEntities.contains(PlayerName))
    {
        return false;
    }
    constexpr float contactMargin = 0.5f;
    const SDL_FPoint playerCenter{ _xEntities[PlayerIndex], _yEntities[PlayerIndex] - _colliderSizes[PlayerIndex].y * 0.5f };
    for (std::size_t index = 1; index < _robotTypes.size(); ++index)
    {
        if (_robotTypes[index] == RobotType::None || _robotStates[index].phase != RobotRuntimeState::Phase::Routine ||
            _robotStates[index].teleportPhase != RobotRuntimeState::TeleportPhase::None ||
            RobotPersonalities[static_cast<std::size_t>(_robotStates[index].personality)].friendly)
        {
            continue;
        }
        const SDL_FPoint robotCenter{ _xEntities[index], _yEntities[index] - _colliderSizes[index].y * 0.5f };
        const float halfWidth = (_colliderSizes[index].x + _colliderSizes[PlayerIndex].x) * 0.5f;
        const float halfHeight = (_colliderSizes[index].y + _colliderSizes[PlayerIndex].y) * 0.5f;
        if (std::abs(robotCenter.x - playerCenter.x) <= halfWidth + contactMargin &&
            std::abs(robotCenter.y - playerCenter.y) <= halfHeight + contactMargin &&
            SightFraction(robotCenter, playerCenter) >= 0.999f)
        {
            return true;
        }
    }
    return false;
}

RobotPersonality Entities::GetRobotPersonality(std::size_t index) const
{
    ValidateIndex(index);
    return _robotTypes[index] == RobotType::None ? RobotPersonality::Count : _robotStates[index].personality;
}

void Entities::SetRobotPersonality(std::size_t index, RobotPersonality personality)
{
    auto& state = _robotStates[index];
    if (state.teleportPhase != RobotRuntimeState::TeleportPhase::None)
    {
        _visibilityEntities[index] = true;
        b2Body_Enable(_bodyIds[index]);
    }
    state.teleportPhase = RobotRuntimeState::TeleportPhase::None;
    state.teleportTime = 0.0f;
    state.returnHomeTime = 0.0f;
    state.rotationProgress = 0.0f;
    const auto direction = Directions[static_cast<std::size_t>(_sightDirections[index])];
    state.rotationAngle = std::atan2(direction.y, direction.x);
    state.personality = personality;
    state.pendingPersonality = personality;
    state.initialized = true;
    state.blockedTime = 0.0f;
    state.attemptedMovement = false;
    state.hasChasedPlayer = false;
    state.chaseMemory = 0.0f;
    state.repathTime = 0.0f;
    state.path.clear();
    state.nextWaypoint = 0;
    state.elapsed = 0.0f;
    state.transitionTime = 0.0f;
    state.patrolDistance = 0.0f;
    const bool friendly = RobotPersonalities[static_cast<std::size_t>(personality)].friendly;
    const float homeX = state.home.x - _xEntities[index];
    const float homeY = state.home.y - _yEntities[index];
    state.phase = friendly && homeX * homeX + homeY * homeY > 0.0625f ?
        RobotRuntimeState::Phase::ReturningHome : RobotRuntimeState::Phase::Routine;
    if (friendly)
    {
        state.patrolDirection = state.axisDirection;
        state.patrolOrigin = state.home;
    }
    else if (personality == RobotPersonality::ChangeAxisAggressive)
    {
        static constexpr std::array swapped{ SightDirection::Left, SightDirection::Right, SightDirection::Up, SightDirection::Down };
        state.patrolDirection = swapped[static_cast<std::size_t>(_sightDirections[index])];
        state.patrolOrigin = { _xEntities[index], _yEntities[index] };
        _sightDirections[index] = state.patrolDirection;
    }
    state.returning = false;
    state.switchDelay = state.phase == RobotRuntimeState::Phase::Routine ? NextRobotPersonalityDelay(index) : 0.0f;
    SetVelocity(index, 0.0f, 0.0f);
    const bool stationary = personality == RobotPersonality::FriendlyStopped || personality == RobotPersonality::FriendlyRotating;
    b2Body_SetType(_bodyIds[index], stationary && state.phase == RobotRuntimeState::Phase::Routine ? b2_staticBody : b2_kinematicBody);
}

float Entities::NextRobotPersonalityDelay(std::size_t index)
{
    const auto& behavior = Config::getInstance().getRobotProfile(_robotTypes[index]).behavior;
    if (!RobotPersonalities[static_cast<std::size_t>(_robotStates[index].personality)].friendly)
    {
        return std::uniform_real_distribution<float>(behavior.hostileMinSeconds, behavior.hostileMaxSeconds)(_robotRandom);
    }
    const float minimum = _personalityIntervalOverride.x > 0.0f ? _personalityIntervalOverride.x : behavior.switchMinSeconds;
    const float maximum = _personalityIntervalOverride.y > 0.0f ? _personalityIntervalOverride.y : behavior.switchMaxSeconds;
    return std::uniform_real_distribution<float>(minimum, maximum)(_robotRandom);
}

void Entities::SetRobotPersonalityInterval(float minimum, float maximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum <= 0.0f || maximum < minimum)
    {
        throw std::invalid_argument("Robot personality intervals must be positive, finite and ordered");
    }
    _personalityIntervalOverride = { minimum, maximum };
    for (std::size_t index = 0; index < _robotTypes.size(); ++index)
    {
        if (_robotTypes[index] != RobotType::None)
        {
            _robotStates[index].elapsed = 0.0f;
            _robotStates[index].switchDelay = NextRobotPersonalityDelay(index);
        }
    }
}

void Entities::SetRobotPersonalityEffect(std::vector<std::shared_ptr<Sprite>> frames)
{
    if (frames.empty() || std::any_of(frames.begin(), frames.end(), [](const auto& frame) { return !frame; }))
    {
        throw std::invalid_argument("Robot personality animation needs valid frames");
    }
    _personalityEffectFrames = std::move(frames);
}

void Entities::SetRobotTeleportEffect(std::vector<std::shared_ptr<Sprite>> frames)
{
    if (frames.empty() || std::any_of(frames.begin(), frames.end(), [](const auto& frame) { return !frame; }))
    {
        throw std::invalid_argument("Robot teleport animation needs valid frames");
    }
    _teleportEffectFrames = std::move(frames);
}

void Entities::BeginRobotPersonalityChange(std::size_t index, RobotPersonality personality)
{
    auto& state = _robotStates[index];
    state.pendingPersonality = personality;
    state.phase = RobotRuntimeState::Phase::Changing;
    state.transitionTime = 0.0f;
    state.elapsed = 0.0f;
    state.attemptedMovement = false;
    SetVelocity(index, 0.0f, 0.0f);
}

void Entities::UpdateRobot(std::size_t index, float deltaTime, bool personalityChangesEnabled)
{
    auto& state = _robotStates[index];
    const auto& behavior = Config::getInstance().getRobotProfile(_robotTypes[index]).behavior;
    if (!state.initialized)
    {
        SetRobotPersonality(index, behavior.initialPersonality);
    }
    SetVelocity(index, 0.0f, 0.0f);
    _visionStates[index] = VisionState::Unaware;
    if (state.phase == RobotRuntimeState::Phase::Changing)
    {
        if (personalityChangesEnabled)
        {
            state.transitionTime += deltaTime;
        }
        const float duration = std::max<std::size_t>(1, _personalityEffectFrames.size()) * behavior.transitionFrameSeconds;
        if (state.transitionTime < duration)
        {
            UpdateRobotAnimation(index);
            return;
        }
        SetRobotPersonality(index, state.pendingPersonality);
    }
    if (personalityChangesEnabled && state.phase == RobotRuntimeState::Phase::Routine)
    {
        state.elapsed += deltaTime;
    }
    if (state.teleportPhase != RobotRuntimeState::TeleportPhase::None)
    {
        if (personalityChangesEnabled)
        {
            UpdateRobotTeleport(index, deltaTime);
        }
        UpdateRobotAnimation(index);
        return;
    }
    const float switchDelay = state.switchDelay * (state.hasChasedPlayer ? behavior.chaseDurationMultiplier : 1.0f);
    if (personalityChangesEnabled && state.phase == RobotRuntimeState::Phase::Routine && state.elapsed >= switchDelay)
    {
        std::array<double, static_cast<std::size_t>(RobotPersonality::Count)> weights{};
        const bool currentlyFriendly = RobotPersonalities[static_cast<std::size_t>(state.personality)].friendly;
        bool canSwitch = false;
        for (const auto& choice : behavior.personalities)
        {
            if (RobotPersonalities[static_cast<std::size_t>(choice.personality)].friendly != currentlyFriendly)
            {
                weights[static_cast<std::size_t>(choice.personality)] = choice.weight;
                canSwitch = true;
            }
        }
        if (canSwitch)
        {
            std::discrete_distribution<std::size_t> choose(weights.begin(), weights.end());
            BeginRobotPersonalityChange(index, static_cast<RobotPersonality>(choose(_robotRandom)));
            UpdateRobotAnimation(index);
            return;
        }
        state.elapsed = 0.0f;
        state.switchDelay = NextRobotPersonalityDelay(index);
    }
    using PersonalityUpdate = void (Entities::*)(std::size_t, float);
    static constexpr std::array<PersonalityUpdate, 6> updates{
        &Entities::UpdateFriendlyMoving,
        &Entities::UpdateFriendlyStopped,
        &Entities::UpdateHostile,
        &Entities::UpdateChangeAxisAggressive,
        &Entities::UpdateFriendlyRotating,
        &Entities::UpdateHostileTeleporting
    };
    static_assert(updates.size() == static_cast<std::size_t>(RobotPersonality::Count));
    if (state.phase == RobotRuntimeState::Phase::ReturningHome)
    {
        UpdateRobotReturnHome(index, deltaTime);
    }
    else
    {
        (this->*updates[static_cast<std::size_t>(state.personality)])(index, deltaTime);
    }
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

bool Entities::PlanRobotPatrol(std::size_t index, bool chooseDistance)
{
    auto& state = _robotStates[index];
    const auto& behavior = Config::getInstance().getRobotProfile(_robotTypes[index]).behavior;
    const SDL_FPoint forward = Directions[static_cast<std::size_t>(state.patrolDirection)];
    const float tileLength = forward.x == 0.0f ? _tileSize.y : _tileSize.x;
    if (chooseDistance || state.patrolTiles == 0)
    {
        state.patrolTiles = std::uniform_int_distribution<int>(behavior.patrolMinTiles, behavior.patrolMaxTiles)(_robotRandom);
    }
    const float distance = tileLength * static_cast<float>(state.patrolTiles);
    SDL_FPoint translation{ forward.x * distance, forward.y * distance };
    float fraction = PatrolClearance(index, state.patrolOrigin, translation);
    if (distance * fraction < 1.0f)
    {
        translation = { -translation.x, -translation.y };
        fraction = PatrolClearance(index, state.patrolOrigin, translation);
    }
    state.patrolDistance = std::max(0.0f, distance * fraction - (fraction < 1.0f ? 0.5f : 0.0f));
    state.patrolEnd = { state.patrolOrigin.x + translation.x * state.patrolDistance / distance,
        state.patrolOrigin.y + translation.y * state.patrolDistance / distance };
    state.returning = false;
    return state.patrolDistance >= 1.0f;
}

void Entities::UpdateFriendlyMoving(std::size_t index, float deltaTime)
{
    auto& state = _robotStates[index];
    const bool chooseDistance = state.personality != RobotPersonality::ChangeAxisAggressive;
    if (state.patrolDistance < 1.0f && !PlanRobotPatrol(index, chooseDistance))
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
    SDL_FPoint target = state.returning ? state.patrolOrigin : state.patrolEnd;
    float distanceX = target.x - _xEntities[index];
    float distanceY = target.y - _yEntities[index];
    if (distanceX * distanceX + distanceY * distanceY <= 0.0625f)
    {
        if (state.returning)
        {
            if (!PlanRobotPatrol(index, chooseDistance))
            {
                return;
            }
        }
        else
        {
            state.returning = true;
        }
        target = state.returning ? state.patrolOrigin : state.patrolEnd;
        distanceX = target.x - _xEntities[index];
        distanceY = target.y - _yEntities[index];
    }
    MoveRobotTowards(index, target, Config::getInstance().getRobotProfile(_robotTypes[index]).behavior.movementSpeed, deltaTime);
}

void Entities::UpdateFriendlyStopped(std::size_t, float)
{
}

void Entities::UpdateChangeAxisAggressive(std::size_t index, float deltaTime)
{
    UpdateHostile(index, deltaTime);
    if (_robotStates[index].chaseMemory <= 0.0f)
    {
        UpdateFriendlyMoving(index, deltaTime);
    }
}

void Entities::UpdateFriendlyRotating(std::size_t index, float deltaTime)
{
    auto& state = _robotStates[index];
    const float turn = Config::getInstance().getRobotProfile(_robotTypes[index]).behavior.rotationSpeedDegrees *
        std::numbers::pi_v<float> / 180.0f * deltaTime;
    state.rotationAngle = std::fmod(state.rotationAngle + turn + 2.0f * std::numbers::pi_v<float>, 2.0f * std::numbers::pi_v<float>);
    state.rotationProgress += turn;
    const float directionX = std::cos(state.rotationAngle);
    const float directionY = std::sin(state.rotationAngle);
    _sightDirections[index] = std::abs(directionX) > std::abs(directionY) ?
        (directionX < 0.0f ? SightDirection::Left : SightDirection::Right) :
        (directionY < 0.0f ? SightDirection::Up : SightDirection::Down);
}

void Entities::UpdateHostileTeleporting(std::size_t index, float deltaTime)
{
    auto& state = _robotStates[index];
    if (state.chaseMemory <= 0.0f)
    {
        UpdateFriendlyRotating(index, deltaTime);
    }
    UpdateHostile(index, deltaTime);
    if (state.chaseMemory > 0.0f)
    {
        state.rotationProgress = 0.0f;
        return;
    }
    if (state.rotationProgress >= 2.0f * std::numbers::pi_v<float>)
    {
        state.teleportPhase = RobotRuntimeState::TeleportPhase::Disappearing;
        state.teleportTime = 0.0f;
        state.teleportOrigin = { _xEntities[index], _yEntities[index] };
    }
}

bool Entities::CanRobotTeleportTo(std::size_t index, const SDL_FPoint& position) const
{
    const float halfWidth = _colliderSizes[index].x * 0.5f + 1.0f;
    const float height = _colliderSizes[index].y + 1.0f;
    if (position.x - halfWidth < _mapBounds.x || position.x + halfWidth > _mapBounds.x + _mapBounds.w ||
        position.y - height < _mapBounds.y || position.y + 1.0f > _mapBounds.y + _mapBounds.h)
    {
        return false;
    }
    if (!_floorTiles.empty())
    {
        const SDL_FRect collider{ position.x - halfWidth, position.y - height, halfWidth * 2.0f, height + 1.0f };
        float floorArea = 0.0f;
        for (const auto& tile : _floorTiles)
        {
            SDL_FRect intersection{};
            if (SDL_GetRectIntersectionFloat(&collider, &tile, &intersection))
            {
                floorArea += intersection.w * intersection.h;
            }
        }
        if (floorArea + 0.001f < collider.w * collider.h)
        {
            return false;
        }
    }
    b2ShapeProxy proxy{};
    proxy.count = 4;
    proxy.points[0] = { (position.x - halfWidth) / PixelsPerMeter, (position.y - height) / PixelsPerMeter };
    proxy.points[1] = { (position.x + halfWidth) / PixelsPerMeter, (position.y - height) / PixelsPerMeter };
    proxy.points[2] = { (position.x + halfWidth) / PixelsPerMeter, (position.y + 1.0f) / PixelsPerMeter };
    proxy.points[3] = { (position.x - halfWidth) / PixelsPerMeter, (position.y + 1.0f) / PixelsPerMeter };
    auto filter = b2DefaultQueryFilter();
    filter.maskBits = 3u;
    struct OccupancyContext
    {
        b2BodyId ignoredBody;
        bool blocked = false;
    };
    OccupancyContext context{ _bodyIds[index] };
    b2World_OverlapShape(_worldId, &proxy, filter, [](b2ShapeId shape, void* userData)
    {
        auto& occupancy = *static_cast<OccupancyContext*>(userData);
        if (B2_ID_EQUALS(b2Shape_GetBody(shape), occupancy.ignoredBody))
        {
            return true;
        }
        occupancy.blocked = true;
        return false;
    }, &context);
    return !context.blocked;
}

void Entities::UpdateRobotTeleport(std::size_t index, float deltaTime)
{
    auto& state = _robotStates[index];
    const auto& behavior = Config::getInstance().getRobotProfile(_robotTypes[index]).behavior;
    state.teleportTime += deltaTime;
    const float effectDuration = std::max<std::size_t>(1, _teleportEffectFrames.size()) * behavior.teleportFrameSeconds;
    if (state.teleportPhase == RobotRuntimeState::TeleportPhase::Disappearing && state.teleportTime >= effectDuration)
    {
        state.teleportPhase = RobotRuntimeState::TeleportPhase::Hidden;
        state.teleportTime = 0.0f;
        _visibilityEntities[index] = false;
        b2Body_Disable(_bodyIds[index]);
    }
    else if (state.teleportPhase == RobotRuntimeState::TeleportPhase::Hidden && state.teleportTime >= behavior.teleportHiddenSeconds)
    {
        const bool returningHome = state.phase == RobotRuntimeState::Phase::ReturningHome;
        SDL_FPoint destination = returningHome ? state.home : state.teleportOrigin;
        bool found = returningHome;
        for (int attempt = 0; !found && attempt < behavior.teleportAttempts; ++attempt)
        {
            const float angle = std::uniform_real_distribution<float>(0.0f, 2.0f * std::numbers::pi_v<float>)(_robotRandom);
            const float radius = std::sqrt(std::uniform_real_distribution<float>(0.0f, 1.0f)(_robotRandom)) * behavior.teleportRadius;
            const SDL_FPoint candidate{ state.teleportOrigin.x + std::cos(angle) * radius,
                state.teleportOrigin.y + std::sin(angle) * radius };
            if (CanRobotTeleportTo(index, candidate))
            {
                destination = candidate;
                found = true;
                break;
            }
        }
        if (!found && !CanRobotTeleportTo(index, destination))
        {
            state.teleportTime = 0.0f;
            return;
        }
        b2Body_SetTransform(_bodyIds[index], { destination.x / PixelsPerMeter,
            (destination.y - _colliderSizes[index].y * 0.5f) / PixelsPerMeter }, b2MakeRot(0.0f));
        _xEntities[index] = destination.x;
        _yEntities[index] = destination.y;
        state.previousPosition = destination;
        state.path.clear();
        state.nextWaypoint = 0;
        state.repathTime = 0.0f;
        _visibilityEntities[index] = true;
        b2Body_Enable(_bodyIds[index]);
        state.teleportPhase = RobotRuntimeState::TeleportPhase::Appearing;
        state.teleportTime = 0.0f;
    }
    else if (state.teleportPhase == RobotRuntimeState::TeleportPhase::Appearing && state.teleportTime >= effectDuration)
    {
        state.teleportPhase = RobotRuntimeState::TeleportPhase::None;
        state.teleportTime = 0.0f;
        state.rotationProgress = 0.0f;
    }
}

void Entities::UpdateRobotReturnHome(std::size_t index, float deltaTime)
{
    auto& state = _robotStates[index];
    const SDL_FPoint position{ _xEntities[index], _yEntities[index] };
    const float deltaX = state.home.x - position.x;
    const float deltaY = state.home.y - position.y;
    if (deltaX * deltaX + deltaY * deltaY <= 0.0625f)
    {
        state.phase = RobotRuntimeState::Phase::Routine;
        state.returnHomeTime = 0.0f;
        state.elapsed = 0.0f;
        state.switchDelay = NextRobotPersonalityDelay(index);
        state.patrolDistance = 0.0f;
        state.path.clear();
        state.nextWaypoint = 0;
        state.attemptedMovement = false;
        _sightDirections[index] = state.axisDirection;
        if (state.personality == RobotPersonality::FriendlyStopped || state.personality == RobotPersonality::FriendlyRotating)
        {
            b2Body_SetType(_bodyIds[index], b2_staticBody);
        }
        return;
    }
    if (_robotTypes[index] == RobotType::MedusaDrone)
    {
        state.returnHomeTime += deltaTime;
        const auto& behavior = Config::getInstance().getRobotProfile(_robotTypes[index]).behavior;
        if (state.returnHomeTime >= behavior.returnHomeTeleportSeconds)
        {
            state.teleportPhase = RobotRuntimeState::TeleportPhase::Disappearing;
            state.teleportTime = 0.0f;
            state.teleportOrigin = position;
            SetVelocity(index, 0.0f, 0.0f);
            return;
        }
    }
    state.repathTime -= deltaTime;
    if (state.repathTime <= 0.0f)
    {
        state.path = Navigation::findPath({ _worldId, _mapBounds, { _tileSize.x * 0.5f, _tileSize.y * 0.5f },
            _colliderSizes[index], position, state.home });
        state.nextWaypoint = 0;
        state.repathTime = Config::getInstance().getRobotProfile(_robotTypes[index]).behavior.chaseRepathSeconds;
    }
    while (state.nextWaypoint < state.path.size())
    {
        const auto& target = state.path[state.nextWaypoint];
        const float distanceX = target.x - position.x;
        const float distanceY = target.y - position.y;
        if (distanceX * distanceX + distanceY * distanceY > 0.0625f)
        {
            MoveRobotTowards(index, target, Config::getInstance().getRobotProfile(_robotTypes[index]).behavior.movementSpeed, deltaTime);
            return;
        }
        ++state.nextWaypoint;
    }
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
        state.hasChasedPlayer = true;
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
    const float stopDistance = std::min(behavior.chaseStopDistance, 0.25f);
    if (distanceX * distanceX + distanceY * distanceY <= stopDistance * stopDistance)
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
    _robotStates[index].rotationAngle = std::atan2(distanceY, distanceX);
    _sightDirections[index] = std::abs(distanceX) > std::abs(distanceY) ?
        (distanceX < 0.0f ? SightDirection::Left : SightDirection::Right) :
        (distanceY < 0.0f ? SightDirection::Up : SightDirection::Down);
}

void Entities::UpdateRobotAnimation(std::size_t index)
{
    const auto& state = _robotStates[index];
    const bool moving = _velocityXEntities[index] * _velocityXEntities[index] + _velocityYEntities[index] * _velocityYEntities[index] > 1.0f;
    const auto& sprite = moving ? state.movingSprite : state.idleSprite;
    const bool rotates = RobotPersonalities[static_cast<std::size_t>(state.personality)].rotates;
    const auto octant = static_cast<std::size_t>(std::floor((state.rotationAngle + 2.0f * std::numbers::pi_v<float> +
        std::numbers::pi_v<float> / 8.0f) / (std::numbers::pi_v<float> / 4.0f))) % 8;
    const std::size_t row = rotates ? (octant + 6) % 8 : RobotIdleRows[static_cast<std::size_t>(_sightDirections[index])];
    const std::size_t firstFrame = row * 8;
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
    const auto& state = _robotStates[index];
    const bool rotates = RobotPersonalities[static_cast<std::size_t>(state.personality)].rotates;
    const SDL_FPoint forward = rotates ? SDL_FPoint{ std::cos(state.rotationAngle), std::sin(state.rotationAngle) } :
        Directions[static_cast<std::size_t>(_sightDirections[index])];
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