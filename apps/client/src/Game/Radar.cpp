#include "Game/Radar.hpp"

#include <algorithm>

namespace rtype::client
{

namespace
{

constexpr render::Color Pilot = render::colors::White;
constexpr render::Color Ally{111, 227, 240, 255};
constexpr render::Color Bydo{232, 114, 110, 255};
constexpr render::Color Boss{255, 160, 64, 255};

constexpr float PilotSize = 16.0f;
constexpr float AllySize = 12.0f;
constexpr float BydoSize = 9.0f;
constexpr float BossSize = 28.0f;

} // namespace

std::optional<RadarBlip> radarBlip(game::EntityKind kind, std::uint8_t owner, std::uint8_t local,
                                   render::Vector2 position, render::Vector2 gameSize)
{
    RadarBlip blip;
    switch (kind) {
    case game::EntityKind::PlayerShip:
        blip.size = owner == local ? PilotSize : AllySize;
        blip.color = owner == local ? Pilot : Ally;
        break;
    case game::EntityKind::AllyShip:
        blip.size = AllySize;
        blip.color = Ally;
        break;
    case game::EntityKind::BydoFlyer:
    case game::EntityKind::BydoShooter:
    case game::EntityKind::BydoTank:
        blip.size = BydoSize;
        blip.color = Bydo;
        break;
    case game::EntityKind::BydoBoss:
        blip.size = BossSize;
        blip.color = Boss;
        blip.outlined = true;
        break;
    default:
        return std::nullopt;
    }
    if (gameSize.x <= 0.0f || gameSize.y <= 0.0f) {
        return std::nullopt;
    }
    // Kept inside the frame: an entity coming in is seen on its edge
    const float across = std::clamp(position.x / gameSize.x, 0.0f, 1.0f);
    const float down = std::clamp(position.y / gameSize.y, 0.0f, 1.0f);
    blip.centre = {RadarFrame.x + (across * RadarFrame.width), RadarFrame.y + (down * RadarFrame.height)};
    return blip;
}

} // namespace rtype::client
