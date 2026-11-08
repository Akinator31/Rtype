#pragma once

// The devices of the voice without a sound card, for the tests of the voice: a microphone that gives what the
// test says, a playback that keeps what it plays, and a transport that links two GameVoice as the game would.

#include "rtype/voice/Devices.hpp"
#include "rtype/voice/Format.hpp"
#include "rtype/voice/GameVoice.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace testing
{

/// A microphone that gives the samples of say(), as fast as they are read.
struct FakeMicrophone final : rtype::voice::Capture {
    bool present = true;
    bool isOpen = false;
    int opens = 0;
    std::vector<std::int16_t> pending;

    bool open() override
    {
        ++opens;
        isOpen = present;
        return present;
    }

    void close() override
    {
        isOpen = false;
        pending.clear();
    }

    std::size_t read(std::span<std::int16_t> samples) override
    {
        const std::size_t count = std::min(samples.size(), pending.size());
        std::copy_n(pending.begin(), count, samples.begin());
        pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(count));
        return count;
    }

    /// Samples heard by the microphone, if it is open.
    void say(std::span<const std::int16_t> samples)
    {
        if (isOpen) {
            pending.insert(pending.end(), samples.begin(), samples.end());
        }
    }
};

/// A playback that keeps the samples written to each stream.
struct FakePlayback final : rtype::voice::Playback {
    bool present = true;
    std::array<bool, rtype::voice::MaxParticipants> streams{};
    std::array<float, rtype::voice::MaxParticipants> volume{};
    std::array<std::vector<std::int16_t>, rtype::voice::MaxParticipants> played;

    [[nodiscard]] bool available() const override { return present; }
    void open(rtype::voice::Slot slot) override { streams.at(slot) = true; }
    void close(rtype::voice::Slot slot) override { streams.at(slot) = false; }

    bool write(rtype::voice::Slot slot, std::span<const std::int16_t> samples) override
    {
        if (!streams.at(slot)) {
            return false;
        }
        played.at(slot).insert(played.at(slot).end(), samples.begin(), samples.end());
        return true;
    }

    void setVolume(rtype::voice::Slot slot, float value) override { volume.at(slot) = value; }
};

/// The frames a GameVoice sends, kept, and those given to it.
struct FakeTransport final : rtype::voice::VoiceTransport {
    struct Sent {
        std::uint16_t sequence = 0;
        bool last = false;
        std::vector<std::uint8_t> bytes;
    };

    std::vector<Sent> sent;
    std::vector<Incoming> incoming;

    bool send(const Outgoing &frame) override
    {
        sent.push_back({frame.sequence, frame.last, {frame.bytes.begin(), frame.bytes.end()}});
        return true;
    }

    std::vector<Incoming> receive() override { return std::exchange(incoming, {}); }

    /// Gives the frames sent by another one, as the game would, from a slot.
    void deliver(FakeTransport &from, rtype::voice::Slot slot)
    {
        for (Sent &frame : from.sent) {
            incoming.push_back({slot, frame.sequence, frame.last, std::move(frame.bytes)});
        }
        from.sent.clear();
    }
};

} // namespace testing
