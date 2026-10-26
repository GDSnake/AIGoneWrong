#include "Office.h"

#include "Config/Config.h"
#include "Game/Scenes/Menus/MainMenu.h"

Office::Office()
    : MapScene("assets/Maps/Office.json", "Office", Config::getInstance().getViewScale("Office"))
{
}

std::unique_ptr<Scene> Office::createExitScene() const
{
    return std::make_unique<MainMenu>();
}

std::unique_ptr<Scene> Office::createRestartScene() const
{
    auto scene = std::make_unique<Office>();
    scene->setViewScale(getViewScale());
    return scene;
}