#include "Lore/CrawlSystem.hpp"

#include <algorithm>

namespace rtype::client
{

namespace
{

/// Above this scale, a point of the crawl is below the bottom of the screen.
constexpr float maxVisibleScale = 1.5f;

/// A text fades out while it shrinks from fadeStartScale to fadeEndScale.
constexpr float fadeStartScale = 0.5f;
constexpr float fadeEndScale = 0.2f;

/// Once the end of the text has started to fade out, the whole crawl fades out in this time, in seconds.
constexpr float endFade = 3.0f;

[[nodiscard]] bool isEndFading(const Crawl &crawl)
{
    return crawl.scrolled - crawl.length >= crawl.depth * ((1.0f / fadeStartScale) - 1.0f);
}

} // namespace

CrawlProjection projectOnCrawl(const Crawl &crawl, float x, float distance)
{
    const float beyondBottom = crawl.scrolled - distance;
    const float sight = crawl.depth + beyondBottom;
    if (sight * maxVisibleScale <= crawl.depth) {
        return {.position = {crawl.centreX, crawl.bottom}, .scale = 0.0f, .opacity = 0.0f, .hidden = true};
    }
    const float scale = crawl.depth / sight;
    const float fading = std::clamp((scale - fadeEndScale) / (fadeStartScale - fadeEndScale), 0.0f, 1.0f);
    const float ending = 1.0f - std::clamp(crawl.ending / endFade, 0.0f, 1.0f);
    const float opacity = fading * ending;
    return {.position = {crawl.centreX + (x * scale), crawl.bottom - crawl.depth + (crawl.depth * scale)},
            .scale = scale,
            .opacity = opacity,
            .hidden = opacity <= 0.0f};
}

CrawlSystem::CrawlSystem(ecs::ComponentCaches &caches, const engine::Scenes &scenes)
    : _scenes(scenes), _crawls(use<Crawl>(caches)), _items(use<CrawlItem>(caches)),
      _positions(use<ecs::Position>(caches)), _scales(use<engine::Scale>(caches)),
      _opacities(use<engine::Opacity>(caches)), _hidden(use<ecs::Hidden>(caches))
{
}

void CrawlSystem::advance(Crawl &crawl, float elapsed)
{
    crawl.scrolled += crawl.speed * elapsed;
    if (isEndFading(crawl)) {
        crawl.ending += elapsed;
    }
    if (crawl.ending >= endFade && !crawl.ended) {
        crawl.ended = true;
        if (crawl.onEnd) {
            crawl.onEnd();
        }
    }
}

void CrawlSystem::setHidden(ecs::entity_t entity, bool hidden)
{
    if (hidden && !_hidden.has(entity)) {
        _hidden.emplace(entity);
    } else if (!hidden && _hidden.has(entity)) {
        _hidden.erase(entity);
    }
}

void CrawlSystem::place(ecs::entity_t entity, const CrawlItem &item)
{
    const CrawlProjection projection = projectOnCrawl(_crawls.get(item.crawl), item.x, item.distance);
    setHidden(entity, projection.hidden);
    if (_positions.has(entity)) {
        _positions.get(entity) = {projection.position.x, projection.position.y};
    }
    if (_scales.has(entity)) {
        _scales.get(entity).value = projection.scale;
    }
    if (_opacities.has(entity)) {
        _opacities.get(entity).value = projection.opacity;
    }
}

void CrawlSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _crawls.size(); ++entity) {
        if (_crawls.has(entity) && _scenes.updates(entity)) {
            advance(_crawls.get(entity), elapsed);
        }
    }
    for (ecs::entity_t entity = 0; entity < _items.size(); ++entity) {
        if (_items.has(entity) && _crawls.has(_items.get(entity).crawl)) {
            place(entity, _items.get(entity));
        }
    }
}

} // namespace rtype::client
