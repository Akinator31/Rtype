#include "rtype/engine/Rendering/SceneRenderer.hpp"

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/render/Renderer.hpp"

#include <algorithm>
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

void SceneRenderer::drawToast(render::Renderer &renderer, const Toast &toast, render::Vector2 centre,
                              float opacity) const
{
    const ToastStyle &style = _style.toast;
    const float height = style.height();
    const render::Rectangle area{centre.x - toast.width / 2.0f, centre.y - height / 2.0f, toast.width,
                                 height};
    const render::Color background = faded(style.background, opacity);

    drawBlurred(renderer, moveDown(area, style.shadowOffset), style.cornerRadius, style.shadowBlur,
                faded(style.shadow, opacity));
    renderer.drawRoundedRectangle(area, style.cornerRadius, background, background);
    renderer.drawRoundedRectangleOutline(area, style.cornerRadius, style.borderThickness,
                                         faded(style.border, opacity));

    const render::Color accent = faded(style.getAccent(toast.type), opacity);
    const render::Rectangle accentArea{area.x + style.padding, area.y + style.padding, style.accentWidth,
                                       height - (2.0f * style.padding)};
    renderer.drawRoundedRectangle(accentArea, style.accentWidth / 2.0f, accent, accent);

    const render::Vector2 measured = renderer.measureText(_style.button.font, toast.text, style.textSize);
    const render::Vector2 topLeft{accentArea.x + style.accentWidth + style.padding,
                                  centre.y - measured.y / 2.0f};
    renderer.drawText(_style.button.font, toast.text, topLeft, style.textSize, faded(style.text, opacity));
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
