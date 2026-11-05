#include "Game/EntityLook.hpp"

#include "rtype/game/Rules.hpp"

#include <algorithm>
#include <array>

namespace rtype::client
{

namespace
{

/// The sizes of the rules, so that what is drawn is what collides.
const game::Rules DefaultRules{};

/// Drawn above the background, the shots under the ships.
constexpr int ShotOrder = 10;
constexpr int ShipOrder = 20;

/// How a kind of entity is drawn. The snapshots do not carry the sizes of the shots and of the Bydos: those
/// of the configuration of the repository (config/ships/, config/projectiles/), which a ShipLook fits its
/// model to.
struct KindLook {
    game::EntityKind kind;
    float width;
    float height;
    /// Drawn in the colour of its player rather than in `color`.
    bool playerColored;
    render::Color color;
    int order;
};

const std::array<KindLook, 8> KindLooks{{
    {game::EntityKind::PlayerShip,
     DefaultRules.shipHalfWidth * 2.0f,
     DefaultRules.shipHalfHeight * 2.0f,
     true,
     {},
     ShipOrder},
    {game::EntityKind::PlayerShot, 68.0f, 18.8f, true, {}, ShotOrder},
    {game::EntityKind::PlayerChargedShot, 48.0f, 16.0f, true, {}, ShotOrder},
    // The allies of the rescue against the final boss: the size of a ship of a player, in silver
    {game::EntityKind::AllyShip,
     DefaultRules.shipHalfWidth * 2.0f,
     DefaultRules.shipHalfHeight * 2.0f,
     false,
     {200, 210, 225, 255},
     ShipOrder},
    {game::EntityKind::BydoFlyer, 140.0f, 54.0f, false, {171, 71, 188, 255}, ShipOrder},
    {game::EntityKind::BydoBoss, 160.0f, 316.0f, false, {171, 71, 188, 255}, ShipOrder},
    {game::EntityKind::BydoShot, 68.0f, 22.0f, false, {255, 112, 67, 255}, ShotOrder},
    {game::EntityKind::BydoPlasmaShot, 44.0f, 44.0f, false, {236, 64, 122, 255}, ShotOrder},
}};

/// The other kinds come with the gameplay: a grey box until they have a look.
const KindLook UnknownLook{game::EntityKind::BydoTank, 48.0f, 48.0f, false, render::colors::Gray, ShipOrder};

} // namespace

render::Color playerColor(std::uint8_t slot)
{
    static constexpr std::array<render::Color, 4> Colors = {{
        {79, 195, 247, 255},  // blue
        {239, 83, 80, 255},   // red
        {102, 187, 106, 255}, // green
        {255, 202, 40, 255},  // yellow
    }};
    return slot < Colors.size() ? Colors[slot] : render::colors::White;
}

EntityLook lookOf(const game::EntityState &entity)
{
    const auto found = std::ranges::find(KindLooks, entity.kind, &KindLook::kind);
    const KindLook &look = found != KindLooks.end() ? *found : UnknownLook;
    const render::Color color = look.playerColored ? playerColor(entity.owner) : look.color;
    return {{look.width, look.height, color}, look.order};
}

} // namespace rtype::client
