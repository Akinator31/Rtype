#include "Game/InputClock.hpp"

#include <algorithm>
#include <utility>

namespace rtype::client
{

void InputClock::advance(double elapsed)
{
    _time += std::max(0.0, elapsed);
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
    _synced = true;
    if (lead <= 0) {
        // Lost until the stamps catch up: they jump at once
        _jump += static_cast<std::uint32_t>(TargetLead - lead);
        _reports.clear();
        _settled = _time + std::max(0.0, settle);
        return;
    }
    _reports.push_back({_time, lead});
}

double InputClock::rate() const
{
    const std::optional<int> smallest = lead();
    if (!smallest) {
        return 1.0;
    }
    const int error = *smallest - TargetLead;
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
