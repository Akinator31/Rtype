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
    /// Time from its appearance to the end of its exit, which depends on its kind.
    [[nodiscard]] float computeLifetime(const Toast &toast) const;
    [[nodiscard]] float computeOpacity(const Toast &toast) const;
    /// The width of a toast, by kind: what its words need, or the line of light of an announcement, or the
    /// whole screen for a banner.
    [[nodiscard]] float computeLineWidth(const Toast &toast) const;
    [[nodiscard]] float computeAnnouncementWidth(const Toast &toast) const;
    [[nodiscard]] float computeBannerWidth(const Toast &toast) const;
    /// How far from its edge of the screen a toast is, from its place in the stack of its kind.
    [[nodiscard]] float computeStacked(const Toast &toast) const;
    /// The centre of a toast, by kind.
    [[nodiscard]] render::Vector2 computeLinePosition(const Toast &toast, float opacity) const;
    [[nodiscard]] render::Vector2 computeAnnouncementPosition(const Toast &toast, float opacity) const;
    [[nodiscard]] render::Vector2 computeBannerPosition(const Toast &toast, float opacity) const;
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
