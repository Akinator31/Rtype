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

PanelLook mix(const PanelLook &from, const PanelLook &to, float amount)
{
    return {mix(from.left, to.left, amount), mix(from.right, to.right, amount),
            mix(from.border, to.border, amount), mix(from.glow, to.glow, amount)};
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
            renderer.drawText(font, text,
                              {topLeft.x + (std::cos(angle) * distance), topLeft.y + (std::sin(angle) * distance)},
                              size, layer, spacing);
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
    // One icon per type, in the order of ToastType: a warning and an error share theirs
    using Icon = void (*)(render::Renderer &, render::Vector2, float, float, render::Color);
    constexpr std::array<Icon, toastTypeCount> icons{drawInfoIcon, drawGainIcon, drawAlertIcon, drawAlertIcon};
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

// Poses the model at the time of its animation, between the previous and the current one while it fades, or
// mixed with its blended animation
void applyPose(render::Renderer &renderer, render::ModelId model, const ModelAnimation &animation)
{
    if (animation.time >= animation.fadeDuration && animation.blendAmount > 0.0f) {
        renderer.blendModelAnimations(model, animation.animation, animation.time, animation.blendedAnimation,
                                      animation.time, animation.blendAmount);
        return;
    }
    if (animation.time >= animation.fadeDuration) {
        renderer.animateModel(model, animation.animation, animation.time);
        return;
    }
    const float progress = animation.time / animation.fadeDuration;
    const float amount = progress * progress * (3.0f - (2.0f * progress));
    renderer.blendModelAnimations(model, animation.previousAnimation, animation.previousTime + animation.time,
                                  animation.animation, animation.time, amount);
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

render::Rectangle moveDown(render::Rectangle area, float distance)
{
    area.y += distance;
    return area;
}

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
    renderer.drawText(text.font, field.input.text(), {topLeft.x + promptWidth, topLeft.y}, size,
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

void SceneRenderer::drawBackdrop(render::Renderer &renderer)
{
    auto &backdrops = _ecs.cache<Backdrop>();
    for (ecs::entity_t entity = 0; entity < backdrops.size(); ++entity) {
        if (backdrops.has(entity) && isShown(entity)) {
            const Backdrop &backdrop = backdrops.get(entity);
            const render::Rectangle area = renderer.visibleArea();
            const render::Rectangle source = computeCoverSource(renderer.textureSize(backdrop.texture), area);
            renderer.drawTexture(backdrop.texture, source, area, 0.0f, backdrop.tint);
            return;
        }
    }
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
        }
    }
    // The light of the energy models spreads over the world, under the 2D entities and the interface
    renderer.drawGlow();
}

void SceneRenderer::drawPanel(render::Renderer &renderer, const Panel &panel, render::Vector2 centre,
                              float scale, float opacity) const
{
    const ButtonStyle &style = _style.button;
    const PanelLook look = mix(style.normal, style.focused, std::clamp(panel.highlight, 0.0f, 1.0f));
    const float width = panel.width * scale;
    const float height = panel.height * scale;
    const render::Rectangle area{centre.x - width / 2.0f, centre.y - height / 2.0f, width, height};
    const float radius = style.cornerRadius * scale;

    drawBlurred(renderer, moveDown(area, style.shadowOffset * scale), radius, style.shadowBlur * scale,
                faded(style.shadow, opacity));
    drawBlurred(renderer, moveDown(area, -style.topLightOffset * scale), radius, style.topLightBlur * scale,
                faded(style.topLight, opacity));
    drawBlurred(renderer, area, radius, style.glowSize * scale, faded(look.glow, opacity));
    renderer.drawRoundedRectangle(area, radius, faded(look.left, opacity), faded(look.right, opacity));
    renderer.drawRoundedRectangleOutline(area, radius, style.borderThickness * scale,
                                         faded(look.border, opacity));
}

/// The line fades in from its left end to the middle of the text, then stays bright up to the star; it is
/// drawn from its left end up to `head`, where it is being traced.
void SceneRenderer::drawFlareLine(render::Renderer &renderer, float left, float right, float middle, float y,
                                  float head, float shine, float opacity) const
{
    const FlareStyle &style = _style.flare;
    const render::Color bright = faded(brightened(style.line, shine), opacity);
    drawFadingLine(renderer, left, middle, y, style.lineThickness, clear(bright), bright, head);
    drawFadingLine(renderer, middle, right, y, style.lineThickness, bright, bright, head);
}

/// While the button lights up, the line runs from left to right with a bright head, and the star flares when
/// the line reaches it. When the button is pressed, the star bursts and turns, and a pulse of light runs from
/// it to the far end of the line.
void SceneRenderer::drawFlareLight(render::Renderer &renderer, const Panel &panel, render::Rectangle textArea,
                                   render::Vector2 star, float opacity) const
{
    const FlareStyle &style = _style.flare;
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
    drawFlareLine(renderer, left, right, textArea.x + (textArea.width / 2.0f), star.y, head, shine, opacity);
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

void SceneRenderer::drawFlare(render::Renderer &renderer, const Panel &panel, const Text &text,
                              render::Vector2 centre, float opacity) const
{
    const FlareStyle &style = _style.flare;
    const float trace = std::clamp(panel.highlight, 0.0f, 1.0f);
    const render::Vector2 measured =
        renderer.measureText(text.font, text.text, text.size, style.letterSpacing);
    const render::Vector2 topLeft{centre.x - (style.computeWidth(measured.x) / 2.0f) + style.padding,
                                  centre.y - (measured.y / 2.0f)};
    const render::Vector2 star{topLeft.x + measured.x + style.starGap + style.starSize, centre.y};
    if (trace > 0.0f) {
        drawFlareLight(renderer, panel, {topLeft.x, topLeft.y, measured.x, measured.y}, star, opacity);
    }
    renderer.drawText(text.font, text.text, topLeft, text.size,
                      faded(mix(style.text, style.litText, trace), opacity), style.letterSpacing);
}

void SceneRenderer::drawTitle(render::Renderer &renderer, const Text &text, render::Vector2 centre, float scale,
                              float opacity) const
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

void SceneRenderer::drawSlider(render::Renderer &renderer, const Slider &slider, const SliderTrack &track,
                               const Text &text, render::Vector2 centre, float opacity) const
{
    const SliderStyle &style = _style.slider;
    const float value = std::clamp(slider.get ? slider.get() : 0.0f, 0.0f, 1.0f);
    // The looks ease in and out, instead of changing at a constant speed
    const auto eased = [](float amount) {
        const float clamped = std::clamp(amount, 0.0f, 1.0f);
        return clamped * clamped * (3.0f - (2.0f * clamped));
    };
    const float lit = eased(track.hover);
    const float focus = eased(std::max(track.focus, track.hover));
    const float grab = eased(track.grab);


    // The caption on its line, then the rail under it, with room for the largest handle
    const float top = centre.y - (style.getHeight() / 2.0f);
    const float left = centre.x - (style.width / 2.0f);
    const float y = style.getRailY(centre.y);
    const float handleX = left + (value * style.width);

    const std::string percentage = std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%";
    const float percentageWidth =
        renderer.measureText(text.font, percentage, style.captionSize, style.letterSpacing).x;
    const render::Color caption = faded(mix(style.caption, style.litCaption, focus), opacity);
    renderer.drawText(text.font, slider.caption, {left, top}, style.captionSize, caption, style.letterSpacing);
    renderer.drawText(text.font, percentage, {left + style.width - percentageWidth, top}, style.captionSize,
                      faded(style.litCaption, opacity), style.letterSpacing);

    const render::Rectangle rail{left, y - (style.railThickness / 2.0f), style.width, style.railThickness};
    renderer.drawRoundedRectangle(rail, style.railThickness / 2.0f, faded(style.rail, opacity),
                                  faded(style.rail, opacity));
    const float filled = handleX - left;
    if (filled > 0.0f) {
        // Two fainter lines around the filled part give it its light, stronger when pointed at or dragged
        const render::Color light = faded(brightened(style.fillGlow, 1.0f + (1.5f * lit) + (1.0f * grab)), opacity);
        for (const float spread : {3.0f, 1.7f}) {
            const float thickness = style.fillThickness * spread;
            renderer.drawRoundedRectangle({left, y - (thickness / 2.0f), filled, thickness}, thickness / 2.0f,
                                          light, light);
        }
        const float thickness = style.fillThickness * (1.0f + (0.35f * lit));
        renderer.drawRoundedRectangle({left, y - (thickness / 2.0f), filled, thickness}, thickness / 2.0f,
                                      faded(style.fill, opacity), faded(style.fill, opacity));
    }

    // The handle grows when pointed at, and more when dragged, with a soft light and a faint ring around it
    const float radius = (style.handleRadius + ((style.handleHoverRadius - style.handleRadius) * lit)) +
                         ((style.handleDragRadius - style.handleHoverRadius) * grab);
    const render::Vector2 handleCentre{handleX, y};
    drawHalo(renderer, handleCentre, radius, style.handleGlowSpread * (0.4f + (0.6f * lit) + (0.4f * grab)),
             faded(brightened(style.handleGlow, lit + grab), opacity));
    if (lit > 0.0f) {
        render::Color ring = style.handleRing;
        ring.a = static_cast<std::uint8_t>(static_cast<float>(ring.a) * lit);
        renderer.drawCircleOutline(handleCentre, radius + style.handleRingGap + (3.0f * lit),
                                   style.handleThickness / 2.0f, faded(ring, opacity));
    }
    const render::Color handle = faded(style.handle, opacity);
    renderer.drawArc(handleCentre, radius, style.handleThickness, 180.0f + style.handleOpening,
                     360.0f - style.handleOpening, handle);
    renderer.drawArc(handleCentre, radius, style.handleThickness, style.handleOpening,
                     180.0f - style.handleOpening, handle);
    renderer.drawCircle(handleCentre, style.handleThickness * 0.7f, handle);
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
    const SwitchLayout layout = computeSwitchLayout(
        style, centre, [&](const char *label, float size, float spacing) {
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
    renderer.drawText(text.font, "OFF", offTop, style.labelSize, faded(offColor, opacity), style.labelSpacing);
    renderer.drawText(text.font, "ON", onTop, style.labelSize, faded(onColor, opacity), style.labelSpacing);

    // The link, lit between the OFF label and the dot once on, and from the dot to the ON label when pointed at
    renderer.drawRoundedRectangle({linkLeft, centre.y - (style.linkThickness / 2.0f), style.linkWidth,
                                   style.linkThickness},
                                  style.linkThickness / 2.0f, faded(style.link, opacity),
                                  faded(style.link, opacity));
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
    const float top = centre.y - (style.getHeight() / 2.0f);
    const float left = centre.x - (style.width / 2.0f);
    const float y = style.getRailY(centre.y);

    if (!gauge.caption.empty()) {
        const std::string percentage = std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%";
        drawCaption(renderer, style.font, gauge.caption, percentage, left, left + style.width, top,
                    style.captionSize, style.letterSpacing, faded(style.caption, opacity),
                    faded(style.litCaption, opacity));
    }
    renderer.drawRoundedRectangle({left, y - (style.railThickness / 2.0f), style.width, style.railThickness},
                                  style.railThickness / 2.0f, faded(style.rail, opacity),
                                  faded(style.rail, opacity));
    const float length = value * style.width;
    if (length <= 0.0f) {
        return;
    }
    // The light is transparent at the start of the gauge and white at its value; a full gauge shines more
    const float thickness = isFull ? style.fullLightThickness : style.lightThickness;
    const render::Rectangle light{left, y - (thickness / 2.0f), length, thickness};
    drawBlurred(renderer, light, thickness / 2.0f, style.glowSpread * (isFull ? 2.0f : 1.0f),
                faded(brightened(style.glow, isFull ? 1.8f : 1.0f), opacity));
    renderer.drawRoundedRectangle(light, thickness / 2.0f, faded(clear(style.light), opacity),
                                  faded(style.light, opacity));
    drawSpark(renderer, {left + length, y}, style.sparkLength * (isFull ? 1.3f : 1.0f), style.sparkHeight,
              faded(style.light, opacity));
}

void SceneRenderer::drawAnnouncement(render::Renderer &renderer, const Toast &toast, render::Vector2 centre,
                                     float progress) const
{
    const ToastStyle &style = _style.toast;
    const render::Color accent = style.getAccent(toast.type);
    render::Color glow = accent;
    glow.a = style.glow.a;

    // The entrance: the icon pops, the title fades in while its letters tighten, the line of light draws itself
    // from the middle with a flash, then the detail rises
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
    renderer.drawText(style.nameFont, title, titleTopLeft, style.announcementTitleSize, faded(accent, titleIn),
                      spacing);

    // The line of light under it, which shines while it is drawn, then the detail in grey
    const float lineY = top + style.announcementTitleSize + style.announcementGap +
                        (style.announcementLineThickness / 2.0f);
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
                      {centre.x - (detailWidth / 2.0f),
                       lineY + (style.announcementLineThickness / 2.0f) + style.announcementGap +
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
        renderer.drawRoundedRectangle({left, upper + 4.0f, half, lower - upper - 8.0f}, 0.0f, clear(veil), veil);
        renderer.drawRoundedRectangle({centre.x, upper + 4.0f, half, lower - upper - 8.0f}, 0.0f, veil,
                                      clear(veil));
    }
    render::Color lineColor = accent;
    lineColor.a = 178;
    render::Color lineGlow = accent;
    lineGlow.a = 90;
    for (const float y : {upper + (style.bannerLineThickness / 2.0f), lower - (style.bannerLineThickness / 2.0f)}) {
        drawLightLine(renderer, left, right, y, style.bannerLineThickness,
                      brightened(lineColor, 1.0f + (0.9f * openFlash)),
                      brightened(lineGlow, 1.0f + (2.0f * openFlash)), 8.0f);
    }

    // The title, with a spark on each side, then the detail under it. The letters are spaced wide at first, then
    // tighten; the sparks start from the middle and fly to their places
    const std::string &title = toast.getHeadline();
    const float titleSpacing = style.bannerTitleSpacing * (1.0f + ((style.spacingIn - 1.0f) * (1.0f - titleIn)));
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
    constexpr std::array<Draw, toastKindCount> draws{&SceneRenderer::drawEventLine,
                                                     &SceneRenderer::drawAnnouncement,
                                                     &SceneRenderer::drawBanner};
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
                                 0.0f, faded(sprite.tint, opacity));
        }
        const bool isFlare = hasPanel && panels.get(entity).type == PanelType::Flare;
        if (hasPanel && !isFlare) {
            drawPanel(renderer, panels.get(entity), {centre.x, centre.y}, scale, opacity);
        }
        if (isFlare && texts.has(entity)) {
            drawFlare(renderer, panels.get(entity), texts.get(entity), {centre.x, centre.y}, opacity);
            continue;
        }
        if (gauges.has(entity)) {
            drawGauge(renderer, gauges.get(entity), {centre.x, centre.y}, opacity);
            continue;
        }
        if (switches.has(entity) && toggles.has(entity) && texts.has(entity)) {
            drawSwitch(renderer, toggles.get(entity), switches.get(entity), texts.get(entity),
                       {centre.x, centre.y}, opacity);
            continue;
        }
        if (tracks.has(entity) && sliders.has(entity) && texts.has(entity)) {
            drawSlider(renderer, sliders.get(entity), tracks.get(entity), texts.get(entity), {centre.x, centre.y},
                       opacity);
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
            const render::Vector2 measured = renderer.measureText(text.font, text.text, size);
            const render::Vector2 topLeft{centre.x - measured.x / 2.0f, centre.y - measured.y / 2.0f};

            // A focused text field: the selection behind the text, the cursor in front of it. Their positions
            // are the widths of the text before them.
            const bool editing = isFocused && fields.has(entity);
            const auto offsetOf = [&](std::size_t byte) {
                const TextField &field = fields.get(entity);
                return topLeft.x + renderer
                                       .measureText(text.font,
                                                    fieldPrefix(field) + field.input.text().substr(0, byte),
                                                    size)
                                       .x;
            };
            if (editing && !fields.get(entity).input.selection().empty()) {
                const input::TextInput::Range selection = fields.get(entity).input.selection();
                const float begin = offsetOf(selection.begin);
                renderer.drawRectangle({begin, topLeft.y, offsetOf(selection.end) - begin, measured.y},
                                       faded(_style.selection, opacity));
            }
            if (hasPanel) {
                renderer.drawText(text.font, text.text,
                                  {topLeft.x, topLeft.y + _style.button.textShadowOffset * scale}, size,
                                  faded(_style.button.textShadow, opacity));
            }
            if (isConsoleField) {
                drawConsoleField(renderer, text, fields.get(entity), topLeft, size, opacity);
            } else {
                renderer.drawText(text.font, text.text, topLeft, size,
                                  faded(showsFocus ? _style.focused : text.color, opacity));
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
