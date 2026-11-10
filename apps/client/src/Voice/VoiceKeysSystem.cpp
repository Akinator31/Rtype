#include "Voice/VoiceKeysSystem.hpp"

#include "Game/RemoteSession.hpp"
#include "Settings/ControlSettings.hpp"

namespace rtype::client
{

VoiceKeysSystem::VoiceKeysSystem(ecs::ComponentCaches & /*caches*/, engine::Engine &engine,
                                 const GameSessionSystem &sessions, VoiceDevices &voice,
                                 const localization::Localization &translations)
    : _engine(engine), _sessions(sessions), _voice(voice), _translations(translations)
{
}

bool VoiceKeysSystem::playingOnline() const
{
    const std::optional<engine::SceneId> scene = _sessions.scene();
    return scene && dynamic_cast<const RemoteSession *>(_sessions.session()) != nullptr &&
           _engine.getScenes().receivesInput(*scene);
}

void VoiceKeysSystem::toast(const std::string &key, engine::ToastType type)
{
    _engine.showToast(_translations.text(key), type);
}

void VoiceKeysSystem::toggleMute()
{
    if (_voice.preferences.mode == voice::TalkMode::Off) {
        toast("voice.toast.off");
        return;
    }
    if (_voice.deafened) {
        _voice.deafened = false;
        _voice.muted = false;
    } else {
        _voice.muted = !_voice.muted;
    }
    toast(_voice.muted ? "voice.toast.muted" : "voice.toast.unmuted");
}

void VoiceKeysSystem::toggleDeafen()
{
    _voice.deafened = !_voice.deafened;
    toast(_voice.deafened ? "voice.toast.deafened" : "voice.toast.undeafened");
}

void VoiceKeysSystem::update(float /*elapsed*/)
{
    const engine::Controls &controls = _engine.getControls();
    const bool playing = playingOnline();
    _voice.pushToTalk = playing && controls.isDown(PushToTalkControl);
    if (!playing) {
        return;
    }
    if (controls.wasPressed(MuteControl)) {
        toggleMute();
    }
    if (controls.wasPressed(DeafenControl)) {
        toggleDeafen();
    }
    if (_voice.microphoneSilent && !_silenceTold) {
        toast("voice.toast.silent", engine::ToastType::Warning);
    }
    _silenceTold = _voice.microphoneSilent;
}

} // namespace rtype::client
