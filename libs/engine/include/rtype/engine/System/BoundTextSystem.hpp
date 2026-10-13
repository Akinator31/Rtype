#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"

namespace rtype::engine
{

/// Writes the Text of the entities with a BoundText from their function, every frame.
class BoundTextSystem : public ecs::System
{
    ecs::ComponentCache<Text> &_texts;
    ecs::ComponentCache<BoundText> &_bindings;

public:
    explicit BoundTextSystem(ecs::ComponentCaches &caches);

    void update(float elapsed) override;
};

} // namespace rtype::engine
