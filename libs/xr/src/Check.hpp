#pragma once

#include "Platform.hpp"
#include "rtype/xr/Math.hpp"

#include <stdexcept>
#include <string>

namespace rtype::xr::detail
{

/// A call to OpenXR that failed, with what it was and the name of its result: "xrCreateSession failed
/// (XR_ERROR_GRAPHICS_DEVICE_INVALID)". Thrown while the headset is opened, caught by openHeadset().
class OpenXrError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/// The name of a result: "XR_ERROR_RUNTIME_UNAVAILABLE". Without an instance, the number only.
[[nodiscard]] inline std::string resultName(XrInstance instance, XrResult result)
{
    char name[XR_MAX_RESULT_STRING_SIZE] = {};
    if (instance != XR_NULL_HANDLE && XR_SUCCEEDED(xrResultToString(instance, result, name))) {
        return name;
    }
    return std::to_string(static_cast<int>(result));
}

/// Throws an OpenXrError when the call failed.
inline void check(XrInstance instance, XrResult result, const char *what)
{
    if (XR_FAILED(result)) {
        throw OpenXrError(std::string(what) + " failed (" + resultName(instance, result) + ")");
    }
}

/// Our pose from an OpenXR pose.
[[nodiscard]] inline Pose toPose(const XrPosef &pose)
{
    return {.position = {pose.position.x, pose.position.y, pose.position.z},
            .orientation = {pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w}};
}

} // namespace rtype::xr::detail
