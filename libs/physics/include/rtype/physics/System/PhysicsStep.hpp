#pragma once

#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/physics/System/FrameSystem.hpp"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace rtype::physics
{

class PhysicsStep : public ecs::System
{
    ecs::ComponentCaches &_caches;
    std::vector<std::unique_ptr<ecs::System>> _systems;
    std::vector<FrameSystem *> _frameSystems;
    double _accumulator = 0.0;
    float _step;
    std::size_t _maxSteps;

public:
    explicit PhysicsStep(ecs::ComponentCaches &caches, float step = 1.0f / 60.0f,
                         std::size_t maxSteps = 8);

    template <typename S, typename... Args>
    S &registerSystem(Args &&...args)
    {
        auto system = std::make_unique<S>(_caches, std::forward<Args>(args)...);
        S &ref = *system;
        if constexpr (std::is_base_of_v<FrameSystem, S>) {
            _frameSystems.push_back(&ref);
        }
        _systems.push_back(std::move(system));
        return ref;
    }

    void update(float elapsed) override;

    [[nodiscard]] float step() const;
    [[nodiscard]] std::size_t maxSteps() const;
    [[nodiscard]] float alpha() const;
};

}
