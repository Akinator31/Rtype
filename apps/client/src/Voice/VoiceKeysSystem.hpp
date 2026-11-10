#pragma once

#include "Game/GameSessionSystem.hpp"
#include "Voice/VoiceDevices.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/localization/Localization.hpp"

#include <string>

namespace rtype::client
{

/// The keys of the voice in a game online (docs/architecture/voice.md#the-keys), each frame, while the game
/// receives the input (no menu over it):
///
/// - the push-to-talk key held (PushToTalkControl) is given to the sessions in VoiceDevices::pushToTalk;
///   released as soon as a menu covers the game;
/// - the mute key (MuteControl) mutes and unmutes the player, the deafen key (DeafenControl) deafens it and
///   lets it hear again; unmuting a deafened player lets it hear again too. A toast tells each change;
/// - a toast warns, once per silence, when the session tells that the microphone gives nothing while the
///   player talks (VoiceDevices::microphoneSilent).
///
/// Offline, there is no voice: the keys do nothing.
class VoiceKeysSystem : public ecs::System
{
    engine::Engine &_engine;
    const GameSessionSystem &_sessions;
    VoiceDevices &_voice;
    const localization::Localization &_translations;
    /// The silence of the microphone was told.
    bool _silenceTold = false;

    /// An online game is played and receives the input.
    [[nodiscard]] bool playingOnline() const;
    void toast(const std::string &key, engine::ToastType type = engine::ToastType::Info);
    void toggleMute();
    void toggleDeafen();

public:
    /// @param engine, sessions, voice, translations They must outlive the system.
    VoiceKeysSystem(ecs::ComponentCaches &caches, engine::Engine &engine, const GameSessionSystem &sessions,
                    VoiceDevices &voice, const localization::Localization &translations);

    void update(float elapsed) override;
};

} // namespace rtype::client
