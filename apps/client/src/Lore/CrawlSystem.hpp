#pragma once

#include "Lore/Crawl.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/Types.hpp"

namespace rtype::client
{

/// Where a point of a Crawl is drawn, and how.
struct CrawlProjection {
    render::Vector2 position;
    float scale = 1.0f;
    float opacity = 1.0f;
    /// Out of the screen: below its bottom, or faded out near the horizon.
    bool hidden = false;
};

/// Where the point of the crawl `x` game units right of its axis and `distance` game units down its text is
/// drawn now (see Crawl). It fades out between half its size and a fifth of it, and with the whole crawl at
/// its end.
[[nodiscard]] CrawlProjection projectOnCrawl(const Crawl &crawl, float x, float distance);

/// Scrolls the Crawls of the scenes that update, and places their CrawlItems on the screen. Once the end of
/// the text starts to fade out, the whole crawl fades out in a few seconds, then calls its onEnd.
///
/// Register it before the TransformSystem: the items have a Position of their own, without a Parent.
class CrawlSystem : public ecs::System
{
    const engine::Scenes &_scenes;
    ecs::ComponentCache<Crawl> &_crawls;
    ecs::ComponentCache<CrawlItem> &_items;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<engine::Scale> &_scales;
    ecs::ComponentCache<engine::Opacity> &_opacities;
    ecs::ComponentCache<ecs::Hidden> &_hidden;

    void advance(Crawl &crawl, float elapsed);
    void place(ecs::entity_t entity, const CrawlItem &item);
    void setHidden(ecs::entity_t entity, bool hidden);

public:
    /// @param scenes It must outlive the system.
    CrawlSystem(ecs::ComponentCaches &caches, const engine::Scenes &scenes);

    void update(float elapsed) override;
};

} // namespace rtype::client
