#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

enum class RobotType : std::uint8_t
{
    None,
    ScoutBot,
    TrooperBot,
    Count
};

enum class SightDirection : std::uint8_t
{
    Up,
    Down,
    Left,
    Right,
    Count
};

enum class VisionState : std::uint8_t
{
    Unaware,
    Peripheral,
    Detected
};

enum class RobotPersonality : std::uint8_t
{
    FriendlyMoving,
    FriendlyStopped,
    Hostile,
    Count
};

struct RobotPersonalityDefinition
{
    std::string_view name;
    bool detectsPlayer;
};

inline constexpr std::array<RobotPersonalityDefinition, static_cast<std::size_t>(RobotPersonality::Count)> RobotPersonalities{ {
    { "friendly-moving", false },
    { "friendly-stopped", false },
    { "hostile", true }
} };

struct RobotPersonalityChoice
{
    RobotPersonality personality;
    float weight;
};

struct RobotBehaviorSettings
{
    RobotPersonality initialPersonality = RobotPersonality::FriendlyStopped;
    float switchSeconds = 10.0f;
    float movementSpeed = 32.0f;
    float chaseSpeed = 64.0f;
    float chaseRepathSeconds = 0.5f;
    float chaseMemorySeconds = 4.0f;
    float chaseStopDistance = 12.0f;
    int patrolMinTiles = 2;
    int patrolMaxTiles = 4;
    std::vector<RobotPersonalityChoice> personalities;
};

struct EntityRobotSettings
{
    RobotType type = RobotType::None;
    SightDirection direction = SightDirection::Down;
};

struct RobotProfile
{
    std::string spriteName;
    float spriteScale = 1.0f;
    // Vision properties
    float visionAngleDegrees; // Field of view angle in degrees
    float visionRadius;       // Maximum vision distance
    float edgeFraction;       // Fraction of the vision cone edge for peripheral detection
    // Collider properties
    float colliderWidth;  // Width of the robot's collider
    float colliderHeight; // Height of the robot's collider
    float pivotX;         // X-coordinate of the collider's pivot point
    float pivotY;         // Y-coordinate of the collider's pivot point
    std::string movingSpriteName;
    RobotBehaviorSettings behavior;
};