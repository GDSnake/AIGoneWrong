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
    MedusaDrone,
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
    ChangeAxisAggressive,
    FriendlyRotating,
    HostileTeleporting,
    Count
};

struct RobotPersonalityDefinition
{
    std::string_view name;
    bool detectsPlayer;
    bool friendly;
    bool rotates = false;
};

inline constexpr std::array<RobotPersonalityDefinition, static_cast<std::size_t>(RobotPersonality::Count)> RobotPersonalities{ {
    { "friendly-moving", false, true },
    { "friendly-stopped", false, true },
    { "hostile", true, false },
    { "change-axis-aggressive", true, false },
    { "friendly-rotating", false, true, true },
    { "hostile-teleporting", true, false, true }
} };

struct RobotPersonalityChoice
{
    RobotPersonality personality;
    float weight;
};

struct RobotBehaviorSettings
{
    RobotPersonality initialPersonality = RobotPersonality::FriendlyStopped;
    float switchMinSeconds = 2.0f;
    float switchMaxSeconds = 4.0f;
    float hostileMinSeconds = 3.0f;
    float hostileMaxSeconds = 5.0f;
    float transitionFrameSeconds = 0.1f;
    float transitionBlinkSeconds = 0.1f;
    float movementSpeed = 32.0f;
    float chaseSpeed = 76.8f;
    float chaseRepathSeconds = 0.5f;
    float chaseMemorySeconds = 4.0f;
    float chaseStopDistance = 12.0f;
    float chaseDurationMultiplier = 1.5f;
    float rotationSpeedDegrees = 180.0f;
    float teleportRadius = 100.0f;
    float teleportHiddenSeconds = 0.5f;
    float teleportFrameSeconds = 0.1f;
    int teleportAttempts = 64;
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