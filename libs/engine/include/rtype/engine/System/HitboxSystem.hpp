#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"

namespace rtype::engine
{

/// Computes the Hitbox of every widget with a text that can get the focus or is in a Column: the size of its
/// Panel if it has one, of its rail if it is a slider, of its row if it is a switch, else of its text plus the padding of the style, times its
/// Scale.
class HitboxSystem : public ecs::System
{
    const Metrics &_metrics;
    const InterfaceStyle &_style;
    ecs::ComponentCache<Focusable> &_focusables;
    ecs::ComponentCache<ColumnItem> &_items;
    ecs::ComponentCache<Text> &_texts;
    ecs::ComponentCache<Panel> &_panels;
    ecs::ComponentCache<SliderTrack> &_tracks;
    ecs::ComponentCache<SwitchTrack> &_switches;
    ecs::ComponentCache<Scale> &_scales;
    ecs::ComponentCache<Hitbox> &_hitboxes;

    /// Size of the widget at scale 1.
    [[nodiscard]] render::Vector2 measure(ecs::entity_t entity) const;

public:
    /// @param metrics, style They must outlive the system.
    HitboxSystem(ecs::ComponentCaches &caches, const Metrics &metrics, const InterfaceStyle &style);

    void update(float elapsed) override;
};

} // namespace rtype::engine
