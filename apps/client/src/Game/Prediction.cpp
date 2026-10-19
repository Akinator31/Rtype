#include "Game/Prediction.hpp"

#include "rtype/game/ShipMovement.hpp"

#include <algorithm>
#include <cmath>

namespace rtype::client
{

void Prediction::apply(std::uint32_t tick, game::PlayerInput input)
{
    // The ticks skipped since the previous input: the game repeats the previous input for them
    if (!_sent.empty() && tick > _sent.back().tick + 1) {
        const game::PlayerInput previous = _sent.back().input;
        const std::uint32_t first = std::max(_sent.back().tick + 1, tick - std::min(tick, MaxFilled));
        for (std::uint32_t skipped = first; skipped < tick; ++skipped) {
            _sent.push_back({skipped, previous});
            if (_predicted) {
                _predicted = game::moveShip(*_predicted, previous, _rules);
            }
        }
    }
    if (!_sent.empty() && tick <= _sent.back().tick) {
        // Never twice the same tick: the stamps only go forward
        return;
    }
    _sent.push_back({tick, input});
    if (_predicted) {
        _predicted = game::moveShip(*_predicted, input, _rules);
    }
    while (_sent.size() > MaxInputs) {
        _sent.pop_front();
    }
}

void Prediction::reconcile(std::optional<ecs::Position> ship, std::uint32_t tick)
{
    // Applied by the game: no longer to replay
    while (!_sent.empty() && _sent.front().tick <= tick) {
        _sent.pop_front();
    }
    if (!ship) {
        // Destroyed, or not back yet: the game decides
        _predicted.reset();
        _offsetX = 0.0f;
        _offsetY = 0.0f;
        return;
    }

    // From where the game had it, the inputs it has not applied yet
    ecs::Position replayed = *ship;
    for (const Sent &sent : _sent) {
        replayed = game::moveShip(replayed, sent.input, _rules);
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
