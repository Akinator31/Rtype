#pragma once

#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Toast.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/engine/Scene/Scenes.hpp"

#include <optional>
#include <string>
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

/// Draws the entities with the Renderer: those with a Drawable, a Position and a Text, Sprite, Box, Panel,
/// Ring or Toast, and the 3D models (Drawable and Model) seen through the Camera, before the rest of the
/// World layer. The sky (Backdrop, BackdropSprite) is drawn first, behind the models.
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
    /// The first shown Backdrop, whose scene is the sky, or nothing if no Backdrop is shown.
    [[nodiscard]] std::optional<ecs::entity_t> findFirstBackdrop();
    /// How far the entity has scrolled: the offset of its Scroll, or 0 without one.
    [[nodiscard]] float getScrollOffset(ecs::entity_t entity);
    /// Draws the sky behind the 3D models: the Backdrops and BackdropSprites of its scene, by order.
    void drawBackdrop(render::Renderer &renderer);
    /// Covers the visible area with the picture, slid to the left by the offset and repeated.
    void drawBackdropLayer(render::Renderer &renderer, const Backdrop &backdrop, float offset);
    /// Draws the picture where its pass through the visible area has brought it.
    void drawBackdropSprite(render::Renderer &renderer, const BackdropSprite &sprite, float offset);
    /// Draws the 3D models, seen through the camera and lit by the light, then the glow of the energy models.
    void drawModels(render::Renderer &renderer);
    /// Poses the model at the time of its animation, unless it is already in this pose.
    void poseModel(render::Renderer &renderer, render::ModelId model, const ModelAnimation &animation);
    /// A button: its text and, while it is lit, the line of light across it and the star after it.
    /// @param style FlareStyle of the style, or its copy in the red of the alerts for a Panel::danger.
    void drawFlare(render::Renderer &renderer, const FlareStyle &style, const Panel &panel, const Text &text,
                   render::Vector2 centre, float opacity) const;
    void drawFlareLight(render::Renderer &renderer, const FlareStyle &style, const Panel &panel,
                        render::Rectangle textArea, render::Vector2 star, float opacity) const;
    void drawFlareLine(render::Renderer &renderer, const FlareStyle &style, float left, float right,
                       float middle, float y, float head, float shine, float opacity) const;
    /// Draws a field of a form (TextFieldType::Line): its caption, its value with the selection and the
    /// cursor while it has the focus, its rule lit by its light, and its error.
    void drawFormField(render::Renderer &renderer, const TextField &field, bool focused,
                       render::Vector2 centre, float opacity) const;
    /// Draws a code (TextFieldType::Code): its caption, a cell per digit with its rule, the cell being typed
    /// lit while the field has the focus, and its error.
    void drawCodeField(render::Renderer &renderer, const TextField &field, bool focused,
                       render::Vector2 centre, float opacity) const;
    /// Draws a row of a list: its avatar, its title and detail, its side text, and its rule, lit as the line
    /// of a button by its Panel, if it has one; dim without.
    void drawListRow(render::Renderer &renderer, const ListRow &row, const Text &text, const Panel *panel,
                     render::Vector2 centre, float opacity) const;
    /// Draws an avatar: its disc, its picture or its initial, and its ring.
    void drawAvatar(render::Renderer &renderer, const Avatar &avatar, render::Vector2 centre,
                    float opacity) const;
    /// Draws a title (see Title): its text in chrome, with a light around it.
    void drawTitle(render::Renderer &renderer, const Text &text, render::Vector2 centre, float scale,
                   float opacity) const;
    /// Draws what the rows of a page of settings share in `area`: the rule under the row, and while it is lit
    /// (`light` from 0 to 1), the veil from its left, the mark on its left edge and the line of light along
    /// its bottom.
    void drawRowFrame(render::Renderer &renderer, render::Rectangle area, float light, float opacity) const;
    /// Draws a row of a page of settings: its frame, its caption, its value and the chevrons of a choice, or
    /// the chevron of a row that opens something.
    void drawSettingRow(render::Renderer &renderer, const SettingRow &row, const Text &text,
                        const Panel *panel, render::Vector2 centre, float opacity) const;
    /// Draws a row of the controls: its frame, its action, its keys and its gamepad buttons.
    void drawKeyRow(render::Renderer &renderer, const KeyRow &row, const Text &text, const Panel *panel,
                    render::Vector2 centre, float opacity) const;
    /// Draws a key in its frame, its left side at `left`, in the look of its state.
    /// @return Its width.
    float drawKeyCap(render::Renderer &renderer, const KeyCap &key, float left, float middle,
                     float opacity) const;
    /// Draws a button of the gamepad in its pill, its left side at `left`.
    /// @return Its width.
    float drawPadButton(render::Renderer &renderer, const std::string &label, float left, float middle,
                        float opacity) const;
    /// Draws the name of a group of rows and its rule.
    void drawSectionLabel(render::Renderer &renderer, const SectionLabel &label, const Text &text,
                          render::Vector2 centre, float opacity) const;
    /// Draws a slider (see SliderTrack): a row of a page of settings, its caption and value, its rail and its
    /// handle.
    void drawSlider(render::Renderer &renderer, const Slider &slider, const SliderTrack &track,
                    const Text &text, render::Vector2 centre, float opacity) const;
    /// Draws a switch (see SwitchTrack): its caption, then "OFF", a link with a dot on it, and "ON".
    void drawSwitch(render::Renderer &renderer, const Toggle &toggle, const SwitchTrack &track,
                    const Text &text, render::Vector2 centre, float opacity) const;
    /// Draws a gauge: its caption and value, its rail, and its light up to its value with a spark at the end.
    void drawGauge(render::Renderer &renderer, const Gauge &gauge, render::Vector2 centre,
                   float opacity) const;
    /// Draws a ring: its dark track, then its light up to its value, with a glow and a spark at its end.
    void drawRing(render::Renderer &renderer, const Ring &ring, render::Vector2 centre, float opacity) const;
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

    /// The light of the scenes shown, which lights their 3D models: the first shown Light that is not on a
    /// Model, or the default light. In VR, it lights the cockpit around the board too.
    [[nodiscard]] render::Light light();
    /// The picture at the back of the sky of the scenes shown: the Backdrop of the lowest order in the scene
    /// of the first shown Backdrop, or nothing without one. In VR, the sky drawn around the cockpit.
    [[nodiscard]] std::optional<Backdrop> sky();
};

} // namespace rtype::engine
