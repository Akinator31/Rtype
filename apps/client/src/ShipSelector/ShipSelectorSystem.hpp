#pragma once

#include "ShipSelector/ShipCarousel.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Control/Controls.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/input/InputSource.hpp"

namespace rtype::client
{

/// Runs the carousel of the hangar: Left and Right look at the previous and the next ship, and the carousel
/// glides to it, smoothly, with the ship in the middle big and bright, its neighbours smaller and dimmer on
/// each side; not while a row of the weapons has the focus, which Left and Right change instead. Dragging the
/// mouse turns the ship in the middle around its vertical axis. The dots of the pagination and the texts
/// follow the glide.
///
/// Once the ship of a game is picked (ShipSelection::departing), it takes off into the game, without a cut:
/// 1. the interface fades out, the ships on the sides slide away, the pedestal sinks;
/// 2. the ship turns to its profile with its big flame, draws back, then speeds off to the right, the way the
///    game goes, and levels off as the game shows it, with its little flame; the camera follows it;
/// 3. once the game has loaded, its sky shows behind the stars of the hangar, which fade out, and the light
///    becomes the one of the game; the camera pulls back while it narrows, until it frames the ship as the
///    game will open on it, without perspective (ArrivalZoom);
/// 4. the carousel calls ShipCarousel::onDeparted, which launches the game: the ship flies on, as long as it
///    takes the game to open (online, the master to answer), and the game opens on the same shot.
///
/// Register it before the AnchorSystem, which places the dots.
class ShipSelectorSystem : public ecs::System
{
    const engine::Scenes &_scenes;
    const engine::Controls &_controls;
    const input::InputSource &_input;
    const engine::Metrics &_metrics;
    ecs::ComponentCache<ShipCarousel> &_carousels;
    ecs::ComponentCache<engine::Model> &_models;
    ecs::ComponentCache<engine::ModelAnimation> &_animations;
    ecs::ComponentCache<engine::Camera> &_cameras;
    ecs::ComponentCache<engine::Light> &_lights;
    ecs::ComponentCache<engine::Backdrop> &_backdrops;
    ecs::ComponentCache<engine::Box> &_boxes;
    ecs::ComponentCache<engine::Opacity> &_opacities;
    ecs::ComponentCache<engine::Focused> &_focused;
    ecs::ComponentCache<engine::SettingRow> &_rows;

    /// A row of the weapons has the focus, in a scene that receives the input.
    [[nodiscard]] bool isRowFocused() const;
    void readControls(ecs::entity_t entity, const ShipCarousel &carousel);
    void turnShip(ecs::entity_t entity, ShipCarousel &carousel);
    static void glide(ShipSelection &selection, float elapsed);
    void placeShips(const ShipCarousel &carousel);
    void lightDots(const ShipCarousel &carousel);
    void fadeTexts(const ShipCarousel &carousel);
    /// Plays the take-off, once the ship is picked, over what placeShips() did.
    void depart(ShipCarousel &carousel, float elapsed);
    /// Slides the ships on the sides away and sinks the pedestal.
    void clearPad(const ShipCarousel &carousel, float time);
    void flyShip(ShipCarousel &carousel, float time, float elapsed);
    /// The view that follows the ship, framing it as the game will open, once the camera has caught up.
    [[nodiscard]] render::Camera computeArrivalView(const ShipCarousel &carousel, float time) const;
    void followShip(const ShipCarousel &carousel, float time);
    /// Shows the sky and the light of the game, once it has loaded.
    void openSky(ShipCarousel &carousel, float elapsed);
    /// Launches the game once the ship is seen as the game will open, and closes the hangar if the game does
    /// not open.
    void launch(ShipCarousel &carousel, float time, float elapsed);
    /// How opaque the interface is: 1 until the take-off, then down to 0.
    [[nodiscard]] static float getInterfaceOpacity(const ShipCarousel &carousel);

public:
    /// @param scenes, controls, input, metrics They must outlive the system.
    ShipSelectorSystem(ecs::ComponentCaches &caches, const engine::Scenes &scenes,
                       const engine::Controls &controls, const input::InputSource &input,
                       const engine::Metrics &metrics);

    void update(float elapsed) override;
};

} // namespace rtype::client
