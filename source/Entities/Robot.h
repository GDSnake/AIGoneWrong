#pragma once

#include <array>
#include <cstddef>

#include "Types/RobotTypes.h"

inline constexpr std::array<std::size_t, static_cast<std::size_t>(SightDirection::Count)> RobotIdleRows{ 4, 0, 2, 6 };
