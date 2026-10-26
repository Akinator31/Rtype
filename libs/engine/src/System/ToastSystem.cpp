#include "rtype/engine/System/ToastSystem.hpp"

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Component/Layout.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace rtype::engine
{

namespace
{

// Drawing order inside the interface layer: over the menus and the console
constexpr int toastOrder = 1000;

// Shortest fade, so that a zero duration does not divide by zero
constexpr float minimumFade = 0.0001f;

// Number of kinds of toasts
constexpr std::size_t kindCount = toastKindCount;

} // namespace

ToastSystem::ToastSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, const Metrics &metrics,
                         const InterfaceStyle &style)
    : _ecs(ecs), _metrics(metrics), _style(style), _toasts(use<Toast>(caches)),
      _positions(use<ecs::Position>(caches)), _opacities(use<Opacity>(caches)),
      _drawables(use<Drawable>(caches))
{
}

std::vector<ecs::entity_t> ToastSystem::getToastsNewestFirst() const
{
    std::vector<ecs::entity_t> toasts;
    for (ecs::entity_t entity = 0; entity < _toasts.size(); ++entity) {
        if (_toasts.has(entity)) {
            toasts.push_back(entity);
        }
    }
    // Toasts shown in the same frame have the same time: the last created, with the highest id, is the newest
    std::ranges::sort(toasts, [this](ecs::entity_t first, ecs::entity_t second) {
        const float firstTime = _toasts.get(first).time;
        const float secondTime = _toasts.get(second).time;
        return firstTime != secondTime ? firstTime < secondTime : first > second;
    });
    return toasts;
}

float ToastSystem::computeLifetime(const Toast &toast) const
{
    return _style.toast.getLifetime(toast.kind);
}

float ToastSystem::computeOpacity(const Toast &toast) const
{
    const float fadeIn = toast.time / std::max(_style.toast.getFadeIn(toast.kind), minimumFade);
    const float fadeOut =
        (computeLifetime(toast) - toast.time) / std::max(_style.toast.getFadeOut(toast.kind), minimumFade);
    return std::clamp(std::min(fadeIn, fadeOut), 0.0f, 1.0f);
}

float ToastSystem::computeLineWidth(const Toast &toast) const
{
    const ToastStyle &style = _style.toast;
    // The subject, the message, the actor and the icon, one gap between two of them
    float width =
        _metrics.measureText(style.nameFont, toast.getHeadline(), style.nameSize, style.letterSpacing).x;
    const std::string body = toast.getBody();
    if (!body.empty()) {
        width += style.gap + _metrics.measureText(style.messageFont, body, style.messageSize, 0.0f).x;
    }
    if (!toast.actor.empty()) {
        width += style.gap +
                 _metrics.measureText(style.nameFont, toast.actor, style.nameSize, style.letterSpacing).x;
    }
    return width + style.gap + style.iconSize;
}

float ToastSystem::computeAnnouncementWidth(const Toast &toast) const
{
    const ToastStyle &style = _style.toast;
    // As wide as its line of light, or wider if its words need it
    const float title = style.iconSize + style.gap +
                        _metrics
                            .measureText(style.nameFont, toast.getHeadline(), style.announcementTitleSize,
                                         style.announcementTitleSpacing)
                            .x;
    const float detail =
        _metrics.measureText(style.messageFont, toast.getBody(), style.announcementDetailSize, 0.0f).x;
    return std::max(style.announcementLineWidth, std::max(title, detail));
}

float ToastSystem::computeBannerWidth(const Toast & /*toast*/) const
{
    // Across the whole screen
    return _metrics.visibleArea().width;
}

float ToastSystem::computeWidth(const Toast &toast) const
{
    // One way of measuring each kind of toast, in the order of ToastKind
    using Measure = float (ToastSystem::*)(const Toast &) const;
    constexpr std::array<Measure, toastKindCount> measures{&ToastSystem::computeLineWidth,
                                                           &ToastSystem::computeAnnouncementWidth,
                                                           &ToastSystem::computeBannerWidth};
    return (this->*measures[static_cast<std::size_t>(toast.kind)])(toast);
}

float ToastSystem::computeStacked(const Toast &toast) const
{
    return toast.place * (_style.toast.getHeight(toast.kind) + _style.toast.spacing);
}

render::Vector2 ToastSystem::computeLinePosition(const Toast &toast, float opacity) const
{
    const ToastStyle &style = _style.toast;
    // While it fades, the toast is pushed back towards the edge it stacks from
    const float slide = style.slideDistance * (1.0f - opacity);
    const render::Vector2 fraction = getAnchorFraction(style.corner);
    // Toasts stack downwards, except from a bottom corner
    const float direction = fraction.y > 0.5f ? -1.0f : 1.0f;
    const Anchor anchor{style.corner, style.margin * (1.0f - (2.0f * fraction.x)),
                        (style.margin * (1.0f - (2.0f * fraction.y))) +
                            (direction * (computeStacked(toast) - slide))};
    return computeAnchoredPosition(_metrics.visibleArea(), anchor, {toast.width, style.height()});
}

// Announcements and banners draw their own entrance: they do not slide
render::Vector2 ToastSystem::computeAnnouncementPosition(const Toast &toast, float /*opacity*/) const
{
    const ToastStyle &style = _style.toast;
    return computeAnchoredPosition(_metrics.visibleArea(),
                                   {AnchorPoint::Top, 0.0f, style.announcementTop + computeStacked(toast)},
                                   {toast.width, style.getHeight(ToastKind::Announcement)});
}

render::Vector2 ToastSystem::computeBannerPosition(const Toast &toast, float /*opacity*/) const
{
    const ToastStyle &style = _style.toast;
    const render::Rectangle screen = _metrics.visibleArea();
    const float height = style.getHeight(ToastKind::Banner);
    return computeAnchoredPosition(
        screen,
        {AnchorPoint::Top, 0.0f,
         (style.bannerCentre * screen.height) - (height / 2.0f) + computeStacked(toast)},
        {toast.width, height});
}

render::Vector2 ToastSystem::computePosition(const Toast &toast, float opacity) const
{
    // One way of placing each kind of toast, in the order of ToastKind
    using Place = render::Vector2 (ToastSystem::*)(const Toast &, float) const;
    constexpr std::array<Place, toastKindCount> places{&ToastSystem::computeLinePosition,
                                                       &ToastSystem::computeAnnouncementPosition,
                                                       &ToastSystem::computeBannerPosition};
    return (this->*places[static_cast<std::size_t>(toast.kind)])(toast, opacity);
}

void ToastSystem::advance(Toast &toast, float target, bool isTooMany, float elapsed) const
{
    const bool isNew = toast.time == 0.0f;
    if (isTooMany) {
        toast.time = std::max(toast.time, computeLifetime(toast) - _style.toast.getFadeOut(toast.kind));
    }
    toast.time += elapsed;
    if (isNew) {
        toast.place = target;
    } else {
        toast.place += (target - toast.place) * std::min(1.0f, _style.toast.glideSpeed * elapsed);
    }
    toast.width = computeWidth(toast);
}

void ToastSystem::update(float elapsed)
{
    // Each kind of toast has its own stack: its newest toast is the nearest to its edge of the screen
    std::array<std::size_t, kindCount> shown{};
    for (const ecs::entity_t entity : getToastsNewestFirst()) {
        Toast &toast = _toasts.get(entity);
        const std::size_t index = shown[static_cast<std::size_t>(toast.kind)]++;
        advance(toast, static_cast<float>(index), index >= _style.toast.getMaxVisible(toast.kind), elapsed);
        if (toast.time >= computeLifetime(toast)) {
            _ecs.defer([&ecs = _ecs, entity] { ecs.destroyEntity(entity); });
            continue;
        }
        const float opacity = computeOpacity(toast);
        const render::Vector2 position = computePosition(toast, opacity);
        _positions.emplace(entity, position.x, position.y);
        _opacities.emplace(entity, opacity);
        _drawables.emplace(entity, Layer::Interface, toastOrder);
    }
}

} // namespace rtype::engine
