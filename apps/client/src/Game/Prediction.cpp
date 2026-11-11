#include "Game/Prediction.hpp"

#include "rtype/game/ShipMovement.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace rtype::client
{

namespace
{

/// A weapon as the game equips it: ready, its magazine full.
game::Armed equipped(const game::WeaponDefinition &weapon)
{
    return {weapon.id, 0, weapon.magazineSize, 0};
}

} // namespace

void Prediction::arm(game::WeaponDefinition weapon, float projectileHalfWidth, std::uint8_t slot)
{
    arm(std::vector<Weapon>{{std::move(weapon), projectileHalfWidth, game::NoShip}}, slot);
}

void Prediction::arm(std::vector<Weapon> weapons, std::uint8_t slot)
{
    _weapons = std::move(weapons);
    _weapons.resize(std::min(_weapons.size(), game::LoadoutSize));
    _slot = slot;
    if (_predicted && !_armed) {
        equip();
    }
}

void Prediction::equip()
{
    _armed.reset();
    _holster.reset();
    if (_weapons.empty()) {
        return;
    }
    _armed = equipped(_weapons[0].definition);
    if (_weapons.size() == game::LoadoutSize) {
        _holster = game::Holster{equipped(_weapons[1].definition), 0};
    }
}

std::optional<PredictedShot> Prediction::step(ecs::Position &position, std::optional<game::Armed> &armed,
                                              std::optional<game::Holster> &holster,
                                              game::PlayerInput input) const
{
    std::optional<PredictedShot> shot;
    // As the game: the weapon asked for is taken in hand, fires from where the ship was, then the ship moves
    // (WeaponFiring.hpp)
    if (!_weapons.empty() && armed) {
        if (holster) {
            game::selectWeapon(*armed, *holster, input.weaponInHand());
        }
        const Weapon &weapon = _weapons[holster ? holster->inHand : 0];
        if (game::stepWeapon(*armed, weapon.definition, input.isHeld(game::PlayerInput::Fire))) {
            shot = PredictedShot{game::playerShot(weapon.definition, weapon.projectileHalfWidth, _slot,
                                                  position, input.aim(), _rules),
                                 weapon.definition.mechanic, weapon.projectile};
        }
    }
    position = game::moveShip(position, input, _rules);
    return shot;
}

void Prediction::apply(std::uint32_t tick, game::PlayerInput input)
{
    // The ticks skipped since the previous input: the game repeats the previous input for them
    if (!_sent.empty() && tick > _sent.back().tick + 1) {
        const game::PlayerInput previous = _sent.back().input;
        const std::uint32_t first = std::max(_sent.back().tick + 1, tick - std::min(tick, MaxFilled));
        for (std::uint32_t skipped = first; skipped < tick; ++skipped) {
            _sent.push_back({skipped, previous});
            if (_predicted) {
                if (const auto shot = step(*_predicted, _armed, _holster, previous)) {
                    _shots[skipped] = *shot;
                }
            }
        }
    }
    if (!_sent.empty() && tick <= _sent.back().tick) {
        // Never twice the same tick: the stamps only go forward
        return;
    }
    _sent.push_back({tick, input});
    if (_predicted) {
        if (const auto shot = step(*_predicted, _armed, _holster, input)) {
            _shots[tick] = *shot;
        }
    }
    while (_sent.size() > MaxInputs) {
        _sent.pop_front();
    }
}

void Prediction::reconcile(std::optional<ecs::Position> ship, std::uint32_t tick,
                           std::optional<game::WeaponState> weapon)
{
    // Applied by the game: no longer to replay
    while (!_sent.empty() && _sent.front().tick <= tick) {
        _sent.pop_front();
    }
    if (!ship) {
        // Destroyed, or not back yet: the game decides
        _predicted.reset();
        _armed.reset();
        _holster.reset();
        _shots.clear();
        _offsetX = 0.0f;
        _offsetY = 0.0f;
        return;
    }

    // From where the game had it, and its weapons, the inputs it has not applied yet
    ecs::Position replayed = *ship;
    const bool rewound = !_weapons.empty() && weapon;
    std::optional<game::Armed> armed;
    std::optional<game::Holster> holster;
    if (rewound) {
        const bool two = _weapons.size() == game::LoadoutSize;
        const std::uint8_t hand = two && weapon->inHand == 1 ? 1 : 0;
        armed = game::Armed{_weapons[hand].definition.id, weapon->cooldown, weapon->ammo, weapon->reloadLeft};
        if (two) {
            holster = game::Holster{game::Armed{_weapons[1 - hand].definition.id, weapon->spareCooldown,
                                                weapon->spareAmmo, weapon->spareReloadLeft},
                                    hand};
        }
    }
    std::map<std::uint32_t, PredictedShot> shots;
    for (const Sent &sent : _sent) {
        if (const auto shot = step(replayed, armed, holster, sent.input)) {
            shots[sent.tick] = *shot;
        }
    }
    if (rewound) {
        _armed = armed;
        _holster = holster;
        _shots = std::move(shots);
    } else {
        // Without the weapon of the snapshot: the ticks up to it are the game's
        std::erase_if(_shots, [tick](const auto &entry) { return entry.first <= tick; });
        if (!_weapons.empty() && !_armed) {
            equip();
        }
    }

    if (_predicted) {
        // Drawn where it was drawn: the gap fades out, unless it is too large to be a mistake of the guess
        const float gapX = _predicted->x + _offsetX - replayed.x;
        const float gapY = _predicted->y + _offsetY - replayed.y;
        if (std::hypot(gapX, gapY) <= MaxSmoothed) {
            _offsetX = gapX;
            _offsetY = gapY;
        } else {
            _offsetX = 0.0f;
            _offsetY = 0.0f;
        }
    }
    _predicted = replayed;
}

void Prediction::advance(double elapsed)
{
    const auto fade = static_cast<float>(std::exp(-Smoothing * std::max(0.0, elapsed)));
    _offsetX *= fade;
    _offsetY *= fade;
}

std::optional<ecs::Position> Prediction::position() const
{
    if (!_predicted) {
        return std::nullopt;
    }
    return ecs::Position{_predicted->x + _offsetX, _predicted->y + _offsetY};
}

std::optional<ecs::Position> Prediction::position(double fraction, game::PlayerInput held) const
{
    if (!_predicted) {
        return std::nullopt;
    }
    const auto part = static_cast<float>(std::clamp(fraction, 0.0, 1.0));
    const ecs::Position next = game::moveShip(*_predicted, held, _rules);
    return ecs::Position{_predicted->x + ((next.x - _predicted->x) * part) + _offsetX,
                         _predicted->y + ((next.y - _predicted->y) * part) + _offsetY};
}

} // namespace rtype::client
