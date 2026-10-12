#pragma once

namespace rtype::physics
{

struct Velocity {
    float x = 0.0f;
    float y = 0.0f;
};

struct Acceleration {
    float x = 0.0f;
    float y = 0.0f;
};

struct MaxSpeed {
    float value = 0.0f;
};

} // namespace rtype::physics
