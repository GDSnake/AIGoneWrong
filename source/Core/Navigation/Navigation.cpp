#include "Navigation.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include <box2d/box2d.h>

#if defined(_MSC_VER)
#include <intrin.h>
#else
#define __debugbreak() __builtin_trap()
#endif

#include <ViennaGameAILibrary.hpp>

#if !defined(_MSC_VER)
#undef __debugbreak
#endif

namespace
{
	constexpr float PixelsPerMeter = 32.0f;

	b2ShapeProxy colliderProxy(const NavigationRequest& request, const SDL_FPoint& feet, float padding)
	{
		const float halfWidth = request.colliderSize.x * 0.5f + padding;
		const float top = feet.y - request.colliderSize.y - padding;
		const float bottom = feet.y + padding;
		b2ShapeProxy proxy{};
		proxy.count = 4;
		proxy.points[0] = { (feet.x - halfWidth) / PixelsPerMeter, top / PixelsPerMeter };
		proxy.points[1] = { (feet.x + halfWidth) / PixelsPerMeter, top / PixelsPerMeter };
		proxy.points[2] = { (feet.x + halfWidth) / PixelsPerMeter, bottom / PixelsPerMeter };
		proxy.points[3] = { (feet.x - halfWidth) / PixelsPerMeter, bottom / PixelsPerMeter };
		return proxy;
	}

	bool canOccupy(const NavigationRequest& request, const SDL_FPoint& feet)
	{
		if (feet.x - request.colliderSize.x * 0.5f < request.bounds.x ||
			feet.x + request.colliderSize.x * 0.5f > request.bounds.x + request.bounds.w ||
			feet.y - request.colliderSize.y < request.bounds.y || feet.y > request.bounds.y + request.bounds.h)
		{
			return false;
		}
		const auto proxy = colliderProxy(request, feet, 1.0f);
		b2QueryFilter filter = b2DefaultQueryFilter();
		filter.maskBits = 1u;
		bool blocked = false;
		b2World_OverlapShape(request.world, &proxy, filter, [](b2ShapeId, void* context)
		{
			*static_cast<bool*>(context) = true;
			return false;
		}, &blocked);
		return !blocked;
	}

	bool canTraverse(const NavigationRequest& request, const SDL_FPoint& from, const SDL_FPoint& to)
	{
		const auto proxy = colliderProxy(request, from, 1.0f);
		b2QueryFilter filter = b2DefaultQueryFilter();
		filter.maskBits = 1u;
		bool blocked = false;
		b2World_CastShape(request.world, &proxy,
			{ (to.x - from.x) / PixelsPerMeter, (to.y - from.y) / PixelsPerMeter }, filter,
			[](b2ShapeId, b2Vec2, b2Vec2, float, void* context)
			{
				*static_cast<bool*>(context) = true;
				return 0.0f;
			}, &blocked);
		return !blocked;
	}
}

std::vector<SDL_FPoint> Navigation::findPath(const NavigationRequest& request)
{
	const std::array values{ request.bounds.x, request.bounds.y, request.bounds.w, request.bounds.h,
		request.cellSize.x, request.cellSize.y, request.colliderSize.x, request.colliderSize.y,
		request.start.x, request.start.y, request.target.x, request.target.y };
	if (!b2World_IsValid(request.world) ||
		!std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); }) ||
		request.bounds.w <= 0.0f || request.bounds.h <= 0.0f || request.cellSize.x <= 0.0f || request.cellSize.y <= 0.0f ||
		request.colliderSize.x <= 0.0f || request.colliderSize.y <= 0.0f || !canOccupy(request, request.target))
	{
		return {};
	}
	if (canOccupy(request, request.start) && canTraverse(request, request.start, request.target))
	{
		return { request.target };
	}
	const double columns = std::ceil(request.bounds.w / request.cellSize.x);
	const double rows = std::ceil(request.bounds.h / request.cellSize.y);
	if (columns * rows > 65536.0)
	{
		return {};
	}
	VGAIL::NavMesh graph(static_cast<VGAIL::ui32>(columns), static_cast<VGAIL::ui32>(rows), 0.0f);
	const auto worldPoint = [&request](VGAIL::Vec2ui node)
	{
		return SDL_FPoint{ request.bounds.x + (node.x + 0.5f) * request.cellSize.x,
			request.bounds.y + (node.y + 0.5f) * request.cellSize.y + request.colliderSize.y * 0.5f };
	};
	for (VGAIL::ui32 row = 0; row < graph.getHeight(); ++row)
	{
		for (VGAIL::ui32 column = 0; column < graph.getWidth(); ++column)
		{
			const VGAIL::Vec2ui node{ column, row };
			if (!canOccupy(request, worldPoint(node)))
			{
				graph.setObstructable(node);
			}
		}
	}
	graph.canTraverse = [&request, &worldPoint](VGAIL::Vec2ui from, VGAIL::Vec2ui to)
	{
		return canTraverse(request, worldPoint(from), worldPoint(to));
	};
	const auto closestNode = [&graph, &worldPoint, &request](const SDL_FPoint& point)
	{
		std::optional<VGAIL::Vec2ui> closest;
		float bestDistance = std::numeric_limits<float>::infinity();
		for (VGAIL::ui32 row = 0; row < graph.getHeight(); ++row)
		{
			for (VGAIL::ui32 column = 0; column < graph.getWidth(); ++column)
			{
				const VGAIL::Vec2ui node{ column, row };
				if (graph.getNode(node).state != VGAIL::WALKABLE)
				{
					continue;
				}
				const SDL_FPoint position = worldPoint(node);
				const float deltaX = position.x - point.x;
				const float deltaY = position.y - point.y;
				const float distance = deltaX * deltaX + deltaY * deltaY;
				if (distance < bestDistance && canTraverse(request, point, position))
				{
					bestDistance = distance;
					closest = node;
				}
			}
		}
		return closest;
	};
	const auto start = closestNode(request.start);
	const auto target = closestNode(request.target);
	if (!start || !target)
	{
		return {};
	}
	const auto nodes = graph.findPath(*start, *target);
	if (nodes.empty())
	{
		return {};
	}
	std::vector<SDL_FPoint> result;
	result.reserve(nodes.size() + 1);
	const std::size_t firstNode = nodes.size() > 1 && canTraverse(request, request.start, worldPoint(nodes[1])) ? 1 : 0;
	for (std::size_t index = firstNode; index < nodes.size(); ++index)
	{
		result.push_back(worldPoint(nodes[index]));
	}
	result.push_back(request.target);
	return result;
}