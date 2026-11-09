#pragma once

#include "Platform.hpp"
#include "rtype/xr/Headset.hpp"

#include <string>
#include <vector>

namespace rtype::xr::detail
{

/// The OpenXR instance, the system (the headset), the session on the OpenGL context of the window and its
/// LOCAL space, and the frames: what every part of the headset shares. Created by the constructor, which
/// throws OpenXrError, destroyed by the destructor.
///
/// The space is LOCAL, not STAGE: the player is seated in a cockpit, and the origin is their head when the
/// session starts, which the runtime moves back when the player recentres.
class Session
{
    XrInstance _instance = XR_NULL_HANDLE;
    XrSystemId _system = XR_NULL_SYSTEM_ID;
    XrSession _session = XR_NULL_HANDLE;
    XrSpace _space = XR_NULL_HANDLE;
    XrEnvironmentBlendMode _blendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    std::string _runtimeName;
    bool _touchControllerPlus = false;

    HeadsetState _state = HeadsetState::Waiting;
    XrFrameState _frame{XR_TYPE_FRAME_STATE};
    bool _frameBegun = false;
    bool _shouldRender = false;
    std::vector<XrView> _views;

    void createInstance(const std::string &applicationName);
    /// Destroys what was created, in the reverse order.
    void release();
    void createSession();
    void changeState(XrSessionState state);
    /// The session is over: logged once, and nothing is called on it any more.
    void lose(const std::string &reason);
    /// Checks the result of a call made every frame: a lost session or instance makes the session lost, any
    /// other failure is logged. True when the call succeeded.
    bool succeeded(XrResult result, const char *what);

public:
    explicit Session(const std::string &applicationName);
    ~Session();

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    Session(Session &&) = delete;
    Session &operator=(Session &&) = delete;

    [[nodiscard]] XrInstance instance() const { return _instance; }
    [[nodiscard]] XrSystemId system() const { return _system; }
    [[nodiscard]] XrSession session() const { return _session; }
    [[nodiscard]] XrSpace space() const { return _space; }
    [[nodiscard]] const std::string &runtimeName() const { return _runtimeName; }
    /// Whether the runtime knows the Touch Plus controllers of the Quest 3 (XR_META_touch_controller_plus).
    [[nodiscard]] bool hasTouchControllerPlus() const { return _touchControllerPlus; }

    void pollEvents();
    [[nodiscard]] HeadsetState state() const { return _state; }

    /// Waits for the next frame and begins it. True when its views were located and must be drawn.
    bool beginFrame();
    /// Where the eyes are this frame, valid after beginFrame() returned true.
    [[nodiscard]] const std::vector<XrView> &views() const { return _views; }
    /// When the frame will be shown: the time to locate the controllers at.
    [[nodiscard]] XrTime displayTime() const { return _frame.predictedDisplayTime; }
    /// Ends the frame begun, showing these views if beginFrame() returned true.
    void endFrame(const std::vector<XrCompositionLayerProjectionView> &views);
};

} // namespace rtype::xr::detail
