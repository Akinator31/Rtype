#pragma once

#include "Session.hpp"

#include <array>

namespace rtype::xr::detail
{

/// The actions of the controllers and the bindings suggested for each profile the game knows, attached to the
/// session. OpenXR takes the bindings once, before the session starts: they are made here with the session,
/// even while nothing reads them yet (docs/architecture/vr.md#next-steps). Created by the
/// constructor, which throws OpenXrError; the actions are destroyed with their set, by the destructor.
class Controllers
{
public:
    /// The left hand, then the right one.
    static constexpr int HandCount = 2;

private:
    Session &_session;
    XrActionSet _actionSet = XR_NULL_HANDLE;
    XrAction _grip = XR_NULL_HANDLE;
    XrAction _aim = XR_NULL_HANDLE;
    XrAction _squeeze = XR_NULL_HANDLE;
    XrAction _trigger = XR_NULL_HANDLE;
    XrAction _thumbstick = XR_NULL_HANDLE;
    XrAction _primary = XR_NULL_HANDLE;
    XrAction _secondary = XR_NULL_HANDLE;
    XrAction _menu = XR_NULL_HANDLE;
    XrAction _haptic = XR_NULL_HANDLE;
    std::array<XrPath, HandCount> _hands{XR_NULL_PATH, XR_NULL_PATH};
    std::array<XrSpace, HandCount> _gripSpaces{XR_NULL_HANDLE, XR_NULL_HANDLE};
    std::array<XrSpace, HandCount> _aimSpaces{XR_NULL_HANDLE, XR_NULL_HANDLE};

    void createActions();
    void suggestBindings();
    void createSpaces();
    void release();

public:
    explicit Controllers(Session &session);
    ~Controllers();

    Controllers(const Controllers &) = delete;
    Controllers &operator=(const Controllers &) = delete;
    Controllers(Controllers &&) = delete;
    Controllers &operator=(Controllers &&) = delete;
};

} // namespace rtype::xr::detail
