#pragma once

#include "rtype/ecs/Entity/Entity.hpp"
#include "rtype/render/Types.hpp"

#include <vector>

namespace rtype::ecs
{
class Ecs;
}

namespace rtype::render
{
class Renderer;
}

namespace rtype::engine
{

class Scenes;

struct DebugCollider {
    ecs::entity_t entity;
    render::Vector2 center;
    render::Vector2 halfSize;
    float radius = 0.0f;
    bool circle = false;
    bool trigger = false;
};

struct DebugGridLine {
    render::Vector2 start;
    render::Vector2 end;
};

class ColliderDebugRenderer
{
    ecs::Ecs &_ecs;
    Scenes &_scenes;
    float _cellSize;

public:
    ColliderDebugRenderer(ecs::Ecs &ecs, Scenes &scenes, float cellSize);

    [[nodiscard]] std::vector<DebugCollider> colliders() const;
    [[nodiscard]] std::vector<DebugGridLine> gridLines(render::Vector2 gameSize) const;
    void draw(render::Renderer &renderer) const;
};

}
