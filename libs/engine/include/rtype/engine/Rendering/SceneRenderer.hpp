#pragma once

#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Toast.hpp"
#include "rtype/engine/Component/Widget.hpp"
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

/// Draws the entities with the Renderer: those with a Drawable, a Position and a Text, Sprite, Box, Panel or
/// Toast, and the 3D models (Drawable and Model) seen through the Camera, before the rest of the World layer.
/// A Backdrop is drawn first, behind the models.
///
/// It is not a system of the Ecs: drawing happens between Renderer::beginFrame() and endFrame(), after the
/// update, and may happen several times per frame (once per eye in VR). Each layer is drawn separately;
/// within a layer, entities are drawn by Drawable order, then by id.
///
/// Hidden entities (see ecs::isHidden()) and entities of hidden scenes are skipped, and so are the entities
/// of the interface layer under a scene that hides the interface below (SceneSettings::hidesInterfaceBelow).
/// The focused widget gets a frame around its Hitbox and the focused colour, unless it has a Panel, which
/// shows the focus itself; a focused text field also shows its selection and cursor.
/// @code
/// ecs.update(elapsed);
/// renderer.beginFrame();
/// sceneRenderer.draw(renderer, Layer::World);
/// sceneRenderer.draw(renderer, Layer::Interface);
/// renderer.endFrame();
/// @endcode
class SceneRenderer
{
    /// The pose an animated model was last given.
    struct Pose {
        render::ModelId model;
        ModelAnimation animation;
    };

    ecs::Ecs &_ecs;
    Scenes &_scenes;
    const InterfaceStyle &_style;
    /// Posing a skinned model costs milliseconds: a model is posed again only when its pose changes, so that
    /// entities sharing a model and an animation, at the same time, are posed once.
    std::vector<Pose> _poses;

    /// Whether the entity is in a visible scene and not hidden.
    [[nodiscard]] bool isShown(ecs::entity_t entity);
    /// Whether the entity is drawn in this layer: in the interface layer, its scene must show its interface.
    [[nodiscard]] bool isShown(ecs::entity_t entity, Layer layer);
    /// The first shown Camera, or nullptr if there is none.
    [[nodiscard]] const Camera *findCamera();
    /// The light of the scene: the first shown Light that is not on a Model, or the default light if there is
    /// none.
    [[nodiscard]] render::Light findLight();
    /// The shown entities with a Model, by Drawable order.
    [[nodiscard]] std::vector<ecs::entity_t> getModelsInOrder();
    /// Draws the first shown Backdrop over the whole visible area.
    void drawBackdrop(render::Renderer &renderer);
    /// Draws the 3D models, seen through the camera and lit by the light.
    void drawModels(render::Renderer &renderer);
    /// Poses the model at the time of its animation, unless it is already in this pose.
    void poseModel(render::Renderer &renderer, render::ModelId model, const ModelAnimation &animation);
    /// Draws a glass panel with its shadows and glow, in the look of its highlight.
    void drawPanel(render::Renderer &renderer, const Panel &panel, render::Vector2 centre, float scale,
                   float opacity) const;
    /// Draws a toast in the look of InterfaceStyle::toast: its panel, its accent and its text.
    void drawToast(render::Renderer &renderer, const Toast &toast, render::Vector2 centre,
                   float opacity) const;
    /// Draws a console text field: its prompt in the prompt colour of the style, then its content.
    void drawConsoleField(render::Renderer &renderer, const Text &text, const TextField &field,
                          render::Vector2 topLeft, float size, float opacity) const;

public:
    /// @param ecs, scenes, style They must outlive the renderer.
    SceneRenderer(ecs::Ecs &ecs, Scenes &scenes, const InterfaceStyle &style);

    /// Draws the entities of one layer.
    void draw(render::Renderer &renderer, Layer layer);
    /// Draws the world, then the interface on top of it.
    void draw(render::Renderer &renderer);
};

} // namespace rtype::engine
