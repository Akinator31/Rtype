#pragma once

namespace rtype::physics
{

class FrameSystem
{
public:
    virtual ~FrameSystem() = default;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
};

}
