#include "rtype/engine/System/ToastSystem.hpp"

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Component/Layout.hpp"

#include <algorithm>
#include <cstddef>

namespace rtype::engine
{

namespace
{

// Drawing order inside the interface layer: over the menus and the console
constexpr int toastOrder = 1000;

// Shortest fade, so that a zero duration does not divide by zero
constexpr float minimumFade = 0.0001f;

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

float ToastSystem::computeLifetime() const
{
    return _style.toast.duration + (2.0f * _style.toast.fadeDuration);
}

float ToastSystem::computeOpacity(const Toast &toast) const
{
    const float fade = std::max(_style.toast.fadeDuration, minimumFade);
    const float fadeIn = toast.time / fade;
    const float fadeOut = (computeLifetime() - toast.time) / fade;
    return std::clamp(std::min(fadeIn, fadeOut), 0.0f, 1.0f);
}

float ToastSystem::computeWidth(const Toast &toast) const
{
    const ToastStyle &style = _style.toast;
    const float text = _metrics.measureText(_style.button.font, toast.text, style.textSize).x;
    // Padding, accent, padding, text, padding
    return std::max(style.minWidth, (3.0f * style.padding) + style.accentWidth + text);
}

render::Vector2 ToastSystem::computePosition(const Toast &toast, float opacity) const
{
    const ToastStyle &style = _style.toast;
    const render::Vector2 fraction = getAnchorFraction(style.corner);
    // Toasts stack downwards, except from a bottom corner
    const float direction = fraction.y > 0.5f ? -1.0f : 1.0f;
    const float stacked = toast.place * (style.height() + style.spacing);
    // While it fades, the toast is pushed back towards the edge it stacks from
    const float slide = style.slideDistance * (1.0f - opacity);
    const Anchor anchor{style.corner, style.margin * (1.0f - (2.0f * fraction.x)),
                        (style.margin * (1.0f - (2.0f * fraction.y))) + (direction * (stacked - slide))};
    return computeAnchoredPosition(_metrics.visibleArea(), anchor, {toast.width, style.height()});
}

void ToastSystem::advance(Toast &toast, float target, bool isTooMany, float elapsed) const
{
    const bool isNew = toast.time == 0.0f;
    if (isTooMany) {
        toast.time = std::max(toast.time, computeLifetime() - _style.toast.fadeDuration);
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
    const std::vector<ecs::entity_t> toasts = getToastsNewestFirst();
    for (std::size_t index = 0; index < toasts.size(); ++index) {
        const ecs::entity_t entity = toasts[index];
        Toast &toast = _toasts.get(entity);
        advance(toast, static_cast<float>(index), index >= _style.toast.maxVisible, elapsed);
        if (toast.time >= computeLifetime()) {
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
