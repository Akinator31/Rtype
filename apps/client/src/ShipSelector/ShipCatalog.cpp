#include "ShipSelector/ShipCatalog.hpp"

#include "Assets.hpp"

namespace rtype::client
{

const std::array<ShipEntry, shipCount> &getShipCatalog()
{
    static const std::array<ShipEntry, shipCount> catalog{{
        {"VX-01",
         "ship.vx01",
         RTYPE_MODEL_SHIP_CAVED,
         0.12f,
         {-12.49f, -2.21f, 25.0f, 5.51f},
         {3}},
        {"VX-02",
         "ship.vx02",
         RTYPE_MODEL_SHIP_EZNO,
         0.11f,
         {-8.66f, -3.31f, 17.31f, 10.01f},
         {6}},
        {"VX-03",
         "ship.vx03",
         RTYPE_MODEL_SHIP_BATTLESHIP,
         0.085f,
         {-12.5f, -3.61f, 25.0f, 6.8f},
         {2}},
        {"VX-04",
         "ship.vx04",
         RTYPE_MODEL_SHIP_SPACESHIP,
         0.08f,
         {-11.46f, -10.58f, 23.52f, 13.71f},
         {72}},
        {"VX-05",
         "ship.vx05",
         RTYPE_MODEL_SHIP_BATTLESTAR,
         0.085f,
         {-12.03f, -1.71f, 24.01f, 6.78f},
         {1}},
        {"VX-06",
         "ship.vx06",
         RTYPE_MODEL_SHIP_DROP_SHIP,
         0.085f,
         {-11.95f, -5.74f, 23.9f, 9.1f},
         {10, 12}},
    }};
    return catalog;
}

} // namespace rtype::client
