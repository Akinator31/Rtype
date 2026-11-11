#include "rtype/xr/Headset.hpp"

// The build without OpenXR (RTYPE_OPENXR off, macOS): the same interface, never a headset, so that the client
// has no case of its own for it

namespace rtype::xr
{

OpenedHeadset openHeadset(const HeadsetSettings & /*settings*/)
{
    return {.headset = nullptr, .error = "this build of R-Type has no VR (built with RTYPE_OPENXR off)"};
}

bool isOpenXrBuilt()
{
    return false;
}

} // namespace rtype::xr
