#pragma once

#include "ShipSelector/ShipCarousel.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Control/Controls.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/input/InputSource.hpp"

namespace rtype::client
{

/// Runs the carousel of the hangar: Left and Right look at the previous and the next ship, and the carousel
/// glides to it, smoothly, with the ship in the middle big and bright, its neighbours smaller and dimmer on
/// each side. Dragging the mouse turns the ship in the middle around its vertical axis. The dots of the
/// pagination and the texts follow the glide.
///
/// Register it before the AnchorSystem, which places the dots.
class ShipSelectorSystem : public ecs::System
{
    const engine::Scenes &_scenes;
    const engine::Controls &_controls;
    const input::InputSource &_input;
    ecs::ComponentCache<ShipCarousel> &_carousels;
    ecs::ComponentCache<engine::Model> &_models;
    ecs::ComponentCache<engine::Box> &_boxes;
    ecs::ComponentCache<engine::Opacity> &_opacities;

    void readControls(ecs::entity_t entity, const ShipCarousel &carousel);
    void turnShip(ecs::entity_t entity, ShipCarousel &carousel);
    static void glide(ShipSelection &selection, float elapsed);
    void placeShips(const ShipCarousel &carousel);
    void lightDots(const ShipCarousel &carousel);
    void fadeTexts(const ShipCarousel &carousel);

public:
    /// @param scenes, controls, input They must outlive the system.
    ShipSelectorSystem(ecs::ComponentCaches &caches, const engine::Scenes &scenes,
                       const engine::Controls &controls, const input::InputSource &input);

    void update(float elapsed) override;
};

} // namespace rtype::client
