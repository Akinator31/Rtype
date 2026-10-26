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
    /// Draws the 3D models, seen through the camera and lit by the light, then the glow of the energy models.
    void drawModels(render::Renderer &renderer);
    /// Poses the model at the time of its animation, unless it is already in this pose.
    void poseModel(render::Renderer &renderer, render::ModelId model, const ModelAnimation &animation);
    /// A button: its text and, while it is lit, the line of light across it and the star after it.
    void drawFlare(render::Renderer &renderer, const Panel &panel, const Text &text, render::Vector2 centre,
                   float opacity) const;
    void drawFlareLight(render::Renderer &renderer, const Panel &panel, render::Rectangle textArea,
                        render::Vector2 star, float opacity) const;
    void drawFlareLine(render::Renderer &renderer, float left, float right, float middle, float y, float head,
                       float shine, float opacity) const;
    /// Draws a title (see Title): its text in chrome, with a light around it.
    void drawTitle(render::Renderer &renderer, const Text &text, render::Vector2 centre, float scale,
                   float opacity) const;
    /// Draws a slider (see SliderTrack): its caption and value, its rail and its handle.
    void drawSlider(render::Renderer &renderer, const Slider &slider, const SliderTrack &track,
                    const Text &text, render::Vector2 centre, float opacity) const;
    /// Draws a switch (see SwitchTrack): its caption, then "OFF", a link with a dot on it, and "ON".
    void drawSwitch(render::Renderer &renderer, const Toggle &toggle, const SwitchTrack &track,
                    const Text &text, render::Vector2 centre, float opacity) const;
    /// Draws a gauge: its caption and value, its rail, and its light up to its value with a spark at the end.
    void drawGauge(render::Renderer &renderer, const Gauge &gauge, render::Vector2 centre,
                   float opacity) const;
    /// Draws an announcement: an icon and a title, a line of light under them, and the detail. `progress`
    /// goes from 0 to 1 while it appears, and back to 0 while it leaves: each part has its moment in this
    /// entrance.
    void drawAnnouncement(render::Renderer &renderer, const Toast &toast, render::Vector2 centre,
                          float progress) const;
    /// Draws a banner: a band across the screen between two lines of light, its title and its detail, with an
    /// entrance in the same way as an announcement.
    void drawBanner(render::Renderer &renderer, const Toast &toast, render::Vector2 centre,
                    float progress) const;
    /// Draws a line of text with its icon.
    void drawEventLine(render::Renderer &renderer, const Toast &toast, render::Vector2 centre,
                       float opacity) const;
    /// Draws a toast in the look of InterfaceStyle::toast: its words on a line, then its icon.
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
