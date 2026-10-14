#include "rtype/engine/Rendering/SceneRenderer.hpp"

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/render/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
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

// Poses the model at the time of its animation, between the previous and the current one while it fades
void poseModel(render::Renderer &renderer, render::ModelId model, const ModelAnimation &animation)
{
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
        renderer.drawModel(model.model, camera->view, light, model.position, model.rotationAxis,
                           model.rotationAngle, model.scale, model.tint, model.spinAngle);
    }
}

void SceneRenderer::drawPanel(render::Renderer &renderer, const Panel &panel, render::Vector2 centre,
                              float scale, float opacity) const
{
    const ButtonStyle &style = _style.button;
    const PanelLook look = mix(style.normal, style.focused, panel.highlight);
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
            _scenes.isVisible(entity) && !ecs::isHidden(_ecs, entity)) {
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
        if (hasPanel) {
            drawPanel(renderer, panels.get(entity), {centre.x, centre.y}, scale, opacity);
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
