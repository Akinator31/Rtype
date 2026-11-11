#include "Game/InputClock.hpp"

#include <algorithm>
#include <utility>

namespace rtype::client
{

void InputClock::advance(double elapsed)
{
    _time += std::max(0.0, elapsed);
    if (_margin > 0 && _time - _lastLate >= Calm) {
        // A tick less, unless the worst datagram of the period needed it: it would be late again
        if (!_lowest || *_lowest > 1) {
            --_margin;
        }
        _lastLate = _time;
        _lowest.reset();
    }
    while (!_reports.empty() && _reports.front().time < _time - Window) {
        _reports.pop_front();
    }
}

void InputClock::receive(int lead, double settle)
{
    if (_time < _settled) {
        // Inputs stamped before the last jump: it is already corrected
        return;
    }
    const bool wasSynced = std::exchange(_synced, true);
    if (lead <= 0) {
        if (wasSynced) {
            // Late again later, unless there is more margin. Not the first report: the estimate was late
            _margin = std::min(_margin + 1, MaxMargin);
            _lastLate = _time;
            _lowest.reset();
        }
        // Lost until the stamps catch up: they jump at once
        _jump += static_cast<std::uint32_t>(target() - lead);
        _reports.clear();
        _settled = _time + std::max(0.0, settle);
        return;
    }
    _reports.push_back({_time, lead});
    _lowest = std::min(_lowest.value_or(lead), lead);
}

double InputClock::rate() const
{
    const std::optional<int> smallest = lead();
    if (!smallest) {
        return 1.0;
    }
    const int error = *smallest - target();
    if (error < 0) {
        return std::min(MaxRate, 1.0 - (error * Gain));
    }
    if (error > Tolerance) {
        return std::max(MinRate, 1.0 - ((error - Tolerance) * Gain));
    }
    return 1.0;
}

std::uint32_t InputClock::takeJump()
{
    return std::exchange(_jump, 0);
}

std::optional<int> InputClock::lead() const
{
    if (_reports.empty()) {
        return std::nullopt;
    }
    return std::ranges::min(_reports, {}, &Report::lead).lead;
}

} // namespace rtype::client
