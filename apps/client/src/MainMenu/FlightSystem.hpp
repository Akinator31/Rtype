#pragma once

#include "MainMenu/Flight.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"

#include <random>

namespace rtype::client
{

/// Flies the ships of the menu: moves each entity with a Flight and a Model along the route of its stage, and
/// starts the next stage when one ends. The ships arrive from under the screen, from its right edge or from
/// behind the camera. Each new trip takes a random route and lasts a random time, so that the ships do not
/// land and leave all together. A ship that leaves towards the camera turns back and passes under it. While
/// it is out of the screen, a ship becomes another of its models, one that no other ship flies.
class FlightSystem : public ecs::System
{
    ecs::ComponentCache<Flight> &_flights;
    ecs::ComponentCache<engine::Model> &_models;
    ecs::ComponentCache<engine::ModelAnimation> &_animations;
    std::mt19937 _random;

    [[nodiscard]] float pickBetween(float low, float high);
    [[nodiscard]] render::Vector3 pickPointBelowScreen();
    [[nodiscard]] render::Vector3 pickPointAboveScreen();
    [[nodiscard]] render::Vector3 pickPointBehindCamera();
    void startNextStage(Flight &flight);
    void startApproach(Flight &flight);
    void startDeparture(Flight &flight);
    [[nodiscard]] bool isFlownByAnotherShip(ecs::entity_t ship, render::ModelId model) const;
    void changeShipModel(ecs::entity_t ship, Flight &flight);

public:
    explicit FlightSystem(ecs::ComponentCaches &caches);

    void update(float elapsed) override;
};

} // namespace rtype::client
