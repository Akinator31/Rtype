#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Toast.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"

#include <vector>

namespace rtype::ecs
{
class Ecs;
} // namespace rtype::ecs

namespace rtype::engine
{

/// Shows the toasts (see Engine::showToast()), with the look and timing of InterfaceStyle::toast: stacks
/// them from the corner of the style, the newest nearest to it, fades and slides them in and out, and
/// destroys them once faded out. When there are more than ToastStyle::maxVisible, the oldest ones fade out
/// early. A new toast takes its place at once; the others glide to theirs.
///
/// It writes the Position, Opacity and Drawable of each toast, and its width from its text. Register it
/// before the TransformSystem.
class ToastSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    const Metrics &_metrics;
    const InterfaceStyle &_style;
    ecs::ComponentCache<Toast> &_toasts;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<Opacity> &_opacities;
    ecs::ComponentCache<Drawable> &_drawables;

    [[nodiscard]] std::vector<ecs::entity_t> getToastsNewestFirst() const;
    /// Time from its appearance to the end of its fade out.
    [[nodiscard]] float computeLifetime() const;
    [[nodiscard]] float computeOpacity(const Toast &toast) const;
    [[nodiscard]] float computeWidth(const Toast &toast) const;
    [[nodiscard]] render::Vector2 computePosition(const Toast &toast, float opacity) const;
    void advance(Toast &toast, float target, bool isTooMany, float elapsed) const;

public:
    /// @param ecs, metrics, style They must outlive the system.
    ToastSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, const Metrics &metrics,
                const InterfaceStyle &style);

    void update(float elapsed) override;
};

} // namespace rtype::engine
