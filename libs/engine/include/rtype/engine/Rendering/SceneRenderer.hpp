#pragma once

#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/engine/Scene/Scenes.hpp"

#include <vector>

namespace rtype::ecs
{
class Ecs;
} // namespace rtype::ecs

namespace rtype::render
{
class Renderer;
} // namespace rtype::render

namespace rtype::engine
{

/// Draws the entities with the Renderer: those with a Drawable, a Position and a Text, Sprite, Box or Panel,
/// and the 3D models (Drawable and Model) seen through the Camera, before the rest of the World layer.
///
/// It is not a system of the Ecs: drawing happens between Renderer::beginFrame() and endFrame(), after the
/// update, and may happen several times per frame (once per eye in VR). Each layer is drawn separately;
/// within a layer, entities are drawn by Drawable order, then by id.
///
/// Hidden entities (see ecs::isHidden()) and entities of hidden scenes are skipped. The focused widget gets a
/// frame around its Hitbox and the focused colour, unless it has a Panel, which shows the focus itself; a
/// focused text field also shows its selection and cursor.
/// @code
/// ecs.update(elapsed);
/// renderer.beginFrame();
/// sceneRenderer.draw(renderer, Layer::World);
/// sceneRenderer.draw(renderer, Layer::Interface);
/// renderer.endFrame();
/// @endcode
class SceneRenderer
{
    ecs::Ecs &_ecs;
    Scenes &_scenes;
    const InterfaceStyle &_style;

    /// Whether the entity is in a visible scene and not hidden.
    [[nodiscard]] bool isShown(ecs::entity_t entity);
    /// The first shown Camera, or nullptr if there is none.
    [[nodiscard]] const Camera *findCamera();
    /// The first shown Light, or the default light if there is none.
    [[nodiscard]] render::Light findLight();
    /// The shown entities with a Model, by Drawable order.
    [[nodiscard]] std::vector<ecs::entity_t> getModelsInOrder();
    /// Draws the 3D models, seen through the camera and lit by the light.
    void drawModels(render::Renderer &renderer);
    /// Draws a glass panel with its shadows and glow, in the look of its highlight.
    void drawPanel(render::Renderer &renderer, const Panel &panel, render::Vector2 centre, float scale,
                   float opacity) const;

public:
    /// @param ecs, scenes, style They must outlive the renderer.
    SceneRenderer(ecs::Ecs &ecs, Scenes &scenes, const InterfaceStyle &style);

    /// Draws the entities of one layer.
    void draw(render::Renderer &renderer, Layer layer);
    /// Draws the world, then the interface on top of it.
    void draw(render::Renderer &renderer);
};

} // namespace rtype::engine
