#include "Vr/VrMode.hpp"

#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <utility>

namespace rtype::client
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("vr");
    return instance;
}

} // namespace

bool wantsVr(std::span<const std::string_view> arguments, const engine::Settings &settings)
{
    return std::ranges::find(arguments, VrOption) != arguments.end() ||
           settings.getBool(VrSettingsKey).value_or(false);
}

VrMode::VrMode(VrHooks hooks) : _hooks(std::move(hooks)) {}

bool VrMode::start()
{
    if (_headset != nullptr) {
        return true;
    }
    xr::OpenedHeadset opened = _hooks.open();
    if (opened.headset == nullptr) {
        _error = std::move(opened.error);
        logger().warning("no VR headset, the game stays on the screen: {}", _error);
        return false;
    }
    _error.clear();
    _headset = std::move(opened.headset);
    logger().info("VR headset open on {}", _headset->runtimeName());
    return true;
}

void VrMode::pace(bool paced)
{
    if (paced != _paced) {
        _paced = paced;
        _hooks.pace(paced);
    }
}

void VrMode::stop()
{
    pace(false);
    _headset.reset();
    _hooks.lost();
}

void VrMode::update()
{
    if (_headset == nullptr) {
        return;
    }
    _headset->pollEvents();
    if (_headset->state() == xr::HeadsetState::Lost) {
        stop();
        return;
    }
    pace(_headset->state() == xr::HeadsetState::Running);
    if (!_paced) {
        return;
    }
    if (_headset->beginFrame()) {
        for (int eye = 0; eye < _headset->eyeCount(); ++eye) {
            const xr::EyeView view = _headset->beginEye(eye);
            if (_hooks.drawEye) {
                _hooks.drawEye(eye, view);
            }
            _headset->endEye(eye);
        }
    }
    _headset->endFrame();
    // A frame can lose the session (the runtime stopped while it was waited for)
    if (_headset->state() == xr::HeadsetState::Lost) {
        stop();
    }
}

} // namespace rtype::client
