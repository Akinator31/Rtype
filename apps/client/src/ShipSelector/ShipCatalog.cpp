#include "ShipSelector/ShipCatalog.hpp"

#include "Assets.hpp"

namespace rtype::client
{

const std::array<ShipEntry, shipCount> &getShipCatalog()
{
    static const std::array<ShipEntry, shipCount> catalog{{
        {"VX-01", "ship.vx01", RTYPE_MODEL_SHIP_CAVED, 0.12f, 0.90f, 0.30f, 0.25f, false},
        {"VX-02", "ship.vx02", RTYPE_MODEL_SHIP_EZNO, 0.11f, 0.70f, 0.55f, 0.35f, false},
        {"VX-03", "ship.vx03", RTYPE_MODEL_SHIP_BATTLESHIP, 0.085f, 0.30f, 0.60f, 0.95f, false},
        {"VX-04", "ship.vx04", RTYPE_MODEL_SHIP_SPACESHIP, 0.08f, 0.80f, 0.70f, 0.50f, false},
        {"VX-05", "ship.vx05", RTYPE_MODEL_SHIP_BATTLESTAR, 0.085f, 0.60f, 0.85f, 0.60f, true},
        {"VX-06", "ship.vx06", RTYPE_MODEL_SHIP_DROP_SHIP, 0.085f, 0.25f, 0.90f, 0.80f, false},
    }};
    return catalog;
}

} // namespace rtype::client
