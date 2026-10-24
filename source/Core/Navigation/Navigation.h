#pragma once

#include <vector>

#include <SDL3/SDL_rect.h>
#include <box2d/id.h>

struct NavigationRequest
{
    b2WorldId world;
    SDL_FRect bounds;
    SDL_FPoint cellSize;
    SDL_FPoint colliderSize;
    SDL_FPoint start;
    SDL_FPoint target;
};

class Navigation
{
public:
    static std::vector<SDL_FPoint> findPath(const NavigationRequest& request);
};