#include "rtype/engine/Rendering/ColliderDebugRenderer.hpp"

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/physics/Component/Collider.hpp"
#include "rtype/render/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace rtype::engine
{

ColliderDebugRenderer::ColliderDebugRenderer(ecs::Ecs &ecs, Scenes &scenes, float cellSize)
    : _ecs(ecs), _scenes(scenes), _cellSize(cellSize)
{
    if (!std::isfinite(cellSize) || cellSize <= 0.0f) {
        throw std::invalid_argument("ColliderDebugRenderer cell size must be positive and finite");
    }
}

std::vector<DebugCollider> ColliderDebugRenderer::colliders() const
{
    auto &colliderCache = _ecs.cache<physics::Collider>();
    auto &positions = _ecs.cache<ecs::Position>();
    std::vector<DebugCollider> result;
    for (ecs::entity_t entity = 0; entity < colliderCache.size(); ++entity) {
        if (!colliderCache.has(entity) || !positions.has(entity) || !_ecs.isAlive(entity) ||
            !_scenes.isVisible(entity) || ecs::isHidden(_ecs, entity)) {
            continue;
        }
        const physics::Collider &collider = colliderCache.get(entity);
        const ecs::Position &position = positions.get(entity);
        std::visit([&](const auto &shape) {
            using Shape = std::decay_t<decltype(shape)>;
            DebugCollider item;
            item.entity = entity;
            item.center = {position.x + collider.offset.x + shape.center.x,
                           position.y + collider.offset.y + shape.center.y};
            item.trigger = collider.trigger;
            if constexpr (std::is_same_v<Shape, physics::Aabb>) {
                item.halfSize = {std::max(0.0f, shape.halfSize.x),
                                 std::max(0.0f, shape.halfSize.y)};
            } else {
                item.radius = std::max(0.0f, shape.radius);
                item.circle = true;
            }
            result.push_back(item);
        }, collider.shape);
    }
    return result;
}

std::vector<DebugGridLine> ColliderDebugRenderer::gridLines(render::Vector2 gameSize) const
{
    std::vector<DebugGridLine> result;
    if (!std::isfinite(gameSize.x) || !std::isfinite(gameSize.y) || gameSize.x <= 0.0f ||
        gameSize.y <= 0.0f) {
        return result;
    }
    const auto vertical = static_cast<std::size_t>(
        std::min(2048.0, std::floor(static_cast<double>(gameSize.x) / _cellSize) + 1.0));
    const auto horizontal = static_cast<std::size_t>(
        std::min(2048.0, std::floor(static_cast<double>(gameSize.y) / _cellSize) + 1.0));
    result.reserve(vertical + horizontal);
    for (std::size_t index = 0; index < vertical; ++index) {
        const float x = static_cast<float>(index) * _cellSize;
        result.push_back({{x, 0.0f}, {x, gameSize.y}});
    }
    for (std::size_t index = 0; index < horizontal; ++index) {
        const float y = static_cast<float>(index) * _cellSize;
        result.push_back({{0.0f, y}, {gameSize.x, y}});
    }
    return result;
}

void ColliderDebugRenderer::draw(render::Renderer &renderer) const
{
    for (const DebugGridLine &line : gridLines(renderer.gameSize())) {
        renderer.drawLine(line.start, line.end, 1.0f, {100, 150, 255, 50});
    }
    for (const DebugCollider &collider : colliders()) {
        const render::Color color = collider.trigger ? render::Color{255, 210, 40, 220}
                                                       : render::Color{40, 240, 90, 220};
        if (collider.circle) {
            renderer.drawCircleOutline(collider.center, collider.radius, 2.0f, color);
        } else {
            renderer.drawRectangleOutline(
                {collider.center.x - collider.halfSize.x,
                 collider.center.y - collider.halfSize.y,
                 collider.halfSize.x * 2.0f,
                 collider.halfSize.y * 2.0f},
                2.0f, color);
        }
    }
}

}
