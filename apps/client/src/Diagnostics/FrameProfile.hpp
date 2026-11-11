#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/Types.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace rtype::client
{

/// What a frame took, part by part, in seconds.
struct FrameSample {
    /// From the start of this frame to the start of the next one: what the player sees.
    double total = 0.0;
    /// The input, the network, the update of the engine, the loads finished and the audio.
    double update = 0.0;
    /// The drawing calls, on the processor: the graphics card works later.
    double draw = 0.0;
    /// Showing the frame: waiting for the graphics card to finish it, and for the screen (vertical sync).
    double present = 0.0;
    /// The poses of the animated models in the frame: part of `draw`.
    render::PoseStats poses{};
    /// The animated models drawn in the frame.
    std::size_t animatedModels = 0;
};

/// Finds why frames are slow: records the parts of every frame, and reports, once a second, the average and
/// the longest of each part, then every frame that took more than twice the average of the second before.
/// The reports are lines of text, logged by the client.
/// @code
/// FrameProfile profile([](const std::string &line) { logger.info("{}", line); });
/// profile.record({.total = 0.016, .update = 0.002, .draw = 0.004, .present = 0.010});
/// @endcode
class FrameProfile
{
    /// The parts of the frames of a second, added up, and the longest of each.
    struct Window {
        std::size_t frames = 0;
        FrameSample sum{};
        FrameSample longest{};
    };

    std::function<void(const std::string &)> _report;
    Window _window{};
    /// The average frame of the second before, 0 for none yet: a frame is slow next to it.
    double _usualFrame = 0.0;
    std::size_t _slowFrames = 0;

    void reportSlowFrame(const FrameSample &frame) const;
    void reportWindow();

public:
    /// A frame is slow when it takes this many times the average frame of the second before...
    static constexpr double SlowFactor = 2.0;
    /// ...and at least this long, in seconds: a frame of 2 ms after frames of 1 ms is not a lag.
    static constexpr double SlowAtLeast = 0.010;

    /// @param report Given each line of the report.
    explicit FrameProfile(std::function<void(const std::string &)> report);

    /// Adds a frame: reports it if it is slow, and the second it ends, if it does.
    void record(const FrameSample &frame);
    /// Forgets the frames recorded: when the profile starts again.
    void reset();
};

/// Times the parts of the frames of the game loop, and gives each frame to a FrameProfile once the next one
/// starts: its total and its showing are only known then.
/// @code
/// timer.startFrame(enabled, renderer.takePoseStats(), countAnimatedModels(ecs, scenes));
/// updateEverything();
/// timer.endUpdate();
/// drawEverything();
/// timer.endDraw();
/// renderer.endFrame();
/// @endcode
class FrameTimer
{
    using Clock = std::chrono::steady_clock;

    FrameProfile &_profile;
    /// The start of the frame being timed, none while the profile is disabled.
    std::optional<Clock::time_point> _start;
    Clock::time_point _updateEnd{};
    Clock::time_point _drawEnd{};
    FrameSample _frame{};

public:
    /// @param profile It must outlive the timer.
    explicit FrameTimer(FrameProfile &profile);

    /// Starts a frame: gives the previous one to the profile, with what it drew.
    /// @param enabled Whether the frames are profiled: the profile starts again from nothing when it is
    /// enabled again.
    /// @param poses, animatedModels What the previous frame drew.
    void startFrame(bool enabled, const render::PoseStats &poses, std::size_t animatedModels);
    /// The update is over, the drawing starts.
    void endUpdate();
    /// The drawing is over, the frame is being shown.
    void endDraw();
};

/// The animated models drawn: the entities of the visible scenes with a Model and a ModelAnimation.
[[nodiscard]] std::size_t countAnimatedModels(ecs::Ecs &ecs, const engine::Scenes &scenes);

} // namespace rtype::client
