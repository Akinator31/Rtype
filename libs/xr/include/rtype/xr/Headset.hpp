#pragma once

#include "rtype/xr/Math.hpp"

#include <memory>
#include <string>

namespace rtype::xr
{

/// Where the session of a headset is.
enum class HeadsetState {
    /// The runtime has not started the session yet, or has stopped it for a while: no headset worn, the
    /// headset asleep, another application in front. Nothing is drawn; the window paces the frames.
    Waiting,
    /// The headset shows the game: a frame is given to it every time beginFrame() returns, and it paces them.
    Running,
    /// The session is over: the player quit from the runtime, the runtime stopped, the headset was unplugged.
    /// It never comes back; the client goes on on the screen.
    Lost,
};

/// What an eye sees this frame, given by Headset::beginEye().
struct EyeView {
    /// From the space of the headset to the space of the eye.
    Matrix view;
    /// OpenGL projection of the eye, depth in [-1, 1].
    Matrix projection;
    /// Where the eye is, in the space of the headset.
    Vector3 position;
    /// Size of the image of the eye, in pixels.
    int width = 0;
    int height = 0;
};

/// A colour of the background of the eyes, each channel from 0 to 1.
struct Rgb {
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
};

/// A VR headset: the frames it shows, one image per eye. The real one is opened by openHeadset(); the tests
/// use a fake one.
///
/// Once per frame of the game:
/// @code
/// headset.pollEvents();
/// if (headset.state() == HeadsetState::Running && headset.beginFrame()) {
///     for (int eye = 0; eye < headset.eyeCount(); ++eye) {
///         const EyeView view = headset.beginEye(eye);   // binds the image of the eye, cleared
///         // ... draw with view.view and view.projection
///         headset.endEye(eye);
///     }
/// }
/// headset.endFrame();   // always, even when beginFrame() returned false
/// @endcode
class Headset
{
public:
    virtual ~Headset() = default;

    /// Reads what the runtime says (the session starts, stops, is lost). Call it once per frame, before
    /// beginFrame().
    virtual void pollEvents() = 0;
    [[nodiscard]] virtual HeadsetState state() const = 0;

    /// Waits until the headset is ready for the next image, which paces the game while the headset runs. True
    /// when the eyes must be drawn: false while the headset shows nothing of ours (in its menu), or when the
    /// tracking is lost for this frame.
    virtual bool beginFrame() = 0;
    [[nodiscard]] virtual int eyeCount() const = 0;
    /// Binds the image of an eye, cleared to the background colour with an empty depth buffer, and gives what
    /// it sees. Only between beginFrame() returning true and endFrame().
    virtual EyeView beginEye(int eye) = 0;
    /// Gives the image of the eye back to the headset.
    virtual void endEye(int eye) = 0;
    /// Shows the eyes drawn since beginFrame(), or nothing if it returned false.
    virtual void endFrame() = 0;

    /// The name of the runtime and its version, for the logs ("Monado 25.0.0").
    [[nodiscard]] virtual std::string runtimeName() const = 0;
};

/// What openHeadset() needs.
struct HeadsetSettings {
    /// The name the runtime shows in its menu.
    std::string applicationName = "R-Type";
    /// The colour the eyes are cleared to.
    Rgb background{0.02f, 0.03f, 0.08f};
};

/// The result of openHeadset(): a headset, or why there is none.
struct OpenedHeadset {
    std::unique_ptr<Headset> headset;
    /// Why there is no headset ("no OpenXR runtime is installed or active"), empty with one.
    std::string error;
};

/// Opens the headset of the active OpenXR runtime, on the OpenGL context of the window: call it after the
/// window is open, from the thread that opened it, and destroy the headset before the window closes. No
/// runtime, no headset, or a build without OpenXR (RTYPE_OPENXR off) give no headset and the reason.
[[nodiscard]] OpenedHeadset openHeadset(const HeadsetSettings &settings = {});

/// Whether this build can open a headset: false when it was built with RTYPE_OPENXR off.
[[nodiscard]] bool isOpenXrBuilt();

} // namespace rtype::xr
