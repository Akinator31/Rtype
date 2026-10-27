#include "ShipSelector/ShipSelectorSystem.hpp"

#include <algorithm>
#include <cmath>

namespace rtype::client
{

namespace
{

/// Degrees the ship turns for each pixel the pointer moves.
constexpr float turnPerPixel = 0.4f;

/// How fast the carousel glides: the part of the way left covered per second, so it slows down as it arrives.
constexpr float glideSpeed = 9.0f;

/// Distance between two ships, in world units, and how the ships that are not in the middle look: their size
/// and their brightness, as a part of those of the ship in the middle.
constexpr float shipSpacing = 3.6f;
constexpr float sideScale = 0.62f;
constexpr float sideBrightness = 0.28f;

/// A locked ship is dark: it can be looked at, not flown.
constexpr render::Color lockedTint{110, 112, 125, 255};

/// The dot of the ship looked at is a long bright line, the others short and dim.
constexpr float dotWidth = 22.0f;
constexpr float litDotWidth = 54.0f;
constexpr render::Color dotColor{255, 255, 255, 70};
constexpr render::Color litDotColor{255, 255, 255, 230};

[[nodiscard]] float mix(float from, float to, float amount)
{
    return from + ((to - from) * amount);
}

[[nodiscard]] render::Color shade(render::Color color, float brightness)
{
    const auto scale = [brightness](unsigned char channel) {
        return static_cast<unsigned char>(std::clamp(static_cast<float>(channel) * brightness, 0.0f, 255.0f));
    };
    return {scale(color.r), scale(color.g), scale(color.b), color.a};
}

[[nodiscard]] render::Color mix(render::Color from, render::Color to, float amount)
{
    const auto channel = [amount](unsigned char first, unsigned char second) {
        return static_cast<unsigned char>(mix(static_cast<float>(first), static_cast<float>(second), amount));
    };
    return {channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b), channel(from.a, to.a)};
}

} // namespace

ShipSelectorSystem::ShipSelectorSystem(ecs::ComponentCaches &caches, const engine::Scenes &scenes,
                                       const engine::Controls &controls, const input::InputSource &input)
    : _scenes(scenes), _controls(controls), _input(input), _carousels(use<ShipCarousel>(caches)),
      _models(use<engine::Model>(caches)), _boxes(use<engine::Box>(caches)),
      _opacities(use<engine::Opacity>(caches))
{
}

void ShipSelectorSystem::readControls(ecs::entity_t entity, const ShipCarousel &carousel)
{
    if (!_scenes.receivesInput(entity)) {
        return;
    }
    if (_controls.wasPressed(engine::Control::Left)) {
        carousel.selection->step(-1);
    }
    if (_controls.wasPressed(engine::Control::Right)) {
        carousel.selection->step(1);
    }
}

void ShipSelectorSystem::turnShip(ecs::entity_t entity, ShipCarousel &carousel)
{
    const float pointerX = _input.mousePosition().x;
    const bool held = _scenes.receivesInput(entity) && _input.isMouseButtonDown(input::MouseButton::Left);
    if (held && carousel.dragging) {
        carousel.yaws[carousel.selection->focused] += (pointerX - carousel.lastPointerX) * turnPerPixel;
    }
    carousel.dragging = held;
    carousel.lastPointerX = pointerX;
}

void ShipSelectorSystem::glide(ShipSelection &selection, float elapsed)
{
    const auto count = static_cast<float>(shipCount);
    // The shortest way to the ship looked at, which may cross the end of the catalog
    float way = static_cast<float>(selection.focused) - selection.slide;
    way -= count * std::round(way / count);
    selection.slide += way * std::min(1.0f, glideSpeed * elapsed);
    selection.slide -= count * std::floor(selection.slide / count);
}

void ShipSelectorSystem::placeShips(const ShipCarousel &carousel)
{
    const auto &catalog = getShipCatalog();
    const float slide = carousel.selection->slide;
    for (std::size_t index = 0; index < carousel.ships.size(); ++index) {
        engine::Model &model = _models.get(carousel.ships[index]);
        const float weight = getShipWeight(index, slide);
        model.position.x = getShipOffset(index, slide) * shipSpacing;
        model.scale = catalog[index].modelScale * mix(sideScale, 1.0f, weight);
        model.spinAngle = carousel.yaws[index];
        const render::Color base = catalog[index].locked ? lockedTint : render::colors::White;
        model.tint = shade(base, mix(sideBrightness, 1.0f, weight));
    }
}

void ShipSelectorSystem::lightDots(const ShipCarousel &carousel)
{
    for (std::size_t index = 0; index < carousel.dots.size(); ++index) {
        const ecs::entity_t dot = carousel.dots[index];
        if (!_boxes.has(dot)) {
            continue;
        }
        const float weight = getShipWeight(index, carousel.selection->slide);
        engine::Box &box = _boxes.get(dot);
        box.width = mix(dotWidth, litDotWidth, weight);
        box.color = mix(dotColor, litDotColor, weight);
    }
}

void ShipSelectorSystem::fadeTexts(const ShipCarousel &carousel)
{
    for (const ecs::entity_t text : carousel.fading) {
        if (_opacities.has(text)) {
            _opacities.get(text).value = carousel.selection->getShownClarity();
        }
    }
}

void ShipSelectorSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _carousels.size(); ++entity) {
        if (!_carousels.has(entity)) {
            continue;
        }
        ShipCarousel &carousel = _carousels.get(entity);
        readControls(entity, carousel);
        turnShip(entity, carousel);
        glide(*carousel.selection, elapsed);
        placeShips(carousel);
        lightDots(carousel);
        fadeTexts(carousel);
    }
}

} // namespace rtype::client
