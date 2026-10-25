#include "Credits.h"

#include "MainMenu.h"

Credits::Credits() : MenuScene("Credits", "Credits") {}

void Credits::setup()
{
    _details = { "h-AI-lucinations", "C++23 / SDL3", "Physics: Box2D", "Navigation: Vienna Game AI", "Maps: Tiled",
        "JSON: nlohmann/json", "XML: tinyxml2", "Geometry: Mapbox Earcut", "Samples \"Queer\" by Garbage.",
        "\"Dead Cities\" by Future Sound of London." };
    _items = { { "Back" } };
}

void Credits::activate(std::size_t)
{
    back();
}

void Credits::back()
{
    transitionTo(std::make_unique<MainMenu>());
}