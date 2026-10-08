#include "rtype/engine/Component/Animation.hpp"

#include <algorithm>

namespace rtype::engine
{

float ease(Easing easing, float time)
{
    const float t = std::clamp(time, 0.0f, 1.0f);
    switch (easing) {
    case Easing::Linear:
        return t;
    case Easing::EaseOut:
        return 1.0f - (1.0f - t) * (1.0f - t);
    case Easing::EaseOutBack: {
        // Classic "back" curve: overshoots by about 10% before settling
        constexpr float overshoot = 1.70158f;
        const float u = t - 1.0f;
        return 1.0f + (overshoot + 1.0f) * u * u * u + overshoot * u * u;
    }
    }
    return t;
}

} // namespace rtype::engine
