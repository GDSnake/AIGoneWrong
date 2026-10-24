#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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

struct EntityRobotSettings
{
    RobotType type = RobotType::None;
    SightDirection direction = SightDirection::Down;
};

struct RobotProfile
{
    const char* spriteName;
    // Vision properties
    float visionAngleDegrees; // Field of view angle in degrees
    float visionRadius;       // Maximum vision distance
    float edgeFraction;       // Fraction of the vision cone edge for peripheral detection
    // Collider properties
    float colliderWidth;  // Width of the robot's collider
    float colliderHeight; // Height of the robot's collider
    float pivotX;         // X-coordinate of the collider's pivot point
    float pivotY;         // Y-coordinate of the collider's pivot point
};

inline constexpr std::array<RobotProfile, static_cast<std::size_t>(RobotType::Count)> RobotProfiles{ {
    { "", 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 32.0f, 53.0f },
    { "ScoutBot", 45.0f, 100.0f, 0.8f, 14.0f, 8.0f, 32.0f, 53.0f },
    { "TrooperBot", 70.0f, 100.0f, 0.8f, 16.0f, 10.0f, 32.0f, 53.0f }
} };

inline constexpr std::array<std::size_t, static_cast<std::size_t>(SightDirection::Count)> RobotIdleRows{ 4, 0, 2, 6 };

constexpr const RobotProfile& GetRobotProfile(RobotType type)
{
    return RobotProfiles.at(static_cast<std::size_t>(type));
}