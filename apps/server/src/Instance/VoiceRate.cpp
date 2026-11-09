#include "Instance/VoiceRate.hpp"

#include <algorithm>

namespace rtype::server
{

bool VoiceRate::allow(Clock::time_point now)
{
    if (_started && now > _last) {
        const double elapsed = std::chrono::duration<double>(now - _last).count();
        _tokens = std::min(Burst, _tokens + elapsed * _rate);
    }
    if (!_started || now > _last) {
        _last = now;
    }
    _started = true;
    if (_tokens < 1.0) {
        return false;
    }
    _tokens -= 1.0;
    return true;
}

} // namespace rtype::server
