#include "Entities.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

#include <box2d/box2d.h>

#include "Config/Config.h"
#include "Core/Renderer.h"
#include "Entities/Sprite.h"
#include "Input/InputManager.h"

Entities::Entities() : _particleSystem(ParticleSystem::GetInstance())
{
	const auto baseSize = static_cast<std::size_t>(Config::getInstance().getConfigData().startEntitiesNumber);
	_xEntities.reserve(baseSize);
	_yEntities.reserve(baseSize);
	_spriteEntities.reserve(baseSize);
	_visibilityEntities.reserve(baseSize);
	_particlesEntitiesList.reserve(baseSize);
	_velocityXEntities.reserve(baseSize);
	_velocityYEntities.reserve(baseSize);
	_colliderSizes.reserve(baseSize);
	_bodyIds.reserve(baseSize);
	_animations.reserve(baseSize);
	_frames.reserve(baseSize);
	_animationTimes.reserve(baseSize);
	_moving.reserve(baseSize);
	_facingLeft.reserve(baseSize);
	_behaviors.reserve(baseSize);
	_movementSpeeds.reserve(baseSize);
	_renderLayers.reserve(baseSize);
	_doorOpenRadii.reserve(baseSize);
	_doorEntryBounds.reserve(baseSize);
	_doorVisualIndices.reserve(baseSize);
	_doorOpen.reserve(baseSize);
	_doorEntered.reserve(baseSize);
	_robotTypes.reserve(baseSize);
	_sightDirections.reserve(baseSize);
	_visionStates.reserve(baseSize);
	_robotStates.reserve(baseSize);
	ClearEntities();
}

Entities::~Entities()
{
	if (b2World_IsValid(_worldId))
	{
		b2DestroyWorld(_worldId);
	}
}

Entities& Entities::GetInstance()
{
	static Entities instance;
	return instance;
}

void Entities::AddEntity(const EntityData& entity)
{
	ValidateEntity(entity);

	if (_nameEntities.find(entity.name) != _nameEntities.end())
	{
		throw std::invalid_argument("An entity with this name already exists");
	}

	if (entity.name == PlayerName)
	{
		auto particles = entity.particles;
		const b2BodyId body = CreateBody(entity);
		try
		{
			_nameEntities.try_emplace(entity.name, PlayerIndex);
		}
		catch (...)
		{
			if (b2Body_IsValid(body))
			{
				b2DestroyBody(body);
			}
			throw;
		}
		_xEntities[PlayerIndex] = entity.x;
		_yEntities[PlayerIndex] = entity.y;
		_spriteEntities[PlayerIndex] = entity.sprite;
		_visibilityEntities[PlayerIndex] = entity.visible;
		_particlesEntitiesList[PlayerIndex] = std::move(particles);
		_velocityXEntities[PlayerIndex] = entity.velocityX;
		_velocityYEntities[PlayerIndex] = entity.velocityY;
		_colliderSizes[PlayerIndex] = { entity.colliderWidth, entity.colliderHeight };
		_bodyIds[PlayerIndex] = body;
		_animations[PlayerIndex] = entity.animation;
		_frames[PlayerIndex] = entity.animation.idleFirstFrame;
		_animationTimes[PlayerIndex] = 0.0f;
		_moving[PlayerIndex] = false;
		_facingLeft[PlayerIndex] = entity.velocityX < 0.0f;
		_behaviors[PlayerIndex] = entity.behavior;
		_movementSpeeds[PlayerIndex] = entity.movementSpeed;
		_renderLayers[PlayerIndex] = entity.renderLayer;
		_doorOpenRadii[PlayerIndex] = entity.door.openRadius;
		_doorEntryBounds[PlayerIndex] = entity.door.entryBounds;
		_doorVisualIndices[PlayerIndex] = entity.door.visualIndex;
		_doorOpen[PlayerIndex] = false;
		_doorEntered[PlayerIndex] = false;
		_robotTypes[PlayerIndex] = entity.robot.type;
		_sightDirections[PlayerIndex] = entity.robot.direction;
		_visionStates[PlayerIndex] = VisionState::Unaware;
		_robotStates[PlayerIndex] = MakeRobotState(entity);
		return;
	}

	const auto previousSize = _xEntities.size();
	const auto currentCapacity = _xEntities.capacity();

	if (previousSize >= currentCapacity)
	{
		const auto newCapacity = previousSize + std::max<std::size_t>(static_cast<std::size_t>(previousSize * 0.5f), 1);
		_xEntities.reserve(newCapacity);
		_yEntities.reserve(newCapacity);
		_spriteEntities.reserve(newCapacity);
		_visibilityEntities.reserve(newCapacity);
		_particlesEntitiesList.reserve(newCapacity);
		_velocityXEntities.reserve(newCapacity);
		_velocityYEntities.reserve(newCapacity);
		_colliderSizes.reserve(newCapacity);
		_bodyIds.reserve(newCapacity);
		_animations.reserve(newCapacity);
		_frames.reserve(newCapacity);
		_animationTimes.reserve(newCapacity);
		_moving.reserve(newCapacity);
		_facingLeft.reserve(newCapacity);
		_behaviors.reserve(newCapacity);
		_movementSpeeds.reserve(newCapacity);
		_renderLayers.reserve(newCapacity);
		_doorOpenRadii.reserve(newCapacity);
		_doorEntryBounds.reserve(newCapacity);
		_doorVisualIndices.reserve(newCapacity);
		_doorOpen.reserve(newCapacity);
		_doorEntered.reserve(newCapacity);
		_robotTypes.reserve(newCapacity);
		_sightDirections.reserve(newCapacity);
		_visionStates.reserve(newCapacity);
		_robotStates.reserve(newCapacity);
	}

	const b2BodyId body = CreateBody(entity);
	try
	{
		_xEntities.emplace_back(entity.x);
		_yEntities.emplace_back(entity.y);
		_spriteEntities.emplace_back(entity.sprite);
		_visibilityEntities.emplace_back(entity.visible);
		_particlesEntitiesList.emplace_back(entity.particles);
		_velocityXEntities.emplace_back(entity.velocityX);
		_velocityYEntities.emplace_back(entity.velocityY);
		_colliderSizes.push_back({ entity.colliderWidth, entity.colliderHeight });
		_bodyIds.push_back(body);
		_animations.push_back(entity.animation);
		_frames.push_back(entity.animation.idleFirstFrame);
		_animationTimes.push_back(0.0f);
		_moving.push_back(false);
		_facingLeft.push_back(entity.velocityX < 0.0f);
		_behaviors.push_back(entity.behavior);
		_movementSpeeds.push_back(entity.movementSpeed);
		_renderLayers.push_back(entity.renderLayer);
		_doorOpenRadii.push_back(entity.door.openRadius);
		_doorEntryBounds.push_back(entity.door.entryBounds);
		_doorVisualIndices.push_back(entity.door.visualIndex);
		_doorOpen.push_back(false);
		_doorEntered.push_back(false);
		_robotTypes.push_back(entity.robot.type);
		_sightDirections.push_back(entity.robot.direction);
		_visionStates.push_back(VisionState::Unaware);
		_robotStates.push_back(MakeRobotState(entity));
		_nameEntities.try_emplace(entity.name, previousSize);
	}
	catch (...)
	{
		_xEntities.resize(previousSize);
		_yEntities.resize(previousSize);
		_spriteEntities.resize(previousSize);
		_visibilityEntities.resize(previousSize);
		_particlesEntitiesList.resize(previousSize);
		_velocityXEntities.resize(previousSize);
		_velocityYEntities.resize(previousSize);
		_colliderSizes.resize(previousSize);
		_bodyIds.resize(previousSize);
		_animations.resize(previousSize);
		_frames.resize(previousSize);
		_animationTimes.resize(previousSize);
		_moving.resize(previousSize);
		_facingLeft.resize(previousSize);
		_behaviors.resize(previousSize);
		_movementSpeeds.resize(previousSize);
		_renderLayers.resize(previousSize);
		_doorOpenRadii.resize(previousSize);
		_doorEntryBounds.resize(previousSize);
		_doorVisualIndices.resize(previousSize);
		_doorOpen.resize(previousSize);
		_doorEntered.resize(previousSize);
		_robotTypes.resize(previousSize);
		_sightDirections.resize(previousSize);
		_visionStates.resize(previousSize);
		_robotStates.resize(previousSize);
		if (b2Body_IsValid(body))
		{
			b2DestroyBody(body);
		}
		throw;
	}
}

void Entities::RemoveEntity(const EntityData& entity)
{
	const auto found = _nameEntities.find(entity.name);
	if (found == _nameEntities.end())
	{
		return;
	}

	const auto index = found->second;
	if (b2Body_IsValid(_bodyIds[index]))
	{
		b2DestroyBody(_bodyIds[index]);
	}
	if (index == PlayerIndex)
	{
		_xEntities[PlayerIndex] = 0.0f;
		_yEntities[PlayerIndex] = 0.0f;
		_spriteEntities[PlayerIndex].reset();
		_visibilityEntities[PlayerIndex] = false;
		_particlesEntitiesList[PlayerIndex].clear();
		_velocityXEntities[PlayerIndex] = 0.0f;
		_velocityYEntities[PlayerIndex] = 0.0f;
		_colliderSizes[PlayerIndex] = { 12.0f, 6.0f };
		_bodyIds[PlayerIndex] = b2_nullBodyId;
		_animations[PlayerIndex] = {};
		_frames[PlayerIndex] = 0;
		_animationTimes[PlayerIndex] = 0.0f;
		_moving[PlayerIndex] = false;
		_facingLeft[PlayerIndex] = false;
		_behaviors[PlayerIndex] = EntityBehavior::None;
		_movementSpeeds[PlayerIndex] = 96.0f;
		_renderLayers[PlayerIndex] = 0;
		_doorOpenRadii[PlayerIndex] = 64.0f;
		_doorEntryBounds[PlayerIndex] = {};
		_doorVisualIndices[PlayerIndex] = 0;
		_doorOpen[PlayerIndex] = false;
		_doorEntered[PlayerIndex] = false;
		_robotTypes[PlayerIndex] = RobotType::None;
		_sightDirections[PlayerIndex] = SightDirection::Down;
		_visionStates[PlayerIndex] = VisionState::Unaware;
		_robotStates[PlayerIndex] = {};
		_nameEntities.erase(found);
		return;
	}

	_xEntities.erase(_xEntities.begin() + index);
	_yEntities.erase(_yEntities.begin() + index);
	_spriteEntities.erase(_spriteEntities.begin() + index);
	_visibilityEntities.erase(_visibilityEntities.begin() + index);
	_particlesEntitiesList.erase(_particlesEntitiesList.begin() + index);
	_velocityXEntities.erase(_velocityXEntities.begin() + index);
	_velocityYEntities.erase(_velocityYEntities.begin() + index);
	_colliderSizes.erase(_colliderSizes.begin() + index);
	_bodyIds.erase(_bodyIds.begin() + index);
	_animations.erase(_animations.begin() + index);
	_frames.erase(_frames.begin() + index);
	_animationTimes.erase(_animationTimes.begin() + index);
	_moving.erase(_moving.begin() + index);
	_facingLeft.erase(_facingLeft.begin() + index);
	_behaviors.erase(_behaviors.begin() + index);
	_movementSpeeds.erase(_movementSpeeds.begin() + index);
	_renderLayers.erase(_renderLayers.begin() + index);
	_doorOpenRadii.erase(_doorOpenRadii.begin() + index);
	_doorEntryBounds.erase(_doorEntryBounds.begin() + index);
	_doorVisualIndices.erase(_doorVisualIndices.begin() + index);
	_doorOpen.erase(_doorOpen.begin() + index);
	_doorEntered.erase(_doorEntered.begin() + index);
	_robotTypes.erase(_robotTypes.begin() + index);
	_sightDirections.erase(_sightDirections.begin() + index);
	_visionStates.erase(_visionStates.begin() + index);
	_robotStates.erase(_robotStates.begin() + index);
	_nameEntities.erase(found);
	for (auto& nameIndex : _nameEntities)
	{
		if (nameIndex.second > index)
		{
			--nameIndex.second;
		}
	}
}

void Entities::UpdateEntity(const EntityData& entity)
{
	ValidateEntity(entity);

	const auto found = _nameEntities.find(entity.name);
	if (found == _nameEntities.end())
	{
		throw std::out_of_range("Entity name was not found");
	}

	const auto index = found->second;
	const bool hadBody = b2Body_IsValid(_bodyIds[index]);
	const bool replaceBody = entity.collidable && (!hadBody ||
		_colliderSizes[index].x != entity.colliderWidth || _colliderSizes[index].y != entity.colliderHeight ||
		(_behaviors[index] == EntityBehavior::Door) != (entity.behavior == EntityBehavior::Door) ||
		(_behaviors[index] == EntityBehavior::Robot) != (entity.behavior == EntityBehavior::Robot));
	const bool resetRobot = replaceBody || _robotTypes[index] != entity.robot.type ||
		_robotStates[index].axisDirection != entity.robot.direction ||
		_xEntities[index] != entity.x || _yEntities[index] != entity.y;
	const b2BodyId body = replaceBody ? CreateBody(entity) : _bodyIds[index];
	try
	{
		_particlesEntitiesList[index] = entity.particles;
	}
	catch (...)
	{
		if (replaceBody)
		{
			b2DestroyBody(body);
		}
		throw;
	}
	if (hadBody && (replaceBody || !entity.collidable))
	{
		b2DestroyBody(_bodyIds[index]);
	}
	_bodyIds[index] = entity.collidable ? body : b2_nullBodyId;
	if (entity.collidable && (_xEntities[index] != entity.x || _yEntities[index] != entity.y))
	{
		b2Body_SetTransform(body, { entity.x / PixelsPerMeter,
			(entity.y - entity.colliderHeight * 0.5f) / PixelsPerMeter }, { 1.0f, 0.0f });
	}
	_xEntities[index] = entity.x;
	_yEntities[index] = entity.y;
	_spriteEntities[index] = entity.sprite;
	_visibilityEntities[index] = entity.visible;
	_colliderSizes[index] = { entity.colliderWidth, entity.colliderHeight };
	if (_animations[index] != entity.animation)
	{
		_frames[index] = entity.animation.idleFirstFrame;
		_animationTimes[index] = 0.0f;
		_moving[index] = false;
	}
	_animations[index] = entity.animation;
	if (_behaviors[index] != entity.behavior)
	{
		_doorOpen[index] = false;
		_doorEntered[index] = false;
	}
	_behaviors[index] = entity.behavior;
	_movementSpeeds[index] = entity.movementSpeed;
	_renderLayers[index] = entity.renderLayer;
	_doorOpenRadii[index] = entity.door.openRadius;
	_doorEntryBounds[index] = entity.door.entryBounds;
	_doorVisualIndices[index] = entity.door.visualIndex;
	_robotTypes[index] = entity.robot.type;
	_sightDirections[index] = entity.robot.direction;
	_visionStates[index] = VisionState::Unaware;
	if (resetRobot)
	{
		_robotStates[index] = MakeRobotState(entity);
	}
	if (entity.behavior == EntityBehavior::Door && _doorOpen[index])
	{
		_frames[index] = 1;
		b2Body_Disable(_bodyIds[index]);
	}
	SetVelocity(index, entity.velocityX, entity.velocityY);
}

void Entities::ClearEntities()
{
	if (b2World_IsValid(_worldId))
	{
		b2DestroyWorld(_worldId);
	}
	_worldId = b2_nullWorldId;
	_physicsAccumulator = 0.0f;
	_tileSize = { 32.0f, 32.0f };
	_mapBounds = {};
	_sceneExitRequested = false;
	_playerCaught = false;
	_doorVisualChanges.clear();
	_xEntities.assign(PlayerIndex + 1, 0.0f);
	_yEntities.assign(PlayerIndex + 1, 0.0f);
	_spriteEntities.assign(PlayerIndex + 1, nullptr);
	_visibilityEntities.assign(PlayerIndex + 1, false);
	_particlesEntitiesList.clear();
	_particlesEntitiesList.resize(PlayerIndex + 1);
	_velocityXEntities.assign(PlayerIndex + 1, 0.0f);
	_velocityYEntities.assign(PlayerIndex + 1, 0.0f);
	_colliderSizes.assign(PlayerIndex + 1, { 12.0f, 6.0f });
	_bodyIds.assign(PlayerIndex + 1, b2_nullBodyId);
	_animations.assign(PlayerIndex + 1, {});
	_frames.assign(PlayerIndex + 1, 0);
	_animationTimes.assign(PlayerIndex + 1, 0.0f);
	_moving.assign(PlayerIndex + 1, false);
	_facingLeft.assign(PlayerIndex + 1, false);
	_behaviors.assign(PlayerIndex + 1, EntityBehavior::None);
	_movementSpeeds.assign(PlayerIndex + 1, 96.0f);
	_renderLayers.assign(PlayerIndex + 1, 0);
	_doorOpenRadii.assign(PlayerIndex + 1, 64.0f);
	_doorEntryBounds.assign(PlayerIndex + 1, {});
	_doorVisualIndices.assign(PlayerIndex + 1, 0);
	_doorOpen.assign(PlayerIndex + 1, false);
	_doorEntered.assign(PlayerIndex + 1, false);
	_robotTypes.assign(PlayerIndex + 1, RobotType::None);
	_sightDirections.assign(PlayerIndex + 1, SightDirection::Down);
	_visionStates.assign(PlayerIndex + 1, VisionState::Unaware);
	_robotStates.assign(PlayerIndex + 1, {});
	_personalityEffectFrames.clear();
	_teleportEffectFrames.clear();
	_personalityIntervalOverride = {};
	_nameEntities.clear();
}

EntityData Entities::GetEntity(const std::size_t index) const
{
	if (index >= _xEntities.size())
	{
		throw std::out_of_range("Entity index is out of range");
	}

	const auto found = std::find_if(_nameEntities.begin(), _nameEntities.end(),
		[index](const auto& nameIndex) { return nameIndex.second == index; });
	if (found == _nameEntities.end())
	{
		throw std::out_of_range("Entity slot is empty");
	}

	return {
		_xEntities[index],
		_yEntities[index],
		_spriteEntities[index],
		_visibilityEntities[index],
		found->first,
		_particlesEntitiesList[index],
		_velocityXEntities[index],
		_velocityYEntities[index],
		b2Body_IsValid(_bodyIds[index]),
		_colliderSizes[index].x,
		_colliderSizes[index].y,
		_animations[index],
		_behaviors[index],
		_movementSpeeds[index],
		_renderLayers[index],
		{ _doorOpenRadii[index], _doorEntryBounds[index], _doorVisualIndices[index] },
		{ _robotTypes[index], _robotTypes[index] == RobotType::None ? _sightDirections[index] : _robotStates[index].axisDirection },
		_robotStates[index].movingSprite
	};
}

EntityData Entities::GetEntity(const std::string& name) const
{
	const auto found = _nameEntities.find(name);
	if (found == _nameEntities.end())
	{
		throw std::out_of_range("Entity name was not found");
	}

	const auto index = found->second;
	return {
		_xEntities[index],
		_yEntities[index],
		_spriteEntities[index],
		_visibilityEntities[index],
		found->first,
		_particlesEntitiesList[index],
		_velocityXEntities[index],
		_velocityYEntities[index],
		b2Body_IsValid(_bodyIds[index]),
		_colliderSizes[index].x,
		_colliderSizes[index].y,
		_animations[index],
		_behaviors[index],
		_movementSpeeds[index],
		_renderLayers[index],
		{ _doorOpenRadii[index], _doorEntryBounds[index], _doorVisualIndices[index] },
		{ _robotTypes[index], _robotTypes[index] == RobotType::None ? _sightDirections[index] : _robotStates[index].axisDirection },
		_robotStates[index].movingSprite
	};
}

void Entities::InitializePhysics(std::span<const SDL_FRect> collisions, const SDL_FRect& bounds,
	std::span<const std::vector<SDL_FPoint>> frameCollisions, SDL_FPoint tileSize)
{
	if (!_nameEntities.empty())
	{
		throw std::logic_error("Initialize physics before adding scene entities");
	}
	if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) || !std::isfinite(bounds.w) ||
		!std::isfinite(bounds.h) || bounds.w <= 0.0f || bounds.h <= 0.0f ||
		!std::isfinite(tileSize.x) || !std::isfinite(tileSize.y) || tileSize.x <= 0.0f || tileSize.y <= 0.0f)
	{
		throw std::invalid_argument("Physics map bounds and tile sizes must be finite and positive");
	}
	ClearEntities();
	_tileSize = tileSize;
	_mapBounds = bounds;
	b2WorldDef definition = b2DefaultWorldDef();
	definition.gravity = { 0.0f, 0.0f };
	_worldId = b2CreateWorld(&definition);
	try
	{
		for (const SDL_FRect& collision : collisions)
		{
			CreateStaticCollider(collision);
		}
		for (const auto& vertices : frameCollisions)
		{
			CreateStaticCollider(vertices);
		}
		CreateStaticCollider({ bounds.x - PixelsPerMeter, bounds.y - PixelsPerMeter,
			bounds.w + 2.0f * PixelsPerMeter, PixelsPerMeter });
		CreateStaticCollider({ bounds.x - PixelsPerMeter, bounds.y + bounds.h,
			bounds.w + 2.0f * PixelsPerMeter, PixelsPerMeter });
		CreateStaticCollider({ bounds.x - PixelsPerMeter, bounds.y, PixelsPerMeter, bounds.h });
		CreateStaticCollider({ bounds.x + bounds.w, bounds.y, PixelsPerMeter, bounds.h });
	}
	catch (...)
	{
		ClearEntities();
		throw;
	}
}

void Entities::CreateStaticCollider(const SDL_FRect& bounds)
{
	if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) || !std::isfinite(bounds.w) ||
		!std::isfinite(bounds.h) || bounds.w <= 0.0f || bounds.h <= 0.0f)
	{
		throw std::invalid_argument("Static collider bounds must be finite and positive");
	}
	b2BodyDef definition = b2DefaultBodyDef();
	definition.position = { (bounds.x + bounds.w * 0.5f) / PixelsPerMeter,
		(bounds.y + bounds.h * 0.5f) / PixelsPerMeter };
	const b2BodyId body = b2CreateBody(_worldId, &definition);
	b2ShapeDef shape = b2DefaultShapeDef();
	shape.material.friction = 0.0f;
	const b2Polygon polygon = b2MakeBox(bounds.w * 0.5f / PixelsPerMeter, bounds.h * 0.5f / PixelsPerMeter);
	b2CreatePolygonShape(body, &shape, &polygon);
}

void Entities::CreateStaticCollider(std::span<const SDL_FPoint> vertices)
{
	if (vertices.size() < 3 || vertices.size() > B2_MAX_POLYGON_VERTICES)
	{
		throw std::invalid_argument("A convex static collider needs three to eight vertices");
	}
	SDL_FPoint center{};
	for (const SDL_FPoint& vertex : vertices)
	{
		if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y))
		{
			throw std::invalid_argument("Static collider vertices must be finite");
		}
		center.x += vertex.x / static_cast<float>(vertices.size());
		center.y += vertex.y / static_cast<float>(vertices.size());
	}
	std::array<b2Vec2, B2_MAX_POLYGON_VERTICES> points{};
	for (std::size_t index = 0; index < vertices.size(); ++index)
	{
		points[index] = { (vertices[index].x - center.x) / PixelsPerMeter,
			(vertices[index].y - center.y) / PixelsPerMeter };
	}
	const b2Hull hull = b2ComputeHull(points.data(), static_cast<int>(vertices.size()));
	if (hull.count < 3)
	{
		throw std::invalid_argument("Static collider polygon is degenerate or too small for physics");
	}
	b2BodyDef definition = b2DefaultBodyDef();
	definition.position = { center.x / PixelsPerMeter, center.y / PixelsPerMeter };
	const b2BodyId body = b2CreateBody(_worldId, &definition);
	b2ShapeDef shape = b2DefaultShapeDef();
	shape.material.friction = 0.0f;
	const b2Polygon polygon = b2MakePolygon(&hull, 0.0f);
	b2CreatePolygonShape(body, &shape, &polygon);
}

b2BodyId Entities::CreateBody(const EntityData& entity)
{
	if (!entity.collidable)
	{
		return b2_nullBodyId;
	}
	if (!b2World_IsValid(_worldId))
	{
		throw std::logic_error("Initialize physics before adding a collidable entity");
	}
	b2BodyDef definition = b2DefaultBodyDef();
	definition.type = entity.behavior == EntityBehavior::Door || entity.behavior == EntityBehavior::Robot
		? b2_staticBody : b2_dynamicBody;
	definition.fixedRotation = true;
	definition.position = { entity.x / PixelsPerMeter,
		(entity.y - entity.colliderHeight * 0.5f) / PixelsPerMeter };
	definition.linearVelocity = { entity.velocityX / PixelsPerMeter, entity.velocityY / PixelsPerMeter };
	const b2BodyId body = b2CreateBody(_worldId, &definition);
	b2ShapeDef shape = b2DefaultShapeDef();
	shape.material.friction = 0.0f;
	shape.filter.categoryBits = entity.behavior == EntityBehavior::Door ? 1u : 2u;
	const b2Polygon polygon = b2MakeBox(entity.colliderWidth * 0.5f / PixelsPerMeter,
		entity.colliderHeight * 0.5f / PixelsPerMeter);
	b2CreatePolygonShape(body, &shape, &polygon);
	return body;
}

void Entities::SetVelocity(std::size_t index, float velocityX, float velocityY)
{
	ValidateIndex(index);
	if (!std::isfinite(velocityX) || !std::isfinite(velocityY))
	{
		throw std::invalid_argument("Entity velocity must be finite");
	}
	_velocityXEntities[index] = velocityX;
	_velocityYEntities[index] = velocityY;
	if (velocityX != 0.0f)
	{
		_facingLeft[index] = velocityX < 0.0f;
	}
	if (b2Body_IsValid(_bodyIds[index]))
	{
		b2Body_SetLinearVelocity(_bodyIds[index], { velocityX / PixelsPerMeter, velocityY / PixelsPerMeter });
	}
}

void Entities::UpdateControllers(const InputManager& input, bool playerInputEnabled)
{
	using ControllerUpdate = void (Entities::*)(std::size_t, const InputManager&);
	static constexpr std::array<ControllerUpdate, static_cast<std::size_t>(EntityBehavior::Count)> controllers{
		nullptr,
		&Entities::UpdatePlayerInput,
		&Entities::UpdateDoor,
		nullptr
	};
	_doorVisualChanges.clear();
	for (std::size_t index = 0; index < _behaviors.size(); ++index)
	{
		if (!playerInputEnabled && index == PlayerIndex && _behaviors[index] == EntityBehavior::PlayerInput)
		{
			SetVelocity(index, 0.0f, 0.0f);
			continue;
		}
		if (const auto update = controllers[static_cast<std::size_t>(_behaviors[index])])
		{
			(this->*update)(index, input);
		}
	}
}

void Entities::UpdatePlayerInput(std::size_t index, const InputManager& input)
{
	const auto down = [&input](SDL_Scancode key)
	{
		return input.isKeyPressed(key) || input.isKeyHeld(key);
	};
	const float directionX = static_cast<float>(down(SDL_SCANCODE_D) || down(SDL_SCANCODE_RIGHT)) -
		static_cast<float>(down(SDL_SCANCODE_A) || down(SDL_SCANCODE_LEFT));
	const float directionY = static_cast<float>(down(SDL_SCANCODE_S) || down(SDL_SCANCODE_DOWN)) -
		static_cast<float>(down(SDL_SCANCODE_W) || down(SDL_SCANCODE_UP));
	const float length = std::sqrt(directionX * directionX + directionY * directionY);
	const float speed = length > 0.0f ? _movementSpeeds[index] / length : 0.0f;
	SetVelocity(index, directionX * speed, directionY * speed);
}

void Entities::UpdateDoor(std::size_t index, const InputManager&)
{
	if (!_nameEntities.contains(PlayerName))
	{
		return;
	}
	const float playerX = _xEntities[PlayerIndex];
	const float playerY = _yEntities[PlayerIndex];
	const float distanceX = playerX - _xEntities[index];
	const float distanceY = playerY - _yEntities[index];
	const float radius = _doorOpenRadii[index];
	const bool inRadius = distanceX * distanceX + distanceY * distanceY <= radius * radius;
	if (_doorOpen[index] != inRadius && !_doorEntered[index])
	{
		const std::size_t frameIndex = inRadius ? 1 : 0;
		_doorVisualChanges.push_back({ _doorVisualIndices[index], frameIndex });
		_doorOpen[index] = inRadius;
		_frames[index] = frameIndex;
		if (inRadius)
		{
			b2Body_Disable(_bodyIds[index]);
		}
		else
		{
			b2Body_Enable(_bodyIds[index]);
		}
	}
	const SDL_FRect& entry = _doorEntryBounds[index];
	if (_doorOpen[index] && !_doorEntered[index] && entry.w > 0.0f && entry.h > 0.0f &&
		playerX >= entry.x && playerX <= entry.x + entry.w && playerY >= entry.y && playerY <= entry.y + entry.h)
	{
		_doorEntered[index] = true;
		_sceneExitRequested = true;
	}
}

std::span<const DoorVisualChange> Entities::GetDoorVisualChanges() const noexcept
{
	return _doorVisualChanges;
}

bool Entities::ConsumeSceneExitRequest() noexcept
{
	return std::exchange(_sceneExitRequested, false);
}

bool Entities::ConsumePlayerCaught() noexcept
{
	return std::exchange(_playerCaught, false);
}

void Entities::Update(float deltaTime, bool robotPersonalityChangesEnabled)
{
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
	{
		return;
	}
	_physicsAccumulator += std::min(deltaTime, 0.25f);
	while (_physicsAccumulator >= PhysicsTimeStep)
	{
		for (std::size_t index = 0; index < _robotTypes.size(); ++index)
		{
			if (_robotTypes[index] != RobotType::None)
			{
				UpdateRobot(index, PhysicsTimeStep, robotPersonalityChangesEnabled);
			}
		}
		if (b2World_IsValid(_worldId))
		{
			b2World_Step(_worldId, PhysicsTimeStep, 4);
		}
		for (std::size_t index = 0; index < _xEntities.size(); ++index)
		{
			if (b2Body_IsValid(_bodyIds[index]))
			{
				const b2Vec2 position = b2Body_GetPosition(_bodyIds[index]);
				const b2Vec2 velocity = b2Body_GetLinearVelocity(_bodyIds[index]);
				_xEntities[index] = position.x * PixelsPerMeter;
				_yEntities[index] = position.y * PixelsPerMeter + _colliderSizes[index].y * 0.5f;
				_velocityXEntities[index] = velocity.x * PixelsPerMeter;
				_velocityYEntities[index] = velocity.y * PixelsPerMeter;
			}
			else
			{
				_xEntities[index] += _velocityXEntities[index] * PhysicsTimeStep;
				_yEntities[index] += _velocityYEntities[index] * PhysicsTimeStep;
			}
		}
		if (robotPersonalityChangesEnabled && CheckPlayerCapture())
		{
			_playerCaught = true;
			_physicsAccumulator = 0.0f;
			break;
		}
		_physicsAccumulator -= PhysicsTimeStep;
	}

	for (std::size_t index = 0; index < _spriteEntities.size(); ++index)
	{
		if (!_spriteEntities[index] || _behaviors[index] == EntityBehavior::Door)
		{
			continue;
		}
		const float velocityX = _velocityXEntities[index];
		const float velocityY = _velocityYEntities[index];
		const bool moving = velocityX * velocityX + velocityY * velocityY > 1.0f;
		if (moving != _moving[index])
		{
			_animationTimes[index] = 0.0f;
			_moving[index] = moving;
		}
		const EntityAnimation& animation = _animations[index];
		const std::size_t firstFrame = moving ? animation.movingFirstFrame : animation.idleFirstFrame;
		const std::size_t frameCount = moving ? animation.movingFrameCount : animation.idleFrameCount;
		const float frameDuration = moving ? animation.movingFrameDuration : animation.idleFrameDuration;
		_animationTimes[index] = std::fmod(_animationTimes[index] + std::min(deltaTime, 0.25f), frameCount * frameDuration);
		_frames[index] = firstFrame + std::min(frameCount - 1, static_cast<std::size_t>(_animationTimes[index] / frameDuration));
	}
}

std::span<const float> Entities::GetXPositions() const noexcept
{
	return _xEntities;
}

std::span<const float> Entities::GetYPositions() const noexcept
{
	return _yEntities;
}

std::span<const std::size_t> Entities::GetFrames() const noexcept
{
	return _frames;
}

std::span<const RobotType> Entities::GetRobotTypes() const noexcept
{
	return _robotTypes;
}

std::span<const VisionState> Entities::GetVisionStates() const noexcept
{
	return _visionStates;
}

std::span<const ParticleHandle> Entities::GetParticles(std::size_t index) const
{
	ValidateIndex(index);
	return _particlesEntitiesList[index];
}

void Entities::RenderEntity(std::size_t index, const Renderer& renderer, float scale, float offsetX, float offsetY) const
{
	ValidateIndex(index);
	if (!_visibilityEntities[index] || !_spriteEntities[index])
	{
		return;
	}
	const SDL_FRect& source = _spriteEntities[index]->getSourceRect(_frames[index]);
	const RobotProfile* profile = _robotTypes[index] != RobotType::None ?
		&Config::getInstance().getRobotProfile(_robotTypes[index]) : nullptr;
	const float spriteScale = profile ? profile->spriteScale : (index == PlayerIndex ? PlayerScale : 1.0f);
	const float pivotX = profile ? profile->pivotX : source.w * 0.5f;
	const float pivotY = profile ? profile->pivotY : source.h * 0.75f;
	const SDL_FRect destination{
		offsetX + (_xEntities[index] - pivotX * spriteScale) * scale,
		offsetY + (_yEntities[index] - pivotY * spriteScale) * scale,
		source.w * spriteScale * scale,
		source.h * spriteScale * scale
	};
	renderer.drawTextureRotated(_spriteEntities[index]->getTexture().get(), destination, &source,
		0.0, _facingLeft[index] ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
	const auto& state = _robotStates[index];
	const bool teleporting = state.teleportPhase == RobotRuntimeState::TeleportPhase::Disappearing ||
		state.teleportPhase == RobotRuntimeState::TeleportPhase::Appearing;
	const auto& effectFrames = teleporting ? _teleportEffectFrames : _personalityEffectFrames;
	if (profile && (state.phase == RobotRuntimeState::Phase::Changing || teleporting) && !effectFrames.empty())
	{
		const float elapsed = teleporting ? state.teleportTime : state.transitionTime;
		const float frameSeconds = teleporting ? profile->behavior.teleportFrameSeconds : profile->behavior.transitionFrameSeconds;
		const auto frame = std::min(effectFrames.size() - 1, static_cast<std::size_t>(elapsed / frameSeconds));
		const auto& effect = effectFrames[frame];
		const auto& effectSource = effect->getSourceRect(0);
		const float size = std::max(source.w, source.h) * spriteScale * scale;
		renderer.drawTexture(effect->getTexture().get(),
			{ offsetX + _xEntities[index] * scale - size * 0.5f,
				offsetY + (_yEntities[index] - (pivotY - source.h * 0.5f) * spriteScale) * scale - size * 0.5f,
				size, size }, &effectSource);
	}
}

void Entities::RenderLayer(std::size_t layer, const Renderer& renderer, float scale, float offsetX, float offsetY) const
{
	for (std::size_t index = 0; index < _robotTypes.size(); ++index)
	{
		if (_renderLayers[index] == layer && _visibilityEntities[index] && _robotTypes[index] != RobotType::None)
		{
			RenderVision(index, renderer, scale, offsetX, offsetY);
		}
	}
	std::vector<std::size_t> drawOrder;
	drawOrder.reserve(_spriteEntities.size());
	for (std::size_t index = 0; index < _spriteEntities.size(); ++index)
	{
		if (_renderLayers[index] == layer && _visibilityEntities[index] && _spriteEntities[index])
		{
			drawOrder.push_back(index);
		}
	}
	std::stable_sort(drawOrder.begin(), drawOrder.end(), [this](std::size_t left, std::size_t right)
	{
		return _yEntities[left] < _yEntities[right];
	});
	for (const auto index : drawOrder)
	{
		RenderEntity(index, renderer, scale, offsetX, offsetY);
	}
}

void Entities::RenderVision(std::size_t index, const Renderer& renderer, float scale, float offsetX, float offsetY) const
{
	const RobotProfile& profile = Config::getInstance().getRobotProfile(_robotTypes[index]);
	const auto& state = _robotStates[index];
	if (state.teleportPhase != RobotRuntimeState::TeleportPhase::None)
	{
		return;
	}
	const SDL_FPoint origin{ _xEntities[index], _yEntities[index] - _colliderSizes[index].y * 0.5f };
	static constexpr std::array<float, 4> angles{
		-std::numbers::pi_v<float> * 0.5f, std::numbers::pi_v<float> * 0.5f, std::numbers::pi_v<float>, 0.0f
	};
	const float halfAngle = profile.visionAngleDegrees * std::numbers::pi_v<float> / 360.0f;
	const float facing = RobotPersonalities[static_cast<std::size_t>(state.personality)].rotates ?
		state.rotationAngle : angles[static_cast<std::size_t>(_sightDirections[index])];
	constexpr std::size_t segments = 32;
	std::array<SDL_FPoint, segments + 2> points{};
	points[0] = { offsetX + origin.x * scale, offsetY + origin.y * scale };
	for (std::size_t segment = 0; segment <= segments; ++segment)
	{
		const float angle = facing - halfAngle + 2.0f * halfAngle * static_cast<float>(segment) / segments;
		const SDL_FPoint target{ origin.x + std::cos(angle) * profile.visionRadius,
			origin.y + std::sin(angle) * profile.visionRadius };
		const float fraction = SightFraction(origin, target);
		points[segment + 1] = { offsetX + (origin.x + (target.x - origin.x) * fraction) * scale,
			offsetY + (origin.y + (target.y - origin.y) * fraction) * scale };
	}
	const auto personality = static_cast<std::size_t>(state.personality);
	const bool friendly = RobotPersonalities[personality].friendly;
	SDL_Color color = friendly ? SDL_Color{ 245, 110, 190, 75 } : SDL_Color{ 50, 210, 100, 70 };
	if (!friendly && _visionStates[index] == VisionState::Peripheral)
	{
		color = { 255, 210, 50, 85 };
	}
	else if (!friendly && _visionStates[index] == VisionState::Detected)
	{
		color = { 235, 65, 70, 90 };
	}
	if (state.phase == RobotRuntimeState::Phase::Changing)
	{
		const bool orange = static_cast<std::size_t>(state.transitionTime / profile.behavior.transitionBlinkSeconds) % 2 != 0;
		color = orange ? SDL_Color{ 255, 150, 50, 85 } : SDL_Color{ 245, 110, 190, 75 };
	}
	renderer.drawTriangleFan(points, color);
}

#ifdef DEBUG
void Entities::DrawCollisions(const Renderer& renderer, const RenderView& view) const
{
	if (!b2World_IsValid(_worldId))
	{
		return;
	}
	struct DrawContext
	{
		const Renderer& renderer;
		float scale;
		float offsetX;
		float offsetY;
	};
	DrawContext context{ renderer, view.scale * PixelsPerMeter, view.offsetX, view.offsetY };
	b2DebugDraw draw = b2DefaultDebugDraw();
	draw.drawShapes = true;
	draw.context = &context;
	draw.DrawSolidPolygonFcn = [](b2Transform transform, const b2Vec2* vertices, int vertexCount,
		float, b2HexColor color, void* rawContext)
	{
		if (vertexCount < 2)
		{
			return;
		}
		const auto& context = *static_cast<const DrawContext*>(rawContext);
		std::vector<SDL_FPoint> points(static_cast<std::size_t>(vertexCount) + 1);
		for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
		{
			const b2Vec2 point = b2TransformPoint(transform, vertices[vertexIndex]);
			points[vertexIndex] = { context.offsetX + point.x * context.scale,
				context.offsetY + point.y * context.scale };
		}
		points.back() = points.front();
		context.renderer.drawLines(points, { static_cast<Uint8>((color >> 16) & 0xff),
			static_cast<Uint8>((color >> 8) & 0xff), static_cast<Uint8>(color & 0xff), 255 });
	};
	b2World_Draw(_worldId, &draw);
}
#endif

void Entities::ValidateEntity(const EntityData& entity) const
{
	if (entity.behavior == EntityBehavior::Robot || entity.robot.type != RobotType::None)
	{
		if (entity.behavior != EntityBehavior::Robot || entity.name == PlayerName || !entity.collidable ||
			entity.robot.type == RobotType::None || entity.robot.type >= RobotType::Count ||
			entity.robot.direction >= SightDirection::Count || !entity.sprite || entity.sprite->getFrameCount() < 64)
		{
			throw std::invalid_argument("Robot entities need a valid type, facing direction, collider, and directional sprite sheet");
		}
		if (entity.robotMovingSprite && entity.robotMovingSprite->getFrameCount() < 64)
		{
			throw std::invalid_argument("Robot walking sprites need all eight directional animation rows");
		}
	}
	if (entity.behavior == EntityBehavior::Door)
	{
		const SDL_FRect& entry = entity.door.entryBounds;
		if (entity.name == PlayerName || !entity.collidable || !entity.sprite || entity.sprite->getFrameCount() < 2 ||
			!std::isfinite(entity.door.openRadius) || entity.door.openRadius <= 0.0f ||
			!std::isfinite(entry.x) || !std::isfinite(entry.y) || !std::isfinite(entry.w) || !std::isfinite(entry.h) ||
			entry.w < 0.0f || entry.h < 0.0f || (entry.w == 0.0f) != (entry.h == 0.0f))
		{
			throw std::invalid_argument("Door entities need a two-frame sprite, collider, positive radius, and valid entry bounds");
		}
	}
	if (entity.behavior >= EntityBehavior::Count || !std::isfinite(entity.movementSpeed) || entity.movementSpeed < 0.0f)
	{
		throw std::invalid_argument("Entity behavior and movement speed must be valid");
	}
	if (!std::isfinite(entity.x) || !std::isfinite(entity.y) ||
		!std::isfinite(entity.velocityX) || !std::isfinite(entity.velocityY) ||
		!std::isfinite(entity.colliderWidth) || !std::isfinite(entity.colliderHeight) ||
		entity.colliderWidth <= 0.0f || entity.colliderHeight <= 0.0f)
	{
		throw std::invalid_argument("Entity positions, velocities, and collider dimensions must be valid");
	}
	const EntityAnimation& animation = entity.animation;
	if (animation.idleFrameCount == 0 || animation.movingFrameCount == 0 ||
		!std::isfinite(animation.idleFrameDuration) || !std::isfinite(animation.movingFrameDuration) ||
		animation.idleFrameDuration <= 0.0f || animation.movingFrameDuration <= 0.0f)
	{
		throw std::invalid_argument("Entity animation clips must have frames and positive finite durations");
	}
	if (entity.sprite)
	{
		const auto count = entity.sprite->getFrameCount();
		if (animation.idleFirstFrame >= count || animation.idleFrameCount > count - animation.idleFirstFrame ||
			animation.movingFirstFrame >= count || animation.movingFrameCount > count - animation.movingFirstFrame)
		{
			throw std::invalid_argument("Entity animation clips exceed the sprite sheet");
		}
	}
	for (const ParticleHandle handle : entity.particles)
	{
		if (!_particleSystem.isValid(handle))
		{
			throw std::invalid_argument("Entity references an invalid particle handle");
		}
	}
}

void Entities::ValidateIndex(std::size_t index) const
{
	if (index >= _xEntities.size() || (index == PlayerIndex && !_nameEntities.contains(PlayerName)))
	{
		throw std::out_of_range("Entity slot is empty");
	}
}
