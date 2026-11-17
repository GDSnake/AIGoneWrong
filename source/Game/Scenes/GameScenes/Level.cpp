#include "Level.h"
#include "Office.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

#include "AssetManagers/AudioManager.h"
#include "Config/Config.h"
#include "Entities/Entities.h"

namespace
{
    std::filesystem::path levelMapPath(int index)
    {
        if (index <= 0)
        {
            throw std::invalid_argument("Level index must be positive");
        }
        return std::filesystem::path("assets/Maps") / std::format("Level_{:02}.json", index);
    }
}

Level::Level(int index)
    : Level(index, Config::getInstance().getViewScale(std::format("Level_{:02}", index)))
{
