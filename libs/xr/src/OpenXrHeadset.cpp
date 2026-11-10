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
    bool beginFrame() override { return _session.beginFrame(); }
    [[nodiscard]] int eyeCount() const override { return _eyes.count(); }
    EyeView beginEye(int eye) override { return _eyes.begin(eye); }
    void endEye(int eye) override { _eyes.end(eye); }
    void endFrame() override { _session.endFrame(_eyes.layerViews()); }
    void recenter() override { _session.recenter(); }
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
