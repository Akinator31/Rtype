#pragma once

#include "Session.hpp"

#include <array>

namespace rtype::xr::detail
{

/// The actions of the controllers and the bindings suggested for each profile the game knows (Touch, Touch
/// Plus, Valve Index, HTC Vive, the simple controller), attached to the session, and what they give each
/// frame. OpenXR takes the bindings once, before the session starts: they are made here with the session.
/// Created by the constructor, which throws OpenXrError; the actions are destroyed with their set, by the
/// destructor.
class Controllers
{
public:
    /// The left hand, then the right one.
    static constexpr int HandCount = xr::HandCount;

private:
    Session &_session;
    XrActionSet _actionSet = XR_NULL_HANDLE;
    XrAction _grip = XR_NULL_HANDLE;
    XrAction _aim = XR_NULL_HANDLE;
    XrAction _squeeze = XR_NULL_HANDLE;
    XrAction _trigger = XR_NULL_HANDLE;
    XrAction _thumbstick = XR_NULL_HANDLE;
    XrAction _thumbstickClick = XR_NULL_HANDLE;
    XrAction _primary = XR_NULL_HANDLE;
    XrAction _secondary = XR_NULL_HANDLE;
    XrAction _menu = XR_NULL_HANDLE;
    XrAction _haptic = XR_NULL_HANDLE;
    std::array<XrPath, HandCount> _hands{XR_NULL_PATH, XR_NULL_PATH};
    std::array<XrSpace, HandCount> _gripSpaces{XR_NULL_HANDLE, XR_NULL_HANDLE};
    std::array<XrSpace, HandCount> _aimSpaces{XR_NULL_HANDLE, XR_NULL_HANDLE};
    std::array<HandState, HandCount> _states{};

    void createActions();
    void suggestBindings();
    void createSpaces();
    void release();
    /// Reads the actions of a hand, synchronised: a float, a vector, a boolean, a pose.
    [[nodiscard]] float readFloat(XrAction action, int hand) const;
    [[nodiscard]] bool readBoolean(XrAction action, int hand) const;
    /// Where a space of a hand is when the frame is shown, if the runtime knows it.
    [[nodiscard]] bool locate(XrSpace space, XrTime time, Pose &pose) const;

public:
    explicit Controllers(Session &session);
    ~Controllers();

    Controllers(const Controllers &) = delete;
    Controllers &operator=(const Controllers &) = delete;
    Controllers(Controllers &&) = delete;
    Controllers &operator=(Controllers &&) = delete;

    /// Reads what the controllers give, where they will be at `time`: once per frame, after it was begun.
    /// Everything is released while the session is not focused (the runtime shows its menu over the game).
    void update(XrTime time);
    /// Releases everything and tracks nothing: the session does not show the game.
    void forget() { _states = {}; }
    [[nodiscard]] const HandState &state(int hand) const { return _states.at(hand); }
    /// Makes a controller vibrate, at once.
    void vibrate(int hand, const Vibration &vibration);
};

} // namespace rtype::xr::detail
