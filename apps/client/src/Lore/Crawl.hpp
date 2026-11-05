#pragma once

#include "rtype/ecs/Entity/Entity.hpp"

#include <functional>

namespace rtype::client
{

/// A text that scrolls away on a plane tilted into the depth of the screen, like the opening of an old
/// space film: it comes from the bottom of the screen, shrinks and slows down as it goes towards the horizon,
/// and fades out before it reaches it. The CrawlSystem moves its CrawlItems.
///
/// A point of the plane at `distance` game units beyond the bottom edge is drawn at the scale
/// depth / (depth + distance), between the bottom of the screen (scale 1) and the horizon, `depth` above it
/// (scale 0): a unit of the plane is a unit of the screen at the bottom.
struct Crawl {
    /// Axis of the crawl, and the height of its bottom edge, in game units.
    float centreX = 0.0f;
    float bottom = 0.0f;
    /// Height of the horizon above the bottom edge, in game units.
    float depth = 1000.0f;
    /// How fast the text goes away, in game units of the plane per second.
    float speed = 80.0f;
    /// Distance travelled by the text from the bottom edge. Below 0 at the start, so that the text comes in
    /// from below the screen.
    float scrolled = 0.0f;
    /// Height of the whole text, in game units: the crawl ends once its end has gone far enough.
    float length = 0.0f;
    /// Time since the crawl began to fade out, in seconds, once its end has started to fade.
    float ending = 0.0f;
    /// Called once the crawl has faded out.
    std::function<void()> onEnd{};
    bool ended = false;
};

/// A text of a Crawl, at its place on the plane: `x` game units right of its axis, and `distance` game units
/// from the start of the text downwards, at its natural size. The CrawlSystem gives it its Position, Scale
/// and Opacity, and hides it while it is out of the screen.
struct CrawlItem {
    ecs::entity_t crawl = 0;
    float x = 0.0f;
    float distance = 0.0f;
};

} // namespace rtype::client
