#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <span>

#include <SDL3/SDL_rect.h>
#include <box2d/id.h>

#include "Core/ParticleSystem/ParticleSystem.h"

class Sprite;
class InputManager;
class Renderer;

enum class EntityBehavior : std::uint8_t
{
  None,
  PlayerInput,
  Door,
  Count
};

struct EntityDoorSettings
{
    float openRadius = 20.0f;
    SDL_FRect entryBounds{};
    std::size_t visualIndex = 0;
};

struct DoorVisualChange
{
  std::size_t visualIndex;
  std::size_t frameIndex;
};

struct EntityAnimation
{
  std::size_t idleFirstFrame = 0;
  std::size_t idleFrameCount = 1;
  std::size_t movingFirstFrame = 0;
  std::size_t movingFrameCount = 1;
  float idleFrameDuration = 0.18f;
  float movingFrameDuration = 0.1f;

  bool operator==(const EntityAnimation&) const = default;
};

struct EntityData
{
  float x;
  float y;
    std::shared_ptr<Sprite> sprite;
    bool visible;
    std::string name;
    std::vector<ParticleHandle> particles;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    bool collidable = false;
    float colliderWidth = 12.0f;
    float colliderHeight = 6.0f;
    EntityAnimation animation;
    EntityBehavior behavior = EntityBehavior::None;
    float movementSpeed = 96.0f;
    std::size_t renderLayer = 0;
    EntityDoorSettings door;
};

struct Entities
{
  static constexpr std::size_t PlayerIndex = 0;
  static constexpr const char* PlayerName = "player";
  static constexpr float PixelsPerMeter = 32.0f;
  static constexpr float PhysicsTimeStep = 1.0f / 60.0f;

  static Entities& GetInstance();
  Entities(const Entities&) = delete;
  Entities& operator=(const Entities&) = delete;

  void InitializePhysics(std::span<const SDL_FRect> collisions, const SDL_FRect& bounds,
      std::span<const std::vector<SDL_FPoint>> frameCollisions = {});
  void SetVelocity(std::size_t index, float velocityX, float velocityY);
  void UpdateControllers(const InputManager& input);
  std::span<const DoorVisualChange> GetDoorVisualChanges() const noexcept;
  bool ConsumeSceneExitRequest() noexcept;
  void Update(float deltaTime);
  std::span<const float> GetXPositions() const noexcept;
  std::span<const float> GetYPositions() const noexcept;
  std::span<const std::size_t> GetFrames() const noexcept;
  std::span<const ParticleHandle> GetParticles(std::size_t index) const;
  void RenderEntity(std::size_t index, const Renderer& renderer, float scale, float offsetX, float offsetY) const;
  void RenderLayer(std::size_t layer, const Renderer& renderer, float scale, float offsetX, float offsetY) const;

#ifdef DEBUG
  void DrawCollisions(const Renderer& renderer, const SDL_FRect& mapBounds) const;
#endif

  void AddEntity(const EntityData& entity);

  void RemoveEntity(const EntityData& entity);

  void UpdateEntity(const EntityData& entity);

  void ClearEntities();

  EntityData GetEntity(std::size_t index) const;
  EntityData GetEntity(const std::string& name) const;
  
private:
    Entities();
    ~Entities();
    b2BodyId CreateBody(const EntityData& entity);
    void CreateStaticCollider(const SDL_FRect& bounds);
    void CreateStaticCollider(std::span<const SDL_FPoint> vertices);
    void ValidateEntity(const EntityData& entity) const;
    void ValidateIndex(std::size_t index) const;
    void UpdatePlayerInput(std::size_t index, const InputManager& input);
    void UpdateDoor(std::size_t index, const InputManager& input);

    std::vector<float> _xEntities;  
    std::vector<float> _yEntities;
    std::vector<std::shared_ptr<Sprite>> _spriteEntities;
    std::vector<bool> _visibilityEntities;
    std::vector<std::vector<ParticleHandle>> _particlesEntitiesList;
    std::vector<float> _velocityXEntities;
    std::vector<float> _velocityYEntities;
    std::vector<SDL_FPoint> _colliderSizes;
    std::vector<b2BodyId> _bodyIds;
    std::vector<EntityAnimation> _animations;
    std::vector<std::size_t> _frames;
    std::vector<float> _animationTimes;
    std::vector<bool> _moving;
    std::vector<bool> _facingLeft;
    std::vector<EntityBehavior> _behaviors;
    std::vector<float> _movementSpeeds;
    std::vector<std::size_t> _renderLayers;
    std::vector<float> _doorOpenRadii;
    std::vector<SDL_FRect> _doorEntryBounds;
    std::vector<std::size_t> _doorVisualIndices;
    std::vector<bool> _doorOpen;
    std::vector<bool> _doorEntered;
    std::vector<DoorVisualChange> _doorVisualChanges;
    std::map<std::string, std::size_t> _nameEntities;

    ParticleSystem& _particleSystem;
    b2WorldId _worldId = b2_nullWorldId;
    float _physicsAccumulator = 0.0f;
    bool _sceneExitRequested = false;
};