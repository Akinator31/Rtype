#include "Diagnostics/FrameProfile.hpp"

#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace rtype::client
{

namespace
{

/// Length of the windows the averages are made over, in seconds.
constexpr double WindowLength = 1.0;

constexpr double MillisecondsPerSecond = 1000.0;

[[nodiscard]] double secondsBetween(std::chrono::steady_clock::time_point from,
                                    std::chrono::steady_clock::time_point to)
{
    return std::chrono::duration<double>(to - from).count();
}

[[nodiscard]] double toMilliseconds(double seconds)
{
    return seconds * MillisecondsPerSecond;
}

void addUp(FrameSample &sum, const FrameSample &frame)
{
    sum.total += frame.total;
    sum.update += frame.update;
    sum.draw += frame.draw;
    sum.present += frame.present;
    sum.poses.poses += frame.poses.poses;
    sum.poses.glowPoses += frame.poses.glowPoses;
    sum.poses.vertices += frame.poses.vertices;
    sum.poses.seconds += frame.poses.seconds;
    sum.animatedModels += frame.animatedModels;
}

void keepLongest(FrameSample &longest, const FrameSample &frame)
{
    longest.total = std::max(longest.total, frame.total);
    longest.update = std::max(longest.update, frame.update);
    longest.draw = std::max(longest.draw, frame.draw);
    longest.present = std::max(longest.present, frame.present);
    longest.poses.poses = std::max(longest.poses.poses, frame.poses.poses);
    longest.poses.glowPoses = std::max(longest.poses.glowPoses, frame.poses.glowPoses);
    longest.poses.vertices = std::max(longest.poses.vertices, frame.poses.vertices);
    longest.poses.seconds = std::max(longest.poses.seconds, frame.poses.seconds);
    longest.animatedModels = std::max(longest.animatedModels, frame.animatedModels);
}

} // namespace

FrameProfile::FrameProfile(std::function<void(const std::string &)> report) : _report(std::move(report)) {}

void FrameProfile::record(const FrameSample &frame)
{
    if (_usualFrame > 0.0 && frame.total > std::max(_usualFrame * SlowFactor, SlowAtLeast)) {
        ++_slowFrames;
        reportSlowFrame(frame);
    }
    ++_window.frames;
    addUp(_window.sum, frame);
    keepLongest(_window.longest, frame);
    if (_window.sum.total >= WindowLength) {
        reportWindow();
    }
}

void FrameProfile::reset()
{
    _window = {};
    _usualFrame = 0.0;
    _slowFrames = 0;
}

void FrameProfile::reportSlowFrame(const FrameSample &frame) const
{
    _report(std::format("slow frame: {:.1f} ms (usual {:.1f} ms) | update {:.1f} ms, draw {:.1f} ms, present "
                        "{:.1f} ms | poses {} ({} for the glow), {} vertices, {:.1f} ms, {} animated models",
                        toMilliseconds(frame.total), toMilliseconds(_usualFrame),
                        toMilliseconds(frame.update), toMilliseconds(frame.draw),
                        toMilliseconds(frame.present), frame.poses.poses, frame.poses.glowPoses,
                        frame.poses.vertices, toMilliseconds(frame.poses.seconds), frame.animatedModels));
}

void FrameProfile::reportWindow()
{
    const auto frames = static_cast<double>(_window.frames);
    const FrameSample &sum = _window.sum;
    const FrameSample &longest = _window.longest;
    _report(std::format(
        "{} frames, {} slow | frame {:.1f} ms (longest {:.1f}) | update {:.1f} ({:.1f}) | draw {:.1f} "
        "({:.1f}) "
        "| present {:.1f} ({:.1f}) | poses {:.1f} a frame ({}), glow {:.1f} ({}), {:.0f} vertices ({}), "
        "{:.2f} ms ({:.2f}) | animated models {:.1f} ({})",
        _window.frames, _slowFrames, toMilliseconds(sum.total / frames), toMilliseconds(longest.total),
        toMilliseconds(sum.update / frames), toMilliseconds(longest.update),
        toMilliseconds(sum.draw / frames), toMilliseconds(longest.draw), toMilliseconds(sum.present / frames),
        toMilliseconds(longest.present), static_cast<double>(sum.poses.poses) / frames, longest.poses.poses,
        static_cast<double>(sum.poses.glowPoses) / frames, longest.poses.glowPoses,
        static_cast<double>(sum.poses.vertices) / frames, longest.poses.vertices,
        toMilliseconds(sum.poses.seconds / frames), toMilliseconds(longest.poses.seconds),
        static_cast<double>(sum.animatedModels) / frames, longest.animatedModels));
    _usualFrame = sum.total / frames;
    _slowFrames = 0;
    _window = {};
}

FrameTimer::FrameTimer(FrameProfile &profile) : _profile(profile) {}

void FrameTimer::startFrame(bool enabled, const render::PoseStats &poses, std::size_t animatedModels)
{
    const Clock::time_point now = Clock::now();
    if (!enabled) {
        _start.reset();
        return;
    }
    if (_start) {
        _frame.total = secondsBetween(*_start, now);
        _frame.update = secondsBetween(*_start, _updateEnd);
        _frame.draw = secondsBetween(_updateEnd, _drawEnd);
        _frame.present = secondsBetween(_drawEnd, now);
        _frame.poses = poses;
        _frame.animatedModels = animatedModels;
        _profile.record(_frame);
    } else {
        _profile.reset();
    }
    _start = now;
    _updateEnd = now;
    _drawEnd = now;
}

void FrameTimer::endUpdate()
{
    _updateEnd = Clock::now();
    _drawEnd = _updateEnd;
}

void FrameTimer::endDraw()
{
    _drawEnd = Clock::now();
}

std::size_t countAnimatedModels(ecs::Ecs &ecs, const engine::Scenes &scenes)
{
    const auto &models = ecs.cache<engine::Model>();
    const auto &animations = ecs.cache<engine::ModelAnimation>();
    std::size_t count = 0;
    for (ecs::entity_t entity = 0; entity < animations.size(); ++entity) {
        if (animations.has(entity) && models.has(entity) && scenes.isVisible(entity)) {
            ++count;
        }
    }
    return count;
}

} // namespace rtype::client
