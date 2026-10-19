#include "Instance/InputBuffer.hpp"

namespace rtype::server
{

void InputBuffer::receive(std::uint32_t newest, std::span<const game::PlayerInput> inputs,
                          std::uint32_t current)
{
    for (std::size_t i = 0; i < inputs.size() && i <= newest; ++i) {
        const std::uint32_t tick = newest - static_cast<std::uint32_t>(i);
        // Too late, or too far ahead: a client whose clock runs away cannot fill the memory of the game
        if (tick <= current || tick - current > MaxAhead) {
            continue;
        }
        // try_emplace: a tick already received keeps its first input
        _inputs.try_emplace(tick, inputs[i]);
    }
}

game::PlayerInput InputBuffer::take(std::uint32_t tick)
{
    const auto found = _inputs.find(tick);
    if (found != _inputs.end()) {
        _last = found->second;
        _lastApplied = tick;
        _silence = 0;
    } else if (++_silence > MaxSilence) {
        _last = {};
    }
    // The inputs of this tick and before can no longer be applied
    _inputs.erase(_inputs.begin(), _inputs.upper_bound(tick));
    return _last;
}

} // namespace rtype::server
