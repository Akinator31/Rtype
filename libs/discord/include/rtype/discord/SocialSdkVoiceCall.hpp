#pragma once

#include "rtype/discord/VoiceCall.hpp"

#include <memory>

namespace rtype::discord
{

class Session;

/// The VoiceCall of the game, through the Discord Social SDK: `Client::CreateOrJoinLobbyWithMetadata` with
/// the secret of the game, then `Client::StartCallWithAudioCallbacks` in its lobby. Its received callback
/// sets `outShouldMute`, so that Discord plays none of the voices it gives: the game does. The members and
/// their metadata come from the `LobbyHandle`, who speaks from `Call::SetSpeakingStatusChangedCallback`.
///
/// It needs the connection of the Session (SocialSdkConnection) to be ready: before, or without Discord,
/// start() fails at once, and never starts the SDK for a player who never used Discord. No secret is logged.
class SocialSdkVoiceCall final : public VoiceCall
{
    struct State;
    Session &_session;
    std::shared_ptr<State> _state;

    /// Gives the call what the player chose, once it exists.
    void apply();

public:
    /// Joins through the client of `session`, which outlives it.
    explicit SocialSdkVoiceCall(Session &session);
    /// Ends the call, if any.
    ~SocialSdkVoiceCall() override;

    SocialSdkVoiceCall(const SocialSdkVoiceCall &) = delete;
    SocialSdkVoiceCall &operator=(const SocialSdkVoiceCall &) = delete;

    void start(const std::string &secret, const std::map<std::string, std::string> &metadata,
               Received received, Captured captured) override;
    void end() override;
    [[nodiscard]] CallStatus status() const override;
    [[nodiscard]] std::vector<CallMember> members() const override;
    [[nodiscard]] bool speaking() const override;
    void setSelfMute(bool muted) override;
    void setSelfDeaf(bool deafened) override;
    void setTalkMode(CallTalkMode mode, float threshold) override;
    void setPushToTalk(bool active) override;
    void setInputVolume(float volume) override;
    void setProcessing(bool noiseSuppression, bool gainControl) override;
};

} // namespace rtype::discord
