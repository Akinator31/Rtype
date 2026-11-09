#include "rtype/engine/Rendering/SceneRenderer.hpp"

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Interface/SwitchLayout.hpp"
#include "rtype/render/Renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

namespace rtype::engine
{

namespace
{

render::Color faded(render::Color color, float opacity)
{
    color.a = static_cast<std::uint8_t>(static_cast<float>(color.a) * std::clamp(opacity, 0.0f, 1.0f));
    return color;
}

render::Color mix(render::Color from, render::Color to, float amount)
{
    const auto channel = [amount](std::uint8_t start, std::uint8_t end) {
        return static_cast<std::uint8_t>(std::lround(
            static_cast<float>(start) + (static_cast<float>(end) - static_cast<float>(start)) * amount));
    };
    return {channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b), channel(from.a, to.a)};
}

/// The colour more or less opaque, as much as a light more or less strong: brighter beyond 1.
render::Color brightened(render::Color color, float strength)
{
    color.a = static_cast<std::uint8_t>(std::clamp(static_cast<float>(color.a) * strength, 0.0f, 255.0f));
    return color;
}

/// A four-pointed star turned by `angle` degrees: a thin triangle from the centre to each point.
void drawStar(render::Renderer &renderer, render::Vector2 centre, float size, float thickness, float angle,
              render::Color color)
{
    const float radians = angle * std::numbers::pi_v<float> / 180.0f;
    const render::Vector2 along{std::cos(radians), std::sin(radians)};
    const render::Vector2 across{-along.y, along.x};
    const float half = thickness / 2.0f;
    for (const render::Vector2 arm : {along, across}) {
        const render::Vector2 first{centre.x + (arm.y * half), centre.y - (arm.x * half)};
        const render::Vector2 second{centre.x - (arm.y * half), centre.y + (arm.x * half)};
        renderer.drawTriangle(first, second, {centre.x + (arm.x * size), centre.y + (arm.y * size)}, color);
        renderer.drawTriangle(first, second, {centre.x - (arm.x * size), centre.y - (arm.y * size)}, color);
    }
}

/// A horizontal line from `left` to `right`, its colour going from `from` to `to`, cut at `end`.
void drawFadingLine(render::Renderer &renderer, float left, float right, float y, float thickness,
                    render::Color from, render::Color to, float end)
{
    const float cut = std::min(right, end);
    if (cut <= left) {
        return;
    }
    const render::Color last = mix(from, to, (cut - left) / (right - left));
    const render::Rectangle area{left, y - (thickness / 2.0f), cut - left, thickness};
    renderer.drawRoundedRectangle(area, thickness / 2.0f, from, last);
}

render::Color clear(render::Color color)
{
    color.a = 0;
    return color;
}

render::Rectangle grow(render::Rectangle area, float margin)
{
    return {area.x - margin, area.y - margin, area.width + 2.0f * margin, area.height + 2.0f * margin};
}

// A blurred rounded rectangle, for shadows and glows: faint layers from blur / 2 inside the area to blur / 2
// outside of it. They add up to the colour inside, and fade out across the blur.
void drawBlurred(render::Renderer &renderer, render::Rectangle area, float radius, float blur,
                 render::Color color)
{
    constexpr int layers = 8;
    if (color.a == 0) {
        return;
    }
    // Alpha of one layer such that `layers` of them stacked give the alpha of the colour
    const float coverage = static_cast<float>(color.a) / 255.0f;
    const float layerCoverage = 1.0f - std::pow(1.0f - coverage, 1.0f / static_cast<float>(layers));
    render::Color layerColor = color;
    layerColor.a = static_cast<std::uint8_t>(std::lround(layerCoverage * 255.0f));

    for (int layer = 0; layer < layers; ++layer) {
        const float margin = blur * ((static_cast<float>(layer) + 0.5f) / static_cast<float>(layers) - 0.5f);
        const render::Rectangle layerArea = grow(area, margin);
        if (layerArea.width > 0.0f && layerArea.height > 0.0f) {
            renderer.drawRoundedRectangle(layerArea, std::max(radius + margin, 0.0f), layerColor, layerColor);
        }
    }
}

// The light around a text, which a blur would spread: the text drawn again around its place, on three rings
// of copies so faint that each one is lost against the dark, and only their sum near the letters shows
void drawTextGlow(render::Renderer &renderer, render::FontId font, const std::string &text,
                  render::Vector2 topLeft, float size, float spacing, render::Color color, float blur)
{
    constexpr int directions = 12;
    constexpr float rings[] = {0.35f, 0.7f, 1.0f};
    constexpr float layerStrength = 0.07f;
    if (color.a == 0 || blur <= 0.0f) {
        return;
    }
    render::Color layer = color;
    layer.a = static_cast<std::uint8_t>(static_cast<float>(color.a) * layerStrength);
    for (const float ring : rings) {
        // The middle ring is turned by half a step, so that the copies of two rings do not line up
        const float turn = (ring == rings[1]) ? 0.5f : 0.0f;
        for (int direction = 0; direction < directions; ++direction) {
            const float angle = (2.0f * std::numbers::pi_v<float> * (static_cast<float>(direction) + turn)) /
                                static_cast<float>(directions);
            const float distance = (blur / 2.0f) * ring;
            renderer.drawText(
                font, text,
                {topLeft.x + (std::cos(angle) * distance), topLeft.y + (std::sin(angle) * distance)}, size,
                layer, spacing);
        }
    }
}

// A soft light around a circle: filled circles, each one a little larger than the one inside it, drawn from
// the outside in, so that the light is the faintest at `spread` from the edge and strongest at the edge
void drawHalo(render::Renderer &renderer, render::Vector2 centre, float radius, float spread,
              render::Color color)
{
    constexpr int layers = 16;
    if (color.a == 0 || spread <= 0.0f) {
        return;
    }
    const float coverage = static_cast<float>(color.a) / 255.0f;
    const float layerCoverage = 1.0f - std::pow(1.0f - coverage, 1.0f / static_cast<float>(layers));
    render::Color layer = color;
    layer.a = static_cast<std::uint8_t>(std::lround(layerCoverage * 255.0f));
    for (int index = 0; index < layers; ++index) {
        const float outwards = 1.0f - ((static_cast<float>(index) + 0.5f) / static_cast<float>(layers));
        renderer.drawCircle(centre, radius + (spread * outwards), layer);
    }
}

// The icons at the end of a toast, centred on `centre`: a circled "i" for an information, a star for a gain,
// a triangle with an exclamation mark for a warning or an error
void drawInfoIcon(render::Renderer &renderer, render::Vector2 centre, float size, float thickness,
                  render::Color color)
{
    renderer.drawCircleOutline(centre, size / 2.0f, thickness, color);
    renderer.drawCircle({centre.x, centre.y - (size * 0.2f)}, thickness * 0.6f, color);
    renderer.drawLine({centre.x, centre.y - (size * 0.06f)}, {centre.x, centre.y + (size * 0.24f)}, thickness,
                      color);
}

void drawGainIcon(render::Renderer &renderer, render::Vector2 centre, float size, float thickness,
                  render::Color color)
{
    drawStar(renderer, centre, size / 2.0f, thickness * 1.5f, 0.0f, color);
}

void drawAlertIcon(render::Renderer &renderer, render::Vector2 centre, float size, float thickness,
                   render::Color color)
{
    const float half = size / 2.0f;
    const render::Vector2 apex{centre.x, centre.y - (half * 0.95f)};
    const render::Vector2 left{centre.x - half, centre.y + (half * 0.8f)};
    const render::Vector2 right{centre.x + half, centre.y + (half * 0.8f)};
    renderer.drawLine(apex, left, thickness, color);
    renderer.drawLine(left, right, thickness, color);
    renderer.drawLine(right, apex, thickness, color);
    renderer.drawCircle({centre.x, centre.y + (size * 0.3f)}, thickness * 0.6f, color);
    renderer.drawLine({centre.x, centre.y - (size * 0.28f)}, {centre.x, centre.y + (size * 0.12f)}, thickness,
                      color);
}

void drawToastIcon(render::Renderer &renderer, ToastType type, render::Vector2 centre, float size,
                   float thickness, render::Color color)
{
    // One icon per type, in the order of ToastType: a warning and an error share theirs, a gain and a quest
    // too
    using Icon = void (*)(render::Renderer &, render::Vector2, float, float, render::Color);
    constexpr std::array<Icon, toastTypeCount> icons{drawInfoIcon, drawGainIcon, drawAlertIcon, drawAlertIcon,
                                                     drawGainIcon};
    icons[static_cast<std::size_t>(type)](renderer, centre, size, thickness, color);
}

// A line of light: its colour fades in from the left end to the middle and out to the right end, with a glow
// along its middle part
void drawLightLine(render::Renderer &renderer, float left, float right, float y, float thickness,
                   render::Color color, render::Color glow, float blur)
{
    if (right <= left) {
        return;
    }
    const float middle = (left + right) / 2.0f;
    const float inset = (right - left) * 0.2f;
    drawBlurred(renderer, {left + inset, y - (thickness / 2.0f), (right - left) - (2.0f * inset), thickness},
                thickness / 2.0f, blur, glow);
    drawFadingLine(renderer, left, middle, y, thickness, clear(color), color, middle);
    drawFadingLine(renderer, middle, right, y, thickness, color, clear(color), right);
}

// A spark: a streak of light across, fading at both ends, and a small star at its heart
void drawSpark(render::Renderer &renderer, render::Vector2 centre, float halfLength, float halfHeight,
               render::Color color)
{
    if (halfLength <= 0.0f) {
        return;
    }
    drawFadingLine(renderer, centre.x - halfLength, centre.x, centre.y, 2.0f, clear(color), color, centre.x);
    drawFadingLine(renderer, centre.x, centre.x + halfLength, centre.y, 2.0f, color, clear(color),
                   centre.x + halfLength);
    drawStar(renderer, centre, halfHeight, 1.8f, 0.0f, color);
}

// How far a part of an entrance is, when the whole entrance is at `progress`: 0 before `from`, 1 after `to`,
// eased in between. Played backwards while a toast leaves, since the progress then goes back to 0.
float stage(float progress, float from, float to, Easing easing = Easing::EaseOut)
{
    return ease(easing, std::clamp((progress - from) / (to - from), 0.0f, 1.0f));
}

// A flash that rises and falls, once, while `amount` goes from 0 to 1
float flash(float amount)
{
    return std::sin(std::numbers::pi_v<float> * std::clamp(amount, 0.0f, 1.0f));
}

float smooth(float amount)
{
    const float clamped = std::clamp(amount, 0.0f, 1.0f);
    return clamped * clamped * (3.0f - (2.0f * clamped));
}

// A chevron `width` wide from `left`, centred on `middle`: pointing right for a direction of +1 (›), left for
// -1 (‹)
void drawChevron(render::Renderer &renderer, float left, float middle, float width, float height,
                 float thickness, int direction, render::Color color)
{
    const float tip = direction > 0 ? left + width : left;
    const float back = direction > 0 ? left : left + width;
    const render::Vector2 point{tip, middle};
    renderer.drawLine({back, middle - (height / 2.0f)}, point, thickness, color);
    renderer.drawLine(point, {back, middle + (height / 2.0f)}, thickness, color);
    // A dot on the tip joins the two strokes
    renderer.drawCircle(point, thickness / 2.0f, color);
}

// The outline of a rectangle in dashes `dash` long, with gaps as long
void drawDashedOutline(render::Renderer &renderer, render::Rectangle area, float thickness, float dash,
                       render::Color color)
{
    const float step = 2.0f * std::max(dash, 1.0f);
    for (float x = area.x; x < area.x + area.width; x += step) {
        const float length = std::min(dash, area.x + area.width - x);
        renderer.drawRectangle({x, area.y, length, thickness}, color);
        renderer.drawRectangle({x, area.y + area.height - thickness, length, thickness}, color);
    }
    for (float y = area.y + thickness; y < area.y + area.height - thickness; y += step) {
        const float length = std::min(dash, area.y + area.height - thickness - y);
        renderer.drawRectangle({area.x, y, thickness, length}, color);
        renderer.drawRectangle({area.x + area.width - thickness, y, thickness, length}, color);
    }
}

// The caption on the left of a widget, and its value on the right, on the line starting at `top`
void drawCaption(render::Renderer &renderer, render::FontId font, const std::string &caption,
                 const std::string &value, float left, float right, float top, float size, float spacing,
                 render::Color color, render::Color valueColor)
{
    renderer.drawText(font, caption, {left, top}, size, color, spacing);
    if (!value.empty()) {
        const float width = renderer.measureText(font, value, size, spacing).x;
        renderer.drawText(font, value, {right - width, top}, size, valueColor, spacing);
    }
}

// The pose at the time of an animation, between the previous and the current one while it fades, or mixed
// with its blended animation
render::ModelPose computeBasePose(const ModelAnimation &animation)
{
    if (animation.time >= animation.fadeDuration && animation.blendAmount > 0.0f) {
        return {.from = animation.animation,
                .fromTime = animation.time,
                .to = animation.blendedAnimation,
                .toTime = animation.time,
                .amount = animation.blendAmount};
    }
    if (animation.time >= animation.fadeDuration) {
        return {.from = animation.animation,
                .fromTime = animation.time,
                .to = animation.animation,
                .toTime = animation.time};
    }
    const float progress = animation.time / animation.fadeDuration;
    return {.from = animation.previousAnimation,
            .fromTime = animation.previousTime + animation.time,
            .to = animation.animation,
            .toTime = animation.time,
            .amount = progress * progress * (3.0f - (2.0f * progress))};
}

// Poses the model at the time of its animation, with its added animation over it
void applyPose(render::Renderer &renderer, render::ModelId model, const ModelAnimation &animation)
{
    render::ModelPose pose = computeBasePose(animation);
    pose.added = animation.addedAnimation;
    pose.addedTime = animation.addedTime;
    renderer.poseModel(model, pose);
}

/// The part of a picture that fills an area without being stretched: all of it in one direction, its centre
/// in the other one.
render::Rectangle computeCoverSource(render::Vector2 picture, render::Rectangle area)
{
    const float scale = std::max(area.width / picture.x, area.height / picture.y);
    const float width = area.width / scale;
    const float height = area.height / scale;
    return {(picture.x - width) / 2.0f, (picture.y - height) / 2.0f, width, height};
}

/// The look of a button in the red of the alerts: its text, its line and its star.
FlareStyle inDanger(FlareStyle style)
{
    const render::Color red = style.danger;
    const auto tinted = [red](render::Color color) { return render::Color{red.r, red.g, red.b, color.a}; };
    style.text = tinted(style.text);
    style.litText = red;
    style.textGlow = tinted(style.textGlow);
    style.line = tinted(style.line);
    style.lineGlow = tinted(style.lineGlow);
    return style;
}

/// The value brought between 0 and the period, which repeats it; a period of 0 keeps the value.
float wrap(float value, float period)
{
    if (period <= 0.0f) {
        return value;
    }
    return value - (period * std::floor(value / period));
}

/// A picture of the sky, a Backdrop or a BackdropSprite, to draw in order.
struct SkyPicture {
    ecs::entity_t entity;
    int order;
    bool isSprite;
};

} // namespace

SceneRenderer::SceneRenderer(ecs::Ecs &ecs, Scenes &scenes, const InterfaceStyle &style)
    : _ecs(ecs), _scenes(scenes), _style(style)
{
}

void SceneRenderer::drawConsoleField(render::Renderer &renderer, const Text &text, const TextField &field,
                                     render::Vector2 topLeft, float size, float opacity) const
{
    const std::string prompt = fieldPrefix(field);
    renderer.drawText(text.font, prompt, topLeft, size, faded(_style.console.prompt, opacity));
    const float promptWidth = renderer.measureText(text.font, prompt, size).x;
    renderer.drawText(text.font, fieldContent(field), {topLeft.x + promptWidth, topLeft.y}, size,
                      faded(text.color, opacity));
}

void SceneRenderer::draw(render::Renderer &renderer)
{
    draw(renderer, Layer::World);
    draw(renderer, Layer::Interface);
}

bool SceneRenderer::isShown(ecs::entity_t entity)
{
    return _scenes.isVisible(entity) && !ecs::isHidden(_ecs, entity);
}

bool SceneRenderer::isShown(ecs::entity_t entity, Layer layer)
{
    const bool isSceneShowing =
        layer == Layer::Interface ? _scenes.isInterfaceVisible(entity) : _scenes.isVisible(entity);
    return isSceneShowing && !ecs::isHidden(_ecs, entity);
}

const Camera *SceneRenderer::findCamera()
{
    auto &cameras = _ecs.cache<Camera>();
    for (ecs::entity_t entity = 0; entity < cameras.size(); ++entity) {
        if (cameras.has(entity) && isShown(entity)) {
            return &cameras.get(entity);
        }
    }
    return nullptr;
}

render::Light SceneRenderer::findLight()
{
    auto &lights = _ecs.cache<Light>();
    auto &models = _ecs.cache<Model>();
    for (ecs::entity_t entity = 0; entity < lights.size(); ++entity) {
        if (lights.has(entity) && !models.has(entity) && isShown(entity)) {
            return lights.get(entity).light;
        }
    }
    return {};
}

std::vector<ecs::entity_t> SceneRenderer::getModelsInOrder()
{
    auto &drawables = _ecs.cache<Drawable>();
    auto &models = _ecs.cache<Model>();

    std::vector<ecs::entity_t> entities;
    for (ecs::entity_t entity = 0; entity < models.size(); ++entity) {
        if (models.has(entity) && drawables.has(entity) && drawables.get(entity).layer == Layer::World &&
            isShown(entity)) {
            entities.push_back(entity);
        }
    }
    std::ranges::stable_sort(entities, {}, [&](ecs::entity_t entity) { return drawables.get(entity).order; });
    return entities;
}

std::optional<ecs::entity_t> SceneRenderer::findFirstBackdrop()
{
    auto &backdrops = _ecs.cache<Backdrop>();
    for (ecs::entity_t entity = 0; entity < backdrops.size(); ++entity) {
        if (backdrops.has(entity) && isShown(entity)) {
            return entity;
        }
    }
    return std::nullopt;
}

float SceneRenderer::getScrollOffset(ecs::entity_t entity)
{
    auto &scrolls = _ecs.cache<Scroll>();
    return scrolls.has(entity) ? scrolls.get(entity).offset : 0.0f;
}

void SceneRenderer::drawBackdrop(render::Renderer &renderer)
{
    const std::optional<ecs::entity_t> first = findFirstBackdrop();
    if (!first) {
        return;
    }
    const std::optional<SceneId> sky = _scenes.sceneOf(*first);
    auto &backdrops = _ecs.cache<Backdrop>();
    auto &sprites = _ecs.cache<BackdropSprite>();
    const auto isInSky = [&](ecs::entity_t entity) {
        return isShown(entity) && _scenes.sceneOf(entity) == sky;
    };

    std::vector<SkyPicture> pictures;
    for (ecs::entity_t entity = 0; entity < backdrops.size(); ++entity) {
        if (backdrops.has(entity) && isInSky(entity)) {
            pictures.push_back({entity, backdrops.get(entity).order, false});
        }
    }
    for (ecs::entity_t entity = 0; entity < sprites.size(); ++entity) {
        if (sprites.has(entity) && isInSky(entity)) {
            pictures.push_back({entity, sprites.get(entity).order, true});
        }
    }
    std::ranges::stable_sort(pictures, {}, &SkyPicture::order);

    for (const SkyPicture &picture : pictures) {
        const float offset = getScrollOffset(picture.entity);
        if (picture.isSprite) {
            drawBackdropSprite(renderer, sprites.get(picture.entity), offset);
        } else {
            drawBackdropLayer(renderer, backdrops.get(picture.entity), offset);
        }
    }
}

void SceneRenderer::drawBackdropLayer(render::Renderer &renderer, const Backdrop &backdrop, float offset)
{
    const render::Rectangle area = renderer.visibleArea();
    const render::Vector2 picture = renderer.textureSize(backdrop.texture);
    render::Rectangle source = computeCoverSource(picture, area);
    const float scale = area.width / source.width;
    source.x = wrap(source.x + (offset / scale), picture.x);

    const float firstWidth = std::min(source.width, picture.x - source.x);
    renderer.drawTexture(backdrop.texture, {source.x, source.y, firstWidth, source.height},
                         {area.x, area.y, firstWidth * scale, area.height}, 0.0f, backdrop.tint);
    if (firstWidth < source.width) {
        const float restWidth = source.width - firstWidth;
        renderer.drawTexture(backdrop.texture, {0.0f, source.y, restWidth, source.height},
                             {area.x + (firstWidth * scale), area.y, restWidth * scale, area.height}, 0.0f,
                             backdrop.tint);
    }
}

void SceneRenderer::drawBackdropSprite(render::Renderer &renderer, const BackdropSprite &sprite, float offset)
{
    const render::Rectangle area = renderer.visibleArea();
    const render::Vector2 picture = renderer.textureSize(sprite.texture);
    const float width = sprite.height * picture.x / picture.y;
    const float travel = wrap(sprite.start + offset, sprite.period);
    const render::Rectangle destination{area.x + area.width - travel,
                                        area.y + sprite.y - (sprite.height / 2.0f), width, sprite.height};
    if (destination.x >= area.x + area.width || destination.x + destination.width <= area.x) {
        return;
    }
    renderer.drawTexture(sprite.texture, {0.0f, 0.0f, picture.x, picture.y}, destination, 0.0f, sprite.tint);
}

void SceneRenderer::poseModel(render::Renderer &renderer, render::ModelId model,
                              const ModelAnimation &animation)
{
    const auto posed = std::ranges::find(_poses, model, &Pose::model);
    if (posed == _poses.end()) {
        _poses.push_back({model, animation});
    } else if (posed->animation == animation) {
        return;
    } else {
        posed->animation = animation;
    }
    applyPose(renderer, model, animation);
}

void SceneRenderer::drawModels(render::Renderer &renderer)
{
    const Camera *camera = findCamera();
    if (camera == nullptr) {
        return;
    }
    const render::Light sceneLight = findLight();
    auto &models = _ecs.cache<Model>();
    auto &lights = _ecs.cache<Light>();
    auto &animations = _ecs.cache<ModelAnimation>();

    for (const ecs::entity_t entity : getModelsInOrder()) {
        const Model &model = models.get(entity);
        const render::Light &light = lights.has(entity) ? lights.get(entity).light : sceneLight;

        if (animations.has(entity)) {
            poseModel(renderer, model.model, animations.get(entity));
        }
        if (model.energy) {
            renderer.drawEnergyModel(model.model, camera->view, *model.energy, model.position,
                                     model.rotationAxis, model.rotationAngle, model.scale, model.tint,
                                     model.spinAngle);
        } else {
            renderer.drawModel(model.model, camera->view, light, model.position, model.rotationAxis,
                               model.rotationAngle, model.scale, model.tint, model.spinAngle,
                               model.hiddenMeshes);
            // Right after the model, in its pose: the entities of a model share it, each posing it in turn
            if (model.aura) {
                renderer.drawAura(model.model, camera->view, *model.aura, model.position, model.rotationAxis,
                                  model.rotationAngle, model.scale, model.spinAngle, model.hiddenMeshes);
            }
        }
    }
    // The light of the energy models spreads over the world, under the 2D entities and the interface
    renderer.drawGlow();
}

/// The line fades in from its left end to the middle of the text, then stays bright up to the star; it is
/// drawn from its left end up to `head`, where it is being traced.
void SceneRenderer::drawFlareLine(render::Renderer &renderer, const FlareStyle &style, float left,
                                  float right, float middle, float y, float head, float shine,
                                  float opacity) const
{
    const render::Color bright = faded(brightened(style.line, shine), opacity);
    drawFadingLine(renderer, left, middle, y, style.lineThickness, clear(bright), bright, head);
    drawFadingLine(renderer, middle, right, y, style.lineThickness, bright, bright, head);
}

/// While the button lights up, the line runs from left to right with a bright head, and the star flares when
/// the line reaches it. When the button is pressed, the star bursts and turns, and a pulse of light runs from
/// it to the far end of the line.
void SceneRenderer::drawFlareLight(render::Renderer &renderer, const FlareStyle &style, const Panel &panel,
                                   render::Rectangle textArea, render::Vector2 star, float opacity) const
{
    const float trace = std::clamp(panel.highlight, 0.0f, 1.0f);
    const float burst =
        std::clamp((panel.highlight - 1.0f) / std::max(style.pressedHighlight - 1.0f, 0.001f), 0.0f, 1.0f);
    const float shine = std::max(panel.highlight, 1.0f);
    const float left = textArea.x - style.lineBefore;
    const float right = star.x;
    const float head = left + ((right - left) * trace);
    const render::Color white = faded(style.litText, opacity);

    drawBlurred(renderer, textArea, textArea.height / 2.0f, style.textGlowBlur,
                faded(style.textGlow, opacity * trace));
    const float coreEnd = std::min(head, right);
    if (coreEnd > textArea.x) {
        const render::Rectangle core{textArea.x, star.y - (style.lineThickness / 2.0f), coreEnd - textArea.x,
                                     style.lineThickness};
        drawBlurred(renderer, core, style.lineThickness, style.lineGlowBlur * shine,
                    faded(brightened(style.lineGlow, shine), opacity));
    }
    drawFlareLine(renderer, style, left, right, textArea.x + (textArea.width / 2.0f), star.y, head, shine,
                  opacity);
    if (trace < 1.0f) {
        drawFadingLine(renderer, head - style.pulseLength, head, star.y, 2.0f * style.lineThickness,
                       clear(white), white, head);
    }

    // The star flares once the head of the line touches its left point, until the head reaches its centre
    const float starLeft = star.x - style.starSize;
    const float appearance = std::clamp((head - starLeft) / (right - starLeft), 0.0f, 1.0f);
    if (appearance > 0.0f) {
        const float growth = 1.0f + ((style.burstScale - 1.0f) * burst);
        const float size = style.starSize * ease(Easing::EaseOutBack, appearance) * growth;
        drawBlurred(renderer, {star.x - size / 2.0f, star.y - size / 2.0f, size, size}, size / 2.0f,
                    style.lineGlowBlur * (1.0f + burst), faded(brightened(style.lineGlow, shine), opacity));
        drawStar(renderer, star, size, style.starThickness * (1.0f + burst), style.burstTurn * burst, white);
    }
    if (burst > 0.0f) {
        const float pulse = star.x - ((star.x - left) * (1.0f - burst));
        const render::Color fading = faded(style.litText, opacity * burst);
        drawFadingLine(renderer, pulse, pulse + style.pulseLength, star.y, 2.0f * style.lineThickness, fading,
                       clear(fading), right);
    }
}

void SceneRenderer::drawFlare(render::Renderer &renderer, const FlareStyle &style, const Panel &panel,
                              const Text &text, render::Vector2 centre, float opacity) const
{
    const float trace = std::clamp(panel.highlight, 0.0f, 1.0f);
    const render::Vector2 measured =
        renderer.measureText(text.font, text.text, text.size, style.letterSpacing);
    if (panel.plain) {
        // Eased, so that the text neither jumps nor stops suddenly
        const float eased = trace * trace * (3.0f - (2.0f * trace));
        const render::Vector2 corner{centre.x - (measured.x / 2.0f), centre.y - (measured.y / 2.0f)};
        const render::Rectangle area{corner.x, corner.y, measured.x, measured.y};
        drawBlurred(renderer, area, area.height / 2.0f, style.textGlowBlur,
                    faded(style.textGlow, opacity * eased));
        renderer.drawText(text.font, text.text, corner, text.size,
                          faded(mix(style.text, style.litText, eased), opacity), style.letterSpacing);
        return;
    }
    const render::Vector2 topLeft{centre.x - (style.computeWidth(measured.x) / 2.0f) + style.padding,
                                  centre.y - (measured.y / 2.0f)};
    const render::Vector2 star{topLeft.x + measured.x + style.starGap + style.starSize, centre.y};
    if (trace > 0.0f) {
        drawFlareLight(renderer, style, panel, {topLeft.x, topLeft.y, measured.x, measured.y}, star, opacity);
    }
    renderer.drawText(text.font, text.text, topLeft, text.size,
                      faded(mix(style.text, style.litText, trace), opacity), style.letterSpacing);
}

void SceneRenderer::drawFormField(render::Renderer &renderer, const TextField &field, bool focused,
                                  render::Vector2 centre, float opacity) const
{
    const FieldStyle &style = _style.field;
    const float left = centre.x - (style.width / 2.0f);
    const float top = centre.y - (style.getHeight() / 2.0f);
    const bool hasError = !field.error.empty();

    const render::Color caption = hasError ? style.error : mix(style.caption, style.litCaption, field.light);
    renderer.drawText(style.captionFont, field.caption, {left, top}, style.captionSize,
                      faded(caption, opacity), style.captionSpacing);

    // The value, with the selection behind it and the cursor in front of it while the field is edited: their
    // places are the widths of the content before them
    const float valueTop = top + style.captionSize + style.captionGap;
    const auto offsetOf = [&](std::size_t byte) {
        return left + renderer
                          .measureText(style.valueFont, fieldContent(field, byte), style.valueSize,
                                       style.valueSpacing)
                          .x;
    };
    if (focused && !field.input.selection().empty()) {
        const input::TextInput::Range selection = field.input.selection();
        const float begin = offsetOf(selection.begin);
        renderer.drawRectangle({begin, valueTop, offsetOf(selection.end) - begin, style.valueSize},
                               faded(_style.selection, opacity));
    }
    renderer.drawText(style.valueFont, fieldContent(field), {left, valueTop}, style.valueSize,
                      faded(hasError ? style.error : style.value, opacity), style.valueSpacing);
    if (focused) {
        const float cursor = offsetOf(field.input.cursor()) + 2.0f;
        drawBlurred(renderer,
                    {cursor - (style.cursorWidth / 2.0f), valueTop, style.cursorWidth, style.valueSize},
                    style.cursorWidth / 2.0f, 8.0f, faded(style.glow, opacity));
        renderer.drawLine({cursor, valueTop}, {cursor, valueTop + style.valueSize}, style.cursorWidth,
                          faded(style.value, opacity));
    }

    // The rule, red with an error, or a line of light as the field lights up
    const float ruleY = valueTop + style.valueSize + style.ruleGap;
    renderer.drawRectangle({left, ruleY - (style.ruleThickness / 2.0f), style.width, style.ruleThickness},
                           faded(hasError ? style.error : style.rule, opacity));
    if (field.light > 0.0f) {
        render::Color glow = hasError ? style.error : style.glow;
        glow.a = style.glow.a;
        drawLightLine(renderer, left - style.lightOverhang, left + style.width + style.lightOverhang, ruleY,
                      style.lightThickness,
                      faded(hasError ? style.error : style.light, opacity * field.light),
                      faded(glow, opacity * field.light), 8.0f);
    }
    if (hasError) {
        renderer.drawText(style.captionFont, field.error, {left, ruleY + style.errorGap}, style.errorSize,
                          faded(style.error, opacity));
    }
}

void SceneRenderer::drawCodeField(render::Renderer &renderer, const TextField &field, bool focused,
                                  render::Vector2 centre, float opacity) const
{
    const FieldStyle &style = _style.field;
    const std::size_t cells = field.input.maxLength();
    const float left = centre.x - (style.getCodeWidth(cells) / 2.0f);
    const float top = centre.y - (style.getCodeHeight() / 2.0f);
    const bool hasError = !field.error.empty();

    const render::Color caption = hasError ? style.error : mix(style.caption, style.litCaption, field.light);
    renderer.drawText(style.captionFont, field.caption, {left, top}, style.captionSize,
                      faded(caption, opacity), style.captionSpacing);

    // A digit per cell, centred on it; the cell being typed is the first empty one, or the last
    const std::string &digits = field.input.text();
    const std::size_t current = std::min(digits.size(), cells == 0 ? 0 : cells - 1);
    const float digitTop = top + style.captionSize + style.captionGap;
    const float ruleY = digitTop + style.digitSize + style.ruleGap;
    for (std::size_t cell = 0; cell < cells; ++cell) {
        const float cellLeft = left + (static_cast<float>(cell) * (style.cellWidth + style.cellGap));
        const float middle = cellLeft + (style.cellWidth / 2.0f);
        if (cell < digits.size()) {
            const std::string digit(1, digits[cell]);
            const float width = renderer.measureText(style.valueFont, digit, style.digitSize, 0.0f).x;
            renderer.drawText(style.valueFont, digit, {middle - (width / 2.0f), digitTop}, style.digitSize,
                              faded(hasError ? style.error : style.value, opacity));
        }
        const bool isCurrent = focused && cell == current;
        const render::Color rule = hasError               ? style.error
                                   : cell < digits.size() ? style.filledRule
                                                          : style.rule;
        renderer.drawRectangle(
            {cellLeft, ruleY - (style.ruleThickness / 2.0f), style.cellWidth, style.ruleThickness},
            faded(rule, opacity));
        if (isCurrent && field.light > 0.0f) {
            drawLightLine(renderer, cellLeft - (style.cellGap / 2.0f),
                          cellLeft + style.cellWidth + (style.cellGap / 2.0f), ruleY, style.lightThickness,
                          faded(style.light, opacity * field.light), faded(style.glow, opacity * field.light),
                          8.0f);
            if (cell >= digits.size()) {
                renderer.drawLine({middle, digitTop}, {middle, digitTop + style.digitSize}, style.cursorWidth,
                                  faded(style.value, opacity * field.light));
            }
        }
    }
    if (hasError) {
        renderer.drawText(style.captionFont, field.error, {left, ruleY + style.errorGap}, style.errorSize,
                          faded(style.error, opacity));
    }
}

void SceneRenderer::drawListRow(render::Renderer &renderer, const ListRow &row, const Text &text,
                                const Panel *panel, render::Vector2 centre, float opacity) const
{
    const ListStyle &style = _style.list;
    const float left = centre.x - (row.width / 2.0f);
    const float right = centre.x + (row.width / 2.0f);
    const float bottom = centre.y + (style.height / 2.0f);
    const bool enabled = panel != nullptr;
    const float trace = enabled ? std::clamp(panel->highlight, 0.0f, 1.0f) : 0.0f;

    float textLeft = left;
    if (row.avatar) {
        Avatar avatar = *row.avatar;
        avatar.size = style.avatarSize;
        drawAvatar(renderer, avatar, {left + (style.avatarSize / 2.0f), centre.y},
                   opacity * (enabled ? 1.0f : 0.4f));
        textLeft += style.avatarSize + style.avatarGap;
    }

    // The title and its detail, one above the other, in the middle of the row
    const float block = style.titleSize + (row.detail.empty() ? 0.0f : style.lineGap + style.detailSize);
    const float titleTop = centre.y - (block / 2.0f);
    const render::Color title = enabled ? mix(style.title, style.litTitle, trace) : style.disabled;
    if (trace > 0.0f) {
        const render::Vector2 measured =
            renderer.measureText(style.titleFont, text.text, style.titleSize, style.titleSpacing);
        drawBlurred(renderer, {textLeft, titleTop, measured.x, measured.y}, measured.y / 2.0f,
                    _style.flare.textGlowBlur, faded(_style.flare.textGlow, opacity * trace));
    }
    renderer.drawText(style.titleFont, text.text, {textLeft, titleTop}, style.titleSize,
                      faded(title, opacity), style.titleSpacing);
    if (!row.detail.empty()) {
        renderer.drawText(
            style.detailFont, row.detail, {textLeft, titleTop + style.titleSize + style.lineGap},
            style.detailSize,
            faded(enabled ? mix(style.detail, style.litDetail, trace) : style.disabled, opacity));
    }

    // The side text, on the right, before the room of the star
    const float starRoom = (2.0f * style.starSize) + 12.0f;
    if (!row.side.empty()) {
        const render::Vector2 measured =
            renderer.measureText(style.titleFont, row.side, style.sideSize, style.sideSpacing);
        renderer.drawText(style.titleFont, row.side,
                          {right - starRoom - measured.x, centre.y - (measured.y / 2.0f)}, style.sideSize,
                          faded(enabled ? mix(style.side, style.litTitle, trace) : style.disabled, opacity),
                          style.sideSpacing);
    }

    // The rule under the row, and the line of light that traces itself along it while the row is lit
    renderer.drawRectangle({left, bottom - style.ruleThickness, row.width, style.ruleThickness},
                           faded(style.rule, opacity));
    if (trace <= 0.0f) {
        return;
    }
    const FlareStyle &flare = _style.flare;
    const float shine = std::max(panel->highlight, 1.0f);
    const float lineLeft = left - style.lightBefore;
    const float head = lineLeft + ((right - lineLeft) * trace);
    const float y = bottom - (style.ruleThickness / 2.0f);
    drawFlareLine(renderer, flare, lineLeft, right, left + (row.width / 2.0f), y, head, shine, opacity);
    if (head >= right - style.starSize) {
        const render::Vector2 star{right - style.starSize, centre.y};
        const float burst = std::clamp(
            (panel->highlight - 1.0f) / std::max(flare.pressedHighlight - 1.0f, 0.001f), 0.0f, 1.0f);
        const float size = style.starSize * (1.0f + ((flare.burstScale - 1.0f) * burst));
        drawBlurred(renderer, {star.x - size / 2.0f, star.y - size / 2.0f, size, size}, size / 2.0f,
                    flare.lineGlowBlur, faded(brightened(flare.lineGlow, shine), opacity));
        drawStar(renderer, star, size, flare.starThickness, flare.burstTurn * burst,
                 faded(flare.litText, opacity));
    }
}

void SceneRenderer::drawAvatar(render::Renderer &renderer, const Avatar &avatar, render::Vector2 centre,
                               float opacity) const
{
    const AvatarStyle &style = _style.avatar;
    const float radius = avatar.size / 2.0f;
    const float inner = radius - (avatar.size * style.ringGap);
    if (avatar.kind == AvatarKind::Discord) {
        drawHalo(renderer, centre, radius, avatar.size * style.glowSpread, faded(style.glow, opacity));
    }
    renderer.drawCircle(centre, inner, faded(style.disc, opacity));
    if (avatar.picture) {
        renderer.drawTexture(*avatar.picture, {0.0f, 0.0f, avatar.pictureSize, avatar.pictureSize},
                             {centre.x - inner, centre.y - inner, 2.0f * inner, 2.0f * inner}, 0.0f,
                             faded(render::colors::White, opacity));
    } else if (!avatar.initial.empty()) {
        const float size = avatar.size * style.initialSize;
        const render::Vector2 measured = renderer.measureText(style.font, avatar.initial, size, 0.0f);
        renderer.drawText(
            style.font, avatar.initial, {centre.x - (measured.x / 2.0f), centre.y - (measured.y / 2.0f)},
            size, faded(avatar.kind == AvatarKind::Guest ? style.guestInitial : style.initial, opacity));
    }
    if (avatar.kind == AvatarKind::Guest) {
        // A dotted ring: short arcs, as many as the style says, with gaps as long as them
        const int dots = std::max(style.guestDots, 1);
        const float step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(dots);
        for (int dot = 0; dot < dots; ++dot) {
            const float from = static_cast<float>(dot) * step;
            const float to = from + (step / 2.0f);
            renderer.drawLine({centre.x + (std::cos(from) * radius), centre.y + (std::sin(from) * radius)},
                              {centre.x + (std::cos(to) * radius), centre.y + (std::sin(to) * radius)},
                              style.ringThickness, faded(style.guestRing, opacity));
        }
        return;
    }
    renderer.drawCircleOutline(
        centre, radius, style.ringThickness,
        faded(avatar.kind == AvatarKind::Discord ? style.discordRing : style.ring, opacity));
}

void SceneRenderer::drawTitle(render::Renderer &renderer, const Text &text, render::Vector2 centre,
                              float scale, float opacity) const
{
    const TitleStyle &style = _style.title;
    const float size = text.size * scale;
    const float spacing = style.letterSpacing * size;
    const render::Vector2 measured = renderer.measureText(text.font, text.text, size, spacing);
    const render::Vector2 topLeft{centre.x - measured.x / 2.0f, centre.y - measured.y / 2.0f};

    drawTextGlow(renderer, text.font, text.text, topLeft, size, spacing, faded(style.glow, opacity),
                 style.glowBlur * scale);
    std::vector<render::ColorStop> chrome = style.chrome;
    for (render::ColorStop &stop : chrome) {
        stop.color = faded(stop.color, opacity);
    }
    renderer.drawGradientText(text.font, text.text, topLeft, size, chrome, spacing);
}

void SceneRenderer::drawRowFrame(render::Renderer &renderer, render::Rectangle area, float light,
                                 float opacity) const
{
    const SettingStyle &style = _style.setting;
    const float bottom = area.y + area.height;
    renderer.drawRectangle({area.x, bottom - style.ruleThickness, area.width, style.ruleThickness},
                           faded(style.rule, opacity));
    const float lit = smooth(light);
    if (lit <= 0.0f) {
        return;
    }
    const render::Color veil = faded(style.veil, opacity * lit);
    renderer.drawRoundedRectangle({area.x, area.y, area.width * style.veilLength, area.height}, 0.0f, veil,
                                  clear(veil));
    const render::Rectangle mark{area.x, area.y + ((area.height - style.markHeight) / 2.0f), style.markWidth,
                                 style.markHeight};
    drawBlurred(renderer, mark, style.markWidth / 2.0f, style.markGlowBlur,
                faded(style.markGlow, opacity * lit));
    renderer.drawRectangle(mark, faded(style.mark, opacity * lit));
    // The line of light traces itself from the left edge while the row lights up
    drawLightLine(renderer, area.x, area.x + (area.width * lit), bottom - (style.ruleThickness / 2.0f),
                  style.ruleThickness, faded(style.light, opacity), faded(style.lightGlow, opacity),
                  style.lightGlowBlur);
}

void SceneRenderer::drawSettingRow(render::Renderer &renderer, const SettingRow &row, const Text &text,
                                   const Panel *panel, render::Vector2 centre, float opacity) const
{
    const SettingStyle &style = _style.setting;
    const render::Rectangle area{centre.x - (row.width / 2.0f), centre.y - (style.height / 2.0f), row.width,
                                 style.height};
    const float light = panel != nullptr ? std::clamp(panel->highlight, 0.0f, 1.0f) : 0.0f;
    const float lit = smooth(light);
    drawRowFrame(renderer, area, light, opacity);

    const render::Vector2 caption = renderer.measureText(text.font, text.text, text.size, text.spacing);
    renderer.drawText(text.font, text.text, {area.x + style.inset, centre.y - (caption.y / 2.0f)}, text.size,
                      faded(mix(style.caption, style.litCaption, lit), opacity), text.spacing);

    const float right = area.x + area.width - style.valueInset;
    const render::Color chevron = faded(style.litValue, opacity);
    if (!row.change) {
        // A row that opens something: a chevron alone, brighter while lit
        drawChevron(renderer, right - style.chevronWidth, centre.y, style.chevronWidth, style.chevronHeight,
                    style.chevronThickness, 1, brightened(chevron, 0.55f + (0.45f * lit)));
        return;
    }
    const std::string value = row.value ? row.value() : std::string();
    const render::Vector2 measured =
        renderer.measureText(style.valueFont, value, style.valueSize, style.valueSpacing);
    const float valueRight = area.x + area.width - style.getValueInset();
    const render::Vector2 topLeft{valueRight - measured.x, centre.y - (measured.y / 2.0f)};
    if (lit > 0.0f) {
        drawBlurred(renderer, {topLeft.x, topLeft.y, measured.x, measured.y}, measured.y / 2.0f,
                    _style.flare.textGlowBlur, faded(_style.flare.textGlow, opacity * lit));
        // The chevrons around the value say that Left and Right change it
        const render::Color shown = brightened(chevron, 0.9f * lit);
        drawChevron(renderer, topLeft.x - style.chevronGap - style.chevronWidth, centre.y, style.chevronWidth,
                    style.chevronHeight, style.chevronThickness, -1, shown);
        drawChevron(renderer, valueRight + style.chevronGap, centre.y, style.chevronWidth,
                    style.chevronHeight, style.chevronThickness, 1, shown);
    }
    renderer.drawText(style.valueFont, value, topLeft, style.valueSize,
                      faded(mix(style.value, style.litValue, lit), opacity), style.valueSpacing);
}

float SceneRenderer::drawKeyCap(render::Renderer &renderer, const KeyCap &key, float left, float middle,
                                float opacity) const
{
    const KeyStyle &style = _style.keys;
    // Three dots of ASCII: the fonts may have no ellipsis
    const std::string label = key.state == KeyCapState::Waiting ? std::string("...") : key.label;
    const render::Vector2 measured =
        renderer.measureText(style.font, label, style.textSize, style.letterSpacing);
    const float width = std::max(style.minWidth, measured.x + (2.0f * style.padding));
    const render::Rectangle area{left, middle - (style.height / 2.0f), width, style.height};

    render::Color fill = style.fill;
    render::Color frame = style.frame;
    render::Color text = style.text;
    if (key.state == KeyCapState::New) {
        drawBlurred(renderer, area, style.radius, style.glowBlur, faded(style.newGlow, opacity));
        fill = style.newFill;
        frame = style.newFrame;
    } else if (key.state == KeyCapState::Conflict) {
        fill = style.conflictFill;
        frame = style.conflictFrame;
        text = style.conflict;
    }
    renderer.drawRoundedRectangle(area, style.radius, faded(fill, opacity), faded(fill, opacity));
    if (key.state == KeyCapState::Waiting) {
        drawDashedOutline(renderer, area, style.border, style.dash, faded(style.waitingFrame, opacity));
    } else {
        renderer.drawRoundedRectangleOutline(area, style.radius, style.border, faded(frame, opacity));
    }
    renderer.drawText(style.font, label, {left + ((width - measured.x) / 2.0f), middle - (measured.y / 2.0f)},
                      style.textSize, faded(text, opacity), style.letterSpacing);
    return width;
}

float SceneRenderer::drawPadButton(render::Renderer &renderer, const std::string &label, float left,
                                   float middle, float opacity) const
{
    const KeyStyle &style = _style.keys;
    const render::Vector2 measured =
        renderer.measureText(style.font, label, style.textSize, style.letterSpacing);
    const float width = std::max(style.minWidth, measured.x + (2.0f * style.padding));
    const render::Rectangle area{left, middle - (style.height / 2.0f), width, style.height};
    const float radius = style.height / 2.0f;
    renderer.drawRoundedRectangle(area, radius, faded(style.buttonFill, opacity),
                                  faded(style.buttonFill, opacity));
    renderer.drawRoundedRectangleOutline(area, radius, style.border, faded(style.buttonFrame, opacity));
    renderer.drawText(style.font, label, {left + ((width - measured.x) / 2.0f), middle - (measured.y / 2.0f)},
                      style.textSize, faded(style.buttonText, opacity), style.letterSpacing);
    return width;
}

void SceneRenderer::drawKeyRow(render::Renderer &renderer, const KeyRow &row, const Text &text,
                               const Panel *panel, render::Vector2 centre, float opacity) const
{
    const SettingStyle &setting = _style.setting;
    const KeyStyle &style = _style.keys;
    if (row.dimmed && row.dimmed()) {
        opacity *= setting.dimOpacity;
    }
    const render::Rectangle area{centre.x - (row.width / 2.0f), centre.y - (style.rowHeight / 2.0f),
                                 row.width, style.rowHeight};
    const float light = panel != nullptr ? std::clamp(panel->highlight, 0.0f, 1.0f) : 0.0f;
    drawRowFrame(renderer, area, light, opacity);

    const render::Vector2 caption = renderer.measureText(text.font, text.text, text.size, text.spacing);
    renderer.drawText(text.font, text.text, {area.x + setting.inset, centre.y - (caption.y / 2.0f)},
                      text.size, faded(mix(setting.caption, setting.litCaption, smooth(light)), opacity),
                      text.spacing);

    float left = area.x + row.keysColumn;
    for (const KeyCap &key : row.keys ? row.keys() : std::vector<KeyCap>{}) {
        left += drawKeyCap(renderer, key, left, centre.y, opacity) + style.gap;
    }
    left = area.x + row.buttonsColumn;
    for (const std::string &button : row.buttons ? row.buttons() : std::vector<std::string>{}) {
        left += drawPadButton(renderer, button, left, centre.y, opacity) + style.gap;
    }
}

void SceneRenderer::drawSectionLabel(render::Renderer &renderer, const SectionLabel &label, const Text &text,
                                     render::Vector2 centre, float opacity) const
{
    const SettingStyle &style = _style.setting;
    const render::Vector2 measured = renderer.measureText(text.font, text.text, text.size, text.spacing);
    const render::Vector2 topLeft{centre.x - (measured.x / 2.0f), centre.y - (measured.y / 2.0f)};
    renderer.drawText(text.font, text.text, topLeft, text.size, faded(text.color, opacity), text.spacing);
    const float left = topLeft.x + measured.x + style.sectionGap;
    const float right = topLeft.x + label.width;
    if (label.width > 0.0f && right > left) {
        renderer.drawRectangle(
            {left, centre.y - (style.ruleThickness / 2.0f), right - left, style.ruleThickness},
            faded(style.sectionRule, opacity));
    }
}

void SceneRenderer::drawSlider(render::Renderer &renderer, const Slider &slider, const SliderTrack &track,
                               const Text &text, render::Vector2 centre, float opacity) const
{
    const SliderStyle &style = _style.slider;
    const float value = std::clamp(slider.get ? slider.get() : 0.0f, 0.0f, 1.0f);
    // The looks ease in and out, instead of changing at a constant speed
    const float lit = smooth(track.hover);
    const float focus = std::max(track.focus, track.hover);
    const float grab = smooth(track.grab);

    // The row, lit as the rows of a page of settings
    const float rowWidth = style.getAreaWidth(track.width);
    const render::Rectangle area{centre.x - (rowWidth / 2.0f), centre.y - (style.getHeight() / 2.0f),
                                 rowWidth, style.getHeight()};
    drawRowFrame(renderer, area, focus, opacity);
    const float shown = smooth(focus);

    // The caption on the left, the percentage on the column of the values of the other rows
    const float captionTop = area.y + style.paddingTop;
    renderer.drawText(text.font, slider.caption, {area.x + style.inset, captionTop}, style.captionSize,
                      faded(mix(style.caption, style.litCaption, shown), opacity), style.letterSpacing);
    const std::string percentage = std::to_string(static_cast<int>(std::lround(value * 100.0f))) + " %";
    const render::Vector2 measured =
        renderer.measureText(style.valueFont, percentage, style.valueSize, style.valueSpacing);
    renderer.drawText(style.valueFont, percentage,
                      {area.x + rowWidth - style.valueInset - measured.x,
                       captionTop + ((style.captionSize - measured.y) / 2.0f)},
                      style.valueSize, faded(mix(style.value, style.litCaption, shown), opacity),
                      style.valueSpacing);

    // The rail, from the caption to the percentage, and its part up to the value in light
    const float left = style.getRailLeft(centre.x, track.width);
    const float width = style.getRailWidth(track.width);
    const float y = style.getRailY(centre.y);
    renderer.drawRoundedRectangle({left, y - (style.railThickness / 2.0f), width, style.railThickness},
                                  style.railThickness / 2.0f, faded(style.rail, opacity),
                                  faded(style.rail, opacity));
    const float filled = value * width;
    if (filled > 0.0f) {
        const render::Rectangle fill{left, y - (style.fillThickness / 2.0f), filled, style.fillThickness};
        drawBlurred(renderer, fill, style.fillThickness / 2.0f, style.fillGlowBlur,
                    faded(brightened(style.fillGlow, 1.0f + shown + lit + grab), opacity));
        const render::Color color = faded(mix(style.fill, style.litFill, shown), opacity);
        renderer.drawRoundedRectangle(fill, style.fillThickness / 2.0f, color, color);
    }

    // The handle, a bar across the rail, grows when pointed at, and more when dragged
    const float height = style.handleHeight + ((style.handleHoverHeight - style.handleHeight) * lit) +
                         ((style.handleDragHeight - style.handleHoverHeight) * grab);
    const render::Rectangle handle{left + filled - (style.handleWidth / 2.0f), y - (height / 2.0f),
                                   style.handleWidth, height};
    drawBlurred(renderer, handle, style.handleWidth / 2.0f, style.handleGlowBlur,
                faded(brightened(style.handleGlow, 0.5f + (0.5f * shown) + lit + grab), opacity));
    renderer.drawRoundedRectangle(handle, style.handleWidth / 2.0f, faded(style.handle, opacity),
                                  faded(style.handle, opacity));
}

void SceneRenderer::drawSwitch(render::Renderer &renderer, const Toggle &toggle, const SwitchTrack &track,
                               const Text &text, render::Vector2 centre, float opacity) const
{
    const SwitchStyle &style = _style.toggle;
    const float hover = smooth(track.hover);
    const float focus = smooth(std::max(track.focus, track.hover));
    const float position = std::clamp(track.position, 0.0f, 1.0f);
    const float left = centre.x - (style.width / 2.0f);

    // The caption on the left, vertically centred
    const float captionHeight = renderer.measureText(text.font, toggle.caption, style.captionSize).y;
    renderer.drawText(text.font, toggle.caption, {left, centre.y - (captionHeight / 2.0f)}, style.captionSize,
                      faded(mix(style.caption, style.litCaption, focus), opacity), style.letterSpacing);

    // "OFF", the link, "ON", aligned on the right: the same places as the area that lights up
    const SwitchLayout layout =
        computeSwitchLayout(style, centre, [&](const char *label, float size, float spacing) {
            return renderer.measureText(text.font, label, size, spacing);
        });
    const float offX = layout.offX;
    const float onX = layout.onX;
    const float linkLeft = layout.linkLeft;
    const float linkRight = layout.linkRight;
    const float labelHeight = layout.labelHeight;
    const float dotX = linkLeft + (position * style.linkWidth);

    // The label of the side that is on lights up; the other one goes dim. A switch that is off lights its OFF
    // label when pointed at
    const render::Color offColor = mix(mix(style.label, style.litLabel, hover), style.dimLabel, position);
    const render::Color onColor = mix(style.dimLabel, style.litLabel, position);
    const float offLight = (1.0f - position) * hover;
    const render::Vector2 offTop{offX, centre.y - (labelHeight / 2.0f)};
    const render::Vector2 onTop{onX, centre.y - (labelHeight / 2.0f)};
    drawTextGlow(renderer, text.font, "OFF", offTop, style.labelSize, style.labelSpacing,
                 faded(brightened(style.labelGlow, offLight), opacity), style.labelGlowBlur);
    drawTextGlow(renderer, text.font, "ON", onTop, style.labelSize, style.labelSpacing,
                 faded(brightened(style.labelGlow, position), opacity), style.labelGlowBlur);
    renderer.drawText(text.font, "OFF", offTop, style.labelSize, faded(offColor, opacity),
                      style.labelSpacing);
    renderer.drawText(text.font, "ON", onTop, style.labelSize, faded(onColor, opacity), style.labelSpacing);

    // The link, lit between the OFF label and the dot once on, and from the dot to the ON label when pointed
    // at
    renderer.drawRoundedRectangle(
        {linkLeft, centre.y - (style.linkThickness / 2.0f), style.linkWidth, style.linkThickness},
        style.linkThickness / 2.0f, faded(style.link, opacity), faded(style.link, opacity));
    const auto drawLit = [&](float from, float to, float amount) {
        if (to > from && amount > 0.0f) {
            render::Color lit = style.litLink;
            lit.a = static_cast<std::uint8_t>(static_cast<float>(lit.a) * amount);
            renderer.drawRoundedRectangle(
                {from, centre.y - (style.linkThickness / 2.0f), to - from, style.linkThickness},
                style.linkThickness / 2.0f, faded(lit, opacity), faded(lit, opacity));
        }
    };
    drawLit(linkLeft, dotX, position);
    drawLit(dotX, linkRight, hover * (1.0f - position));

    const float dotLight = std::max(hover, position);
    drawHalo(renderer, {dotX, centre.y}, style.dotRadius, style.dotGlowSpread * dotLight,
             faded(style.dotGlow, opacity));
    renderer.drawCircle({dotX, centre.y}, style.dotRadius,
                        faded(mix(style.dot, style.litDot, dotLight), opacity));
}

void SceneRenderer::drawGauge(render::Renderer &renderer, const Gauge &gauge, render::Vector2 centre,
                              float opacity) const
{
    const GaugeStyle &style = _style.gauge;
    const float value = std::clamp(gauge.get ? gauge.get() : 0.0f, 0.0f, 1.0f);
    const bool isFull = value >= 0.999f;
    const bool isLow = value <= gauge.low;
    const float width = gauge.width > 0.0f ? gauge.width : style.width;
    const float top = centre.y - (style.getHeight() / 2.0f);
    const float left = centre.x - (width / 2.0f);
    const float y = gauge.caption.empty() ? centre.y : style.getRailY(centre.y);

    if (!gauge.caption.empty()) {
        const std::string percentage = std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%";
        drawCaption(renderer, style.font, gauge.caption, percentage, left, left + width, top,
                    style.captionSize, style.letterSpacing, faded(style.caption, opacity),
                    faded(style.litCaption, opacity));
    }
    renderer.drawRoundedRectangle({left, y - (style.railThickness / 2.0f), width, style.railThickness},
                                  style.railThickness / 2.0f, faded(style.rail, opacity),
                                  faded(style.rail, opacity));
    const float length = value * width;
    if (length <= 0.0f) {
        return;
    }
    // The light is transparent at the start of the gauge and white at its value, red when it is low; a full
    // gauge shines more
    const render::Color colour = isLow ? style.lowLight : style.light;
    const render::Color glow = isLow ? style.lowGlow : style.glow;
    const float thickness = isFull ? style.fullLightThickness : style.lightThickness;
    const render::Rectangle light{left, y - (thickness / 2.0f), length, thickness};
    drawBlurred(renderer, light, thickness / 2.0f, style.glowSpread * (isFull ? 2.0f : 1.0f),
                faded(brightened(glow, isFull ? 1.8f : 1.0f), opacity));
    renderer.drawRoundedRectangle(light, thickness / 2.0f, faded(clear(colour), opacity),
                                  faded(colour, opacity));
    drawSpark(renderer, {left + length, y}, style.sparkLength * (isFull ? 1.3f : 1.0f), style.sparkHeight,
              faded(colour, opacity));
}

void SceneRenderer::drawRing(render::Renderer &renderer, const Ring &ring, render::Vector2 centre,
                             float opacity) const
{
    constexpr int glowLayers = 4;
    constexpr float glowSpread = 6.0f;
    renderer.drawArc(centre, ring.radius, ring.thickness, 0.0f, 360.0f, faded(ring.track, opacity));
    const float value = std::clamp(ring.value, 0.0f, 1.0f);
    if (value <= 0.0f) {
        return;
    }
    // Angles turn clockwise on the screen: the light goes anticlockwise from the right, so it starts at its
    // end
    const float end = -360.0f * value;
    render::Color glow = ring.light;
    glow.a = static_cast<std::uint8_t>(static_cast<float>(ring.light.a) * 0.12f);
    for (int layer = 1; layer <= glowLayers; ++layer) {
        const float spread = glowSpread * static_cast<float>(layer) / static_cast<float>(glowLayers);
        renderer.drawArc(centre, ring.radius + spread, ring.thickness + (2.0f * spread), end, 0.0f,
                         faded(glow, opacity));
    }
    renderer.drawArc(centre, ring.radius, ring.thickness, end, 0.0f, faded(ring.light, opacity));
    const float radians = end * std::numbers::pi_v<float> / 180.0f;
    const float middle = ring.radius - (ring.thickness / 2.0f);
    drawSpark(renderer, {centre.x + (middle * std::cos(radians)), centre.y + (middle * std::sin(radians))},
              ring.radius * 0.3f, ring.radius * 0.1f, faded(ring.light, opacity));
}

void SceneRenderer::drawAnnouncement(render::Renderer &renderer, const Toast &toast, render::Vector2 centre,
                                     float progress) const
{
    const ToastStyle &style = _style.toast;
    const render::Color accent = style.getAccent(toast.type);
    render::Color glow = accent;
    glow.a = style.glow.a;

    // The entrance: the icon pops, the title fades in while its letters tighten, the line of light draws
    // itself from the middle with a flash, then the detail rises
    const float iconPop = stage(progress, 0.0f, 0.4f, Easing::EaseOutBack);
    const float iconFade = stage(progress, 0.0f, 0.2f, Easing::Linear);
    const float titleIn = stage(progress, 0.1f, 0.65f);
    const float lineIn = stage(progress, 0.25f, 0.8f);
    const float lineFlash = flash(stage(progress, 0.3f, 0.9f, Easing::Linear));
    const float detailIn = stage(progress, 0.55f, 1.0f);

    // The icon and the title, on one line
    const float top = centre.y - (style.getHeight(ToastKind::Announcement) / 2.0f);
    const std::string &title = toast.getHeadline();
    const float spacing =
        style.announcementTitleSpacing * (1.0f + ((style.spacingIn - 1.0f) * (1.0f - titleIn)));
    const render::Vector2 measured =
        renderer.measureText(style.nameFont, title, style.announcementTitleSize, spacing);
    const float rowLeft = centre.x - ((style.iconSize + style.gap + measured.x) / 2.0f);
    drawToastIcon(renderer, toast.type, {rowLeft + (style.iconSize / 2.0f), top + (measured.y / 2.0f)},
                  std::max(style.iconSize * iconPop, 0.01f), style.iconThickness, faded(accent, iconFade));
    const render::Vector2 titleTopLeft{rowLeft + style.iconSize + style.gap, top + (8.0f * (1.0f - titleIn))};
    drawTextGlow(renderer, style.nameFont, title, titleTopLeft, style.announcementTitleSize, spacing,
                 faded(glow, titleIn), style.glowBlur);
    renderer.drawText(style.nameFont, title, titleTopLeft, style.announcementTitleSize,
                      faded(accent, titleIn), spacing);

    // The line of light under it, which shines while it is drawn, then the detail in grey
    const float lineY =
        top + style.announcementTitleSize + style.announcementGap + (style.announcementLineThickness / 2.0f);
    render::Color lineColor = accent;
    lineColor.a = 217;
    render::Color lineGlow = accent;
    lineGlow.a = 90;
    const float half = (style.announcementLineWidth / 2.0f) * lineIn;
    drawLightLine(renderer, centre.x - half, centre.x + half, lineY, style.announcementLineThickness,
                  brightened(lineColor, 1.0f + lineFlash), brightened(lineGlow, 1.0f + (2.0f * lineFlash)),
                  8.0f);
    const std::string &detail = toast.getBody();
    const float detailWidth = renderer.measureText(style.messageFont, detail, style.announcementDetailSize).x;
    renderer.drawText(style.messageFont, detail,
                      {centre.x - (detailWidth / 2.0f), lineY + (style.announcementLineThickness / 2.0f) +
                                                            style.announcementGap +
                                                            (10.0f * (1.0f - detailIn))},
                      style.announcementDetailSize, faded(style.message, detailIn));
}

void SceneRenderer::drawBanner(render::Renderer &renderer, const Toast &toast, render::Vector2 centre,
                               float progress) const
{
    const ToastStyle &style = _style.toast;
    const render::Color accent = style.getAccent(toast.type);
    const bool isAlert = toast.type == ToastType::Warning || toast.type == ToastType::Error;

    // The entrance: a line of light draws itself from the middle, flashing; the band opens around it, the two
    // lines moving apart; the sparks fly out, the title fades in while its letters tighten, then the detail
    // rises
    const float open = stage(progress, 0.0f, 0.4f);
    const float spread = stage(progress, 0.25f, 0.75f);
    const float openFlash = flash(stage(progress, 0.0f, 0.55f, Easing::Linear));
    const float titleIn = stage(progress, 0.45f, 0.9f);
    const float sparkIn = stage(progress, 0.45f, 0.85f);
    const float detailIn = stage(progress, 0.7f, 1.0f);

    const float height = style.bannerHeight;
    const float half = (toast.width / 2.0f) * open;
    const float left = centre.x - half;
    const float right = centre.x + half;
    const float upper = centre.y - ((height / 2.0f) * spread);
    const float lower = centre.y + ((height / 2.0f) * spread);

    // The veil between the two lines, then the lines
    render::Color veil = accent;
    veil.a = static_cast<std::uint8_t>(style.bannerVeil * 255.0f * spread);
    if (lower - upper > 8.0f) {
        renderer.drawRoundedRectangle({left, upper + 4.0f, half, lower - upper - 8.0f}, 0.0f, clear(veil),
                                      veil);
        renderer.drawRoundedRectangle({centre.x, upper + 4.0f, half, lower - upper - 8.0f}, 0.0f, veil,
                                      clear(veil));
    }
    render::Color lineColor = accent;
    lineColor.a = 178;
    render::Color lineGlow = accent;
    lineGlow.a = 90;
    for (const float y :
         {upper + (style.bannerLineThickness / 2.0f), lower - (style.bannerLineThickness / 2.0f)}) {
        drawLightLine(renderer, left, right, y, style.bannerLineThickness,
                      brightened(lineColor, 1.0f + (0.9f * openFlash)),
                      brightened(lineGlow, 1.0f + (2.0f * openFlash)), 8.0f);
    }

    // The title, with a spark on each side, then the detail under it. The letters are spaced wide at first,
    // then tighten; the sparks start from the middle and fly to their places
    const std::string &title = toast.getHeadline();
    const float titleSpacing =
        style.bannerTitleSpacing * (1.0f + ((style.spacingIn - 1.0f) * (1.0f - titleIn)));
    const render::Vector2 finalSize =
        renderer.measureText(style.nameFont, title, style.bannerTitleSize, style.bannerTitleSpacing);
    const render::Vector2 measured =
        renderer.measureText(style.nameFont, title, style.bannerTitleSize, titleSpacing);
    const float top = centre.y - (height / 2.0f);
    const render::Vector2 titleTopLeft{centre.x - (measured.x / 2.0f), top + (height * 0.2f)};
    render::Color glow = accent;
    glow.a = style.glow.a;
    drawTextGlow(renderer, style.nameFont, title, titleTopLeft, style.bannerTitleSize, titleSpacing,
                 faded(glow, titleIn), style.glowBlur * 2.0f);
    if (isAlert) {
        renderer.drawText(style.nameFont, title, titleTopLeft, style.bannerTitleSize, faded(accent, titleIn),
                          titleSpacing);
    } else {
        std::vector<render::ColorStop> chrome = _style.title.chrome;
        for (render::ColorStop &stop : chrome) {
            stop.color = faded(stop.color, titleIn);
        }
        renderer.drawGradientText(style.nameFont, title, titleTopLeft, style.bannerTitleSize, chrome,
                                  titleSpacing);
    }
    const float sparkY = top + (height * 0.38f);
    const float sparkDistance = ((finalSize.x / 2.0f) + style.bannerSparkGap) * sparkIn;
    for (const float side : {-1.0f, 1.0f}) {
        drawSpark(renderer, {centre.x + (side * sparkDistance), sparkY}, style.bannerSparkSize * sparkIn,
                  style.bannerSparkSize / 3.0f, faded(accent, sparkIn));
    }

    const std::string &detail = toast.getBody();
    const float detailSpacing = style.bannerDetailSpacing * (1.0f + (0.5f * (1.0f - detailIn)));
    const float detailWidth =
        renderer.measureText(style.messageFont, detail, style.bannerDetailSize, detailSpacing).x;
    renderer.drawText(style.messageFont, detail,
                      {centre.x - (detailWidth / 2.0f), top + (height * 0.66f) + (12.0f * (1.0f - detailIn))},
                      style.bannerDetailSize, faded(style.message, detailIn), detailSpacing);
}

void SceneRenderer::drawToast(render::Renderer &renderer, const Toast &toast, render::Vector2 centre,
                              float opacity) const
{
    // One way of drawing each kind of toast, in the order of ToastKind
    using Draw = void (SceneRenderer::*)(render::Renderer &, const Toast &, render::Vector2, float) const;
    constexpr std::array<Draw, toastKindCount> draws{
        &SceneRenderer::drawEventLine, &SceneRenderer::drawAnnouncement, &SceneRenderer::drawBanner};
    (this->*draws[static_cast<std::size_t>(toast.kind)])(renderer, toast, centre, opacity);
}

void SceneRenderer::drawEventLine(render::Renderer &renderer, const Toast &toast, render::Vector2 centre,
                                  float opacity) const
{
    const ToastStyle &style = _style.toast;
    const render::Color accent = style.getAccent(toast.type);
    // The glow takes the colour of the type, with the strength of the style
    render::Color glow = accent;
    glow.a = style.glow.a;

    // The subject (or the message alone), the message, the actor and the icon, from the left
    float x = centre.x - (toast.width / 2.0f);
    const auto drawName = [&](const std::string &name, render::Color color, render::Color light) {
        const render::Vector2 measured =
            renderer.measureText(style.nameFont, name, style.nameSize, style.letterSpacing);
        const render::Vector2 topLeft{x, centre.y - (measured.y / 2.0f)};
        drawTextGlow(renderer, style.nameFont, name, topLeft, style.nameSize, style.letterSpacing,
                     faded(light, opacity), style.glowBlur);
        renderer.drawText(style.nameFont, name, topLeft, style.nameSize, faded(color, opacity),
                          style.letterSpacing);
        x += measured.x + style.gap;
    };
    drawName(toast.getHeadline(), accent, glow);
    if (const std::string body = toast.getBody(); !body.empty()) {
        const render::Vector2 measured = renderer.measureText(style.messageFont, body, style.messageSize);
        renderer.drawText(style.messageFont, body, {x, centre.y - (measured.y / 2.0f)}, style.messageSize,
                          faded(style.message, opacity));
        x += measured.x + style.gap;
    }
    if (!toast.actor.empty()) {
        drawName(toast.actor, style.text, style.glow);
    }
    drawToastIcon(renderer, toast.type, {x + (style.iconSize / 2.0f), centre.y}, style.iconSize,
                  style.iconThickness, faded(accent, opacity));
}

void SceneRenderer::draw(render::Renderer &renderer, Layer layer)
{
    if (layer == Layer::World) {
        drawBackdrop(renderer);
        drawModels(renderer);
    }

    auto &drawables = _ecs.cache<Drawable>();
    auto &positions = _ecs.cache<ecs::Position>();
    auto &texts = _ecs.cache<Text>();
    auto &sprites = _ecs.cache<Sprite>();
    auto &boxes = _ecs.cache<Box>();
    auto &panels = _ecs.cache<Panel>();
    auto &scales = _ecs.cache<Scale>();
    auto &opacities = _ecs.cache<Opacity>();
    auto &focused = _ecs.cache<Focused>();
    auto &hitboxes = _ecs.cache<Hitbox>();
    auto &fields = _ecs.cache<TextField>();
    auto &toasts = _ecs.cache<Toast>();
    auto &titles = _ecs.cache<Title>();
    auto &sliders = _ecs.cache<Slider>();
    auto &tracks = _ecs.cache<SliderTrack>();
    auto &switches = _ecs.cache<SwitchTrack>();
    auto &toggles = _ecs.cache<Toggle>();
    auto &gauges = _ecs.cache<Gauge>();
    auto &rows = _ecs.cache<ListRow>();
    auto &avatars = _ecs.cache<Avatar>();
    auto &rings = _ecs.cache<Ring>();
    auto &settingRows = _ecs.cache<SettingRow>();
    auto &keyRows = _ecs.cache<KeyRow>();
    auto &sectionLabels = _ecs.cache<SectionLabel>();

    std::vector<ecs::entity_t> entities;
    for (ecs::entity_t entity = 0; entity < drawables.size(); ++entity) {
        if (drawables.has(entity) && drawables.get(entity).layer == layer && positions.has(entity) &&
            isShown(entity, layer)) {
            entities.push_back(entity);
        }
    }
    std::ranges::stable_sort(entities, {}, [&](ecs::entity_t entity) { return drawables.get(entity).order; });

    for (const ecs::entity_t entity : entities) {
        const ecs::Position centre = positions.get(entity);
        const float scale = scales.has(entity) ? scales.get(entity).value : 1.0f;
        const float opacity = opacities.has(entity) ? opacities.get(entity).value : 1.0f;
        const bool isFocused = focused.has(entity);
        const bool hasPanel = panels.has(entity);

        if (boxes.has(entity)) {
            const Box &box = boxes.get(entity);
            const render::Rectangle area{centre.x - box.width * scale / 2.0f,
                                         centre.y - box.height * scale / 2.0f, box.width * scale,
                                         box.height * scale};
            if (box.outline > 0.0f) {
                renderer.drawRectangleOutline(area, box.outline, faded(box.color, opacity));
            } else if (box.bottom) {
                renderer.drawGradientRectangle(area, faded(box.color, opacity), faded(*box.bottom, opacity));
            } else if (box.right) {
                renderer.drawRoundedRectangle(area, 0.0f, faded(box.color, opacity),
                                              faded(*box.right, opacity));
            } else {
                renderer.drawRectangle(area, faded(box.color, opacity));
            }
        }
        if (sprites.has(entity)) {
            const Sprite &sprite = sprites.get(entity);
            renderer.drawTexture(sprite.texture, sprite.source,
                                 {centre.x - sprite.width * scale / 2.0f,
                                  centre.y - sprite.height * scale / 2.0f, sprite.width * scale,
                                  sprite.height * scale},
                                 sprite.rotation, faded(sprite.tint, opacity));
        }
        if (rows.has(entity) && texts.has(entity)) {
            drawListRow(renderer, rows.get(entity), texts.get(entity),
                        hasPanel ? &panels.get(entity) : nullptr, {centre.x, centre.y}, opacity);
            continue;
        }
        if (avatars.has(entity)) {
            drawAvatar(renderer, avatars.get(entity), {centre.x, centre.y}, opacity);
            continue;
        }
        if (settingRows.has(entity) && texts.has(entity)) {
            drawSettingRow(renderer, settingRows.get(entity), texts.get(entity),
                           hasPanel ? &panels.get(entity) : nullptr, {centre.x, centre.y}, opacity);
            continue;
        }
        if (keyRows.has(entity) && texts.has(entity)) {
            drawKeyRow(renderer, keyRows.get(entity), texts.get(entity),
                       hasPanel ? &panels.get(entity) : nullptr, {centre.x, centre.y}, opacity);
            continue;
        }
        if (sectionLabels.has(entity) && texts.has(entity)) {
            drawSectionLabel(renderer, sectionLabels.get(entity), texts.get(entity), {centre.x, centre.y},
                             opacity);
            continue;
        }
        if (fields.has(entity) && fields.get(entity).type == TextFieldType::Line) {
            drawFormField(renderer, fields.get(entity), isFocused, {centre.x, centre.y}, opacity);
            continue;
        }
        if (fields.has(entity) && fields.get(entity).type == TextFieldType::Code) {
            drawCodeField(renderer, fields.get(entity), isFocused, {centre.x, centre.y}, opacity);
            continue;
        }
        if (hasPanel && texts.has(entity)) {
            const Panel &panel = panels.get(entity);
            drawFlare(renderer, panel.danger ? inDanger(_style.flare) : _style.flare, panel,
                      texts.get(entity), {centre.x, centre.y}, opacity);
            continue;
        }
        if (gauges.has(entity)) {
            drawGauge(renderer, gauges.get(entity), {centre.x, centre.y}, opacity);
            continue;
        }
        if (rings.has(entity)) {
            drawRing(renderer, rings.get(entity), {centre.x, centre.y}, opacity);
        }
        if (switches.has(entity) && toggles.has(entity) && texts.has(entity)) {
            drawSwitch(renderer, toggles.get(entity), switches.get(entity), texts.get(entity),
                       {centre.x, centre.y}, opacity);
            continue;
        }
        if (tracks.has(entity) && sliders.has(entity) && texts.has(entity)) {
            drawSlider(renderer, sliders.get(entity), tracks.get(entity), texts.get(entity),
                       {centre.x, centre.y}, opacity);
            continue;
        }
        if (titles.has(entity) && texts.has(entity)) {
            drawTitle(renderer, texts.get(entity), {centre.x, centre.y}, scale, opacity);
            continue;
        }
        if (toasts.has(entity)) {
            drawToast(renderer, toasts.get(entity), {centre.x, centre.y}, opacity);
        }
        const bool isConsoleField = fields.has(entity) && fields.get(entity).type == TextFieldType::Console;
        // A focused widget is framed and drawn in the focused colour, except buttons and console fields
        const bool showsFocus = isFocused && !hasPanel && !isConsoleField;
        if (showsFocus && hitboxes.has(entity)) {
            const Hitbox &box = hitboxes.get(entity);
            renderer.drawRectangleOutline(
                {centre.x - box.width / 2.0f, centre.y - box.height / 2.0f, box.width, box.height},
                _style.frameThickness, faded(_style.focused, opacity));
        }
        if (texts.has(entity)) {
            const Text &text = texts.get(entity);
            const float size = text.size * scale;
            const float spacing = text.spacing * scale;
            const render::Vector2 measured = renderer.measureText(text.font, text.text, size, spacing);
            const render::Vector2 topLeft{centre.x - measured.x / 2.0f, centre.y - measured.y / 2.0f};

            // A focused text field: the selection behind the text, the cursor in front of it. Their positions
            // are the widths of the text before them.
            const bool editing = isFocused && fields.has(entity);
            const auto offsetOf = [&](std::size_t byte) {
                const TextField &field = fields.get(entity);
                return topLeft.x +
                       renderer.measureText(text.font, fieldPrefix(field) + fieldContent(field, byte), size)
                           .x;
            };
            if (editing && !fields.get(entity).input.selection().empty()) {
                const input::TextInput::Range selection = fields.get(entity).input.selection();
                const float begin = offsetOf(selection.begin);
                renderer.drawRectangle({begin, topLeft.y, offsetOf(selection.end) - begin, measured.y},
                                       faded(_style.selection, opacity));
            }

            if (isConsoleField) {
                drawConsoleField(renderer, text, fields.get(entity), topLeft, size, opacity);
            } else {
                renderer.drawText(text.font, text.text, topLeft, size,
                                  faded(showsFocus ? _style.focused : text.color, opacity), spacing);
            }
            if (editing) {
                const float cursor = offsetOf(fields.get(entity).input.cursor());
                const float width = isConsoleField ? _style.console.cursorWidth : 3.0f;
                const render::Color color = isConsoleField ? _style.console.prompt : _style.focused;
                renderer.drawLine({cursor, topLeft.y}, {cursor, topLeft.y + measured.y}, width,
                                  faded(color, opacity));
            }
        }
    }
}

} // namespace rtype::engine
