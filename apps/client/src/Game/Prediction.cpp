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
    _weapon = Weapon{std::move(weapon), projectileHalfWidth, slot};
    if (_predicted && !_armed) {
        _armed = equipped(_weapon->definition);
    }
}

std::optional<game::ShotLaunch> Prediction::step(ecs::Position &position, std::optional<game::Armed> &armed,
                                                 game::PlayerInput input) const
{
    std::optional<game::ShotLaunch> shot;
    // As the game: the weapon fires from where the ship was, then the ship moves (WeaponFiring.hpp)
    if (_weapon && armed &&
        game::stepWeapon(*armed, _weapon->definition, input.isHeld(game::PlayerInput::Fire))) {
        shot = game::playerShot(_weapon->definition, _weapon->projectileHalfWidth, _weapon->slot, position,
                                input.aim(), _rules);
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
                if (const auto shot = step(*_predicted, _armed, previous)) {
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
        if (const auto shot = step(*_predicted, _armed, input)) {
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
        _shots.clear();
        _offsetX = 0.0f;
        _offsetY = 0.0f;
        return;
    }

    // From where the game had it, and its weapon, the inputs it has not applied yet
    ecs::Position replayed = *ship;
    const bool rewound = _weapon && weapon;
    std::optional<game::Armed> armed;
    if (rewound) {
        armed = game::Armed{_weapon->definition.id, weapon->cooldown, weapon->ammo, weapon->reloadLeft};
    }
    std::map<std::uint32_t, game::ShotLaunch> shots;
    for (const Sent &sent : _sent) {
        if (const auto shot = step(replayed, armed, sent.input)) {
            shots[sent.tick] = *shot;
        }
    }
    if (rewound) {
        _armed = armed;
        _shots = std::move(shots);
    } else {
        // Without the weapon of the snapshot: the ticks up to it are the game's
        std::erase_if(_shots, [tick](const auto &entry) { return entry.first <= tick; });
        if (_weapon && !_armed) {
            _armed = equipped(_weapon->definition);
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

} // namespace rtype::client
