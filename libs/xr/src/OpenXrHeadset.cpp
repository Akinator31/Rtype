#include "Check.hpp"
#include "Controllers.hpp"
#include "Eyes.hpp"
#include "Session.hpp"
#include "rtype/xr/Headset.hpp"

#include <exception>
#include <memory>

namespace rtype::xr
{

namespace
{

/// The headset of the active OpenXR runtime: its session, its eyes and its controllers, destroyed in the
/// reverse order.
class OpenXrHeadset final : public Headset
{
    detail::Session _session;
    detail::Eyes _eyes;
    detail::Controllers _controllers;

public:
    explicit OpenXrHeadset(const HeadsetSettings &settings)
        : _session(settings.applicationName), _eyes(_session, settings.background), _controllers(_session)
    {
    }

    void pollEvents() override { _session.pollEvents(); }
    [[nodiscard]] HeadsetState state() const override { return _session.state(); }
    bool beginFrame() override
    {
        const bool shown = _session.beginFrame();
        // The controllers where they will be when the frame is shown, as the eyes, read even when it is not
        // drawn; nothing held while there is no frame
        if (_session.frameBegun()) {
            _controllers.update(_session.displayTime());
        } else {
            _controllers.forget();
        }
        return shown;
    }
    [[nodiscard]] int eyeCount() const override { return _eyes.count(); }
    EyeView beginEye(int eye) override { return _eyes.begin(eye); }
    void endEye(int eye) override { _eyes.end(eye); }
    void endFrame() override { _session.endFrame(_eyes.layerViews()); }
    void recenter() override { _session.recenter(); }
    [[nodiscard]] HandState hand(Hand hand) const override
    {
        return _controllers.state(static_cast<int>(hand));
    }
    void vibrate(Hand hand, const Vibration &vibration) override
    {
        if (_session.state() == HeadsetState::Running) {
            _controllers.vibrate(static_cast<int>(hand), vibration);
        }
    }
    [[nodiscard]] std::string runtimeName() const override { return _session.runtimeName(); }
};

} // namespace

OpenedHeadset openHeadset(const HeadsetSettings &settings)
{
    try {
        return {.headset = std::make_unique<OpenXrHeadset>(settings), .error = {}};
    } catch (const std::exception &error) {
        return {.headset = nullptr, .error = error.what()};
    }
}

bool isOpenXrBuilt()
{
    return true;
}

} // namespace rtype::xr
