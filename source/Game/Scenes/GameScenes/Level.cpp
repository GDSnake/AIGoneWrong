#include "Level.h"

#include <format>
#include <stdexcept>

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
    : MapScene(levelMapPath(index), std::format("Level_{:02}", index)), _index(index)
{
}

int Level::getIndex() const noexcept
{
    return _index;
}