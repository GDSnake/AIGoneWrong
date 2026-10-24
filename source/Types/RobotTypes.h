#pragma once

#include <cstdint>
#include <string>

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
};