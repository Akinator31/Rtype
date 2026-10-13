#include "rtype/engine/Rendering/SceneRenderer.hpp"

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/render/Renderer.hpp"

#include <algorithm>
#include <cstdint>
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

} // namespace

SceneRenderer::SceneRenderer(ecs::Ecs &ecs, Scenes &scenes, const InterfaceStyle &style)
    : _ecs(ecs), _scenes(scenes), _style(style)
{
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
    for (ecs::entity_t entity = 0; entity < lights.size(); ++entity) {
        if (lights.has(entity) && isShown(entity)) {
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

void SceneRenderer::drawModels(render::Renderer &renderer)
{
    const Camera *camera = findCamera();
    if (camera == nullptr) {
        return;
    }
    const render::Light light = findLight();
    auto &models = _ecs.cache<Model>();

    for (const ecs::entity_t entity : getModelsInOrder()) {
        const Model &model = models.get(entity);
        renderer.drawModel(model.model, camera->view, light, model.position, model.rotationAxis,
                           model.rotationAngle, model.scale, model.tint);
    }
}

void SceneRenderer::draw(render::Renderer &renderer, Layer layer)
{
    if (layer == Layer::World) {
        drawModels(renderer);
    }

    auto &drawables = _ecs.cache<Drawable>();
    auto &positions = _ecs.cache<ecs::Position>();
    auto &texts = _ecs.cache<Text>();
    auto &sprites = _ecs.cache<Sprite>();
    auto &boxes = _ecs.cache<Box>();
    auto &scales = _ecs.cache<Scale>();
    auto &opacities = _ecs.cache<Opacity>();
    auto &focused = _ecs.cache<Focused>();
    auto &hitboxes = _ecs.cache<Hitbox>();
    auto &fields = _ecs.cache<TextField>();

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
        if (isFocused && hitboxes.has(entity)) {
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
            renderer.drawText(text.font, text.text, topLeft, size,
                              faded(isFocused ? _style.focused : text.color, opacity));
            if (editing) {
                const float cursor = offsetOf(fields.get(entity).input.cursor());
                renderer.drawLine({cursor, topLeft.y}, {cursor, topLeft.y + measured.y}, 3.0f,
                                  faded(_style.focused, opacity));
            }
        }
    }
}

} // namespace rtype::engine
