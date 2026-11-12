#pragma once

// A headset that opens no runtime and draws nothing, for the tests of what drives VR in the client: it counts
// what it is asked, and a test moves it from state to state with `currentState`, or loses it on its next
// frame with `loseOnFrame`. Its controllers are what the test puts in `hands`; it keeps the vibrations asked.

#include "rtype/xr/Headset.hpp"

#include <array>
#include <utility>
#include <vector>

namespace testing
{

struct FakeHeadsetCalls {
    int polls = 0;
    int frames = 0;
    int ended = 0;
    int eyes = 0;
    int recentred = 0;
    /// The turns asked, in degrees.
    std::vector<float> turns;
    bool destroyed = false;
};

struct FakeHeadset final : rtype::xr::Headset {
    FakeHeadsetCalls &calls;
    rtype::xr::HeadsetState currentState = rtype::xr::HeadsetState::Waiting;
    bool shouldRender = true;
    bool loseOnFrame = false;
    std::array<rtype::xr::HandState, rtype::xr::HandCount> hands{};
    std::vector<std::pair<rtype::xr::Hand, rtype::xr::Vibration>> vibrations;

    explicit FakeHeadset(FakeHeadsetCalls &calls) : calls(calls) {}
    ~FakeHeadset() override { calls.destroyed = true; }

    FakeHeadset(const FakeHeadset &) = delete;
    FakeHeadset &operator=(const FakeHeadset &) = delete;
    FakeHeadset(FakeHeadset &&) = delete;
    FakeHeadset &operator=(FakeHeadset &&) = delete;

    void pollEvents() override { ++calls.polls; }
    [[nodiscard]] rtype::xr::HeadsetState state() const override { return currentState; }
    bool beginFrame() override
    {
        ++calls.frames;
        if (loseOnFrame) {
            currentState = rtype::xr::HeadsetState::Lost;
            return false;
        }
        return shouldRender;
    }
    [[nodiscard]] int eyeCount() const override { return 2; }
    rtype::xr::EyeView beginEye(int /*eye*/) override
    {
        ++calls.eyes;
        return {};
    }
    void endEye(int /*eye*/) override {}
    void endFrame() override { ++calls.ended; }
    void recenter() override { ++calls.recentred; }
    void turn(float degrees) override { calls.turns.push_back(degrees); }
    [[nodiscard]] rtype::xr::HandState hand(rtype::xr::Hand hand) const override
    {
        return hands.at(static_cast<std::size_t>(hand));
    }
    void vibrate(rtype::xr::Hand hand, const rtype::xr::Vibration &vibration) override
    {
        vibrations.emplace_back(hand, vibration);
    }
    [[nodiscard]] std::string runtimeName() const override { return "Fake 1.0.0"; }
};

} // namespace testing
