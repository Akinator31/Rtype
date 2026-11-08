#pragma once

// A voice channel that opens no device and carries no voice, for the tests of what drives the voice (the
// session, the HUD, the settings): it keeps what it is told. A test makes a player talk with speak().

#include "rtype/voice/VoiceChannel.hpp"

#include <algorithm>
#include <optional>
#include <vector>

namespace testing
{

struct FakeVoiceChannel final : rtype::voice::VoiceChannel {
    std::optional<rtype::voice::Join> joined;
    std::vector<rtype::voice::Participant> players;
    std::vector<rtype::voice::Position> positions =
        std::vector<rtype::voice::Position>(rtype::voice::MaxParticipants);
    bool selfMuted = false;
    bool talking = false;
    int polls = 0;

    void join(const rtype::voice::Join &join) override
    {
        leave();
        joined = join;
    }

    void leave() override
    {
        joined.reset();
        players.clear();
        talking = false;
    }

    void addParticipant(rtype::voice::Slot slot) override
    {
        if (slot >= rtype::voice::MaxParticipants || (joined && slot == joined->self) || find(slot)) {
            return;
        }
        players.push_back({.slot = slot});
        std::ranges::sort(players, {}, &rtype::voice::Participant::slot);
    }

    void removeParticipant(rtype::voice::Slot slot) override
    {
        std::erase_if(players,
                      [slot](const rtype::voice::Participant &player) { return player.slot == slot; });
    }

    void setBackend(rtype::voice::Slot slot, rtype::voice::Backend backend) override
    {
        if (rtype::voice::Participant *player = find(slot)) {
            player->backend = backend;
        }
    }

    void setSelfMute(bool muted) override { selfMuted = muted; }
    void setTalking(bool value) override { talking = value; }

    void setParticipantVolume(rtype::voice::Slot slot, float volume) override
    {
        if (rtype::voice::Participant *player = find(slot)) {
            player->volume = std::clamp(volume, 0.0f, rtype::voice::MaxVolume);
        }
    }

    void setParticipantMute(rtype::voice::Slot slot, bool muted) override
    {
        if (rtype::voice::Participant *player = find(slot)) {
            player->mutedByMe = muted;
        }
    }

    void setParticipantPosition(rtype::voice::Slot slot, rtype::voice::Position position) override
    {
        if (slot < positions.size()) {
            positions[slot] = position;
        }
    }

    [[nodiscard]] std::vector<rtype::voice::Participant> participants() const override { return players; }

    void poll() override { ++polls; }

    /// What the voice of a real channel would tell: a player starts or stops talking.
    void speak(rtype::voice::Slot slot, bool speaking)
    {
        if (rtype::voice::Participant *player = find(slot)) {
            player->speaking = speaking;
        }
    }

    [[nodiscard]] rtype::voice::Participant *find(rtype::voice::Slot slot)
    {
        const auto found = std::ranges::find(players, slot, &rtype::voice::Participant::slot);
        return found == players.end() ? nullptr : &*found;
    }
};

} // namespace testing
