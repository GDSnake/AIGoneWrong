#include "Credits.h"

#include "AssetManagers/AudioManager.h"
#include "MainMenu.h"

Credits::Credits() : MenuScene("Credits", "Credits") {}

void Credits::init()
{
    MenuScene::init();
    if (!shouldQuit())
    {
        AudioManager::getInstance().playMusic(AudioTrack::MistyBog);
    }
}

void Credits::cleanup()
{
    AudioManager::getInstance().stopMusic();
    MenuScene::cleanup();
}

void Credits::setup()
{
    _details = { "h-AI-lucinations", "C++23 / SDL3", "Physics: Box2D", "Navigation: Vienna Game AI", "Maps: Tiled",
        "JSON: nlohmann/json", "XML: tinyxml2", "Geometry: Mapbox Earcut", "Samples \"Queer\" by Garbage.",
        "\"Dead Cities\" by Future Sound of London.", "\"Infiltration\" by Norihiko Hibino.",
        "From Metal Gear Solid 2: Sons of Liberty.", "\"UNATCO\" by Michiel van den Bos on Deus Ex.",
        "\"Requiem\" by Nobuo Uematsu on Final Fantasy VII.",
        "\"Costa del Sol\" by Nobuo Uematsu on Final Fantasy VII.",
        "\"Misty Bog\" by Stewart Copeland on Spyro the Dragon." };
    _items = { { "Back" } };
    _detailsScrollable = true;
}

void Credits::activate(std::size_t)
{
    back();
}

void Credits::back()
{
    transitionTo(std::make_unique<MainMenu>());
}