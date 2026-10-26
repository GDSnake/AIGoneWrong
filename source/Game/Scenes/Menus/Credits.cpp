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
    _details = { "h-AI-lucinations", "Made by Goncalo Delgado, with help of AI", "Using GDFramework with SDL3", "Physics: Box2D", "Navigation: Vienna Game AI by hlavacs at GitHub", "Maps: Tiled",
        "JSON: nlohmann/json", "XML: tinyxml2", "Geometry: Mapbox Earcut",
        "", "Audio", "Audacity", "vgm-atmospheric-deepspacefrigate\" by n91music on Itch.io",
        "Samples \"Queer\" by Garbage.",
        "\"Dead Cities\" by Future Sound of London.", "\"Infiltration\" by Norihiko Hibino.",
        "From Metal Gear Solid 2: Sons of Liberty.", "\"UNATCO\" by Michiel van den Bos on Deus Ex.",
        "\"Requiem\" by Nobuo Uematsu on Final Fantasy VII.",
        "\"Costa del Sol\" by Nobuo Uematsu on Final Fantasy VII.",
        "\"Misty Bog\" by Stewart Copeland on Spyro the Dragon.",
        "", "Art",
        "Robots sprites by mekai-games at Itch.io",
        "Room sprites by livingtheindie at Itch.io",
        "Factory sprites by trevor-pupkin at Itch.io",
        "Lab sprites by blood-seller at Itch.io",
        "Speech bubbles by aspecsgaming at Itch.io",
        "Emotes by kicked-in-teeth at Itch.io",
        "Keyboard icons by justfredrik at Itch.io",
        "Energy sprites by ansimuz at Itch.io",
        "Textbox sprites by stationsupply at Itch.io",
        "NotJamToolkit19 font by not-jam at Itch.io",
        "Main character sprite by bdragon1727 at Itch.io"
    };
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