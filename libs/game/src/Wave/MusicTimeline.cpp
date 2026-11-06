#include "Wave/MusicTimeline.hpp"

#include "rtype/beat/Analysis/BeatDetector.hpp"
#include "rtype/beat/Decoder/WavDecoder.hpp"
#include "rtype/game/Constants.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <random>
#include <stdexcept>

namespace rtype::game
{

namespace
{

/// The enemies come in this many horizontal lanes.
constexpr std::uint32_t LaneCount = 5;
/// A beat does not use the lane of one of this many beats before it.
constexpr std::size_t RecentLanes = 2;

beat::BeatGaps gapsOf(const WaveDefinition &wave)
{
    beat::BeatGaps gaps{};
    for (std::size_t band = 0; band < gaps.size(); ++band) {
        gaps[band] = wave.beats[band].minGap;
    }
    return gaps;
}

/// The same seed for the same wave on every machine: the positions are part of the wave, not of the game.
std::uint32_t seedOf(const std::string &name)
{
    std::uint32_t hash = 2166136261u;
    for (const char letter : name) {
        hash = (hash ^ static_cast<std::uint8_t>(letter)) * 16777619u;
    }
    return hash;
}

class Placer
{
    const WaveDefinition &_wave;
    std::mt19937 _random;
    std::array<std::uint32_t, RecentLanes> _recent{};
    std::size_t _recentCount = 0;

    [[nodiscard]] std::uint32_t between(std::uint32_t first, std::uint32_t last)
    {
        return first + static_cast<std::uint32_t>(_random() % (last - first + 1));
    }

    [[nodiscard]] float between(float min, float max)
    {
        constexpr std::uint32_t Steps = 1000;
        return min + (max - min) * static_cast<float>(between(0, Steps)) / static_cast<float>(Steps);
    }

    [[nodiscard]] bool isRecent(std::uint32_t lane) const
    {
        return std::find(_recent.begin(), _recent.begin() + static_cast<std::ptrdiff_t>(_recentCount),
                         lane) != _recent.begin() + static_cast<std::ptrdiff_t>(_recentCount);
    }

    void remember(std::uint32_t lane)
    {
        _recent[1] = _recent[0];
        _recent[0] = lane;
        _recentCount = std::min(_recentCount + 1, RecentLanes);
    }

    [[nodiscard]] std::uint32_t pickLane()
    {
        std::uint32_t lane = between(0, LaneCount - 1);
        while (isRecent(lane)) {
            lane = (lane + 1) % LaneCount;
        }
        remember(lane);
        return lane;
    }

public:
    explicit Placer(const WaveDefinition &wave) : _wave(wave), _random(seedOf(wave.name)) {}

    /// The kind of enemy for a band, never twice in a row when it has several.
    [[nodiscard]] const std::string &pickEnemy(const WaveBeatRule &rule, std::size_t &last)
    {
        const auto count = static_cast<std::uint32_t>(rule.enemies.size());
        std::uint32_t index = between(0, count - 1);
        if (count > 1 && index == last) {
            index = (index + 1) % count;
        }
        last = index;
        return rule.enemies[index];
    }

    /// A position in a lane of the area, in the middle of the lane plus a jitter.
    [[nodiscard]] WaveSpawn place(const std::string &enemy)
    {
        const WaveArea &area = _wave.area;
        const float laneHeight = (area.maxY - area.minY) / static_cast<float>(LaneCount);
        const float laneTop = area.minY + laneHeight * static_cast<float>(pickLane());
        return {enemy, between(area.minX, area.maxX),
                between(laneTop + laneHeight * 0.2f, laneTop + laneHeight * 0.8f)};
    }
};

std::uint32_t toTick(float seconds)
{
    return static_cast<std::uint32_t>(std::lround(seconds * static_cast<float>(TickRate)));
}

} // namespace

void fillMusicTimeline(WaveDefinition &wave, const std::filesystem::path &assets)
{
    const beat::MonoSound sound = beat::decodeSound(assets / wave.music);
    const std::vector<beat::Beat> beats = beat::detectBeats(sound, gapsOf(wave));
    Placer placer(wave);
    std::array<std::size_t, 3> lastEnemy{std::numeric_limits<std::size_t>::max(),
                                         std::numeric_limits<std::size_t>::max(),
                                         std::numeric_limits<std::size_t>::max()};
    wave.timeline.clear();
    for (const beat::Beat &found : beats) {
        const auto band = static_cast<std::size_t>(found.band);
        const WaveBeatRule &rule = wave.beats[band];
        // The enemies that would have to come before the music starts are not sent
        if (rule.enemies.empty() || found.time < wave.lead) {
            continue;
        }
        const std::string &enemy = placer.pickEnemy(rule, lastEnemy[band]);
        wave.timeline.push_back({toTick(found.time - wave.lead), placer.place(enemy)});
    }
    if (wave.timeline.empty()) {
        throw std::runtime_error(std::format("{}: no beat found for the bands used", wave.music));
    }
    if (wave.timeline.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error(std::format("{}: too many beats, raise a minGap", wave.music));
    }
    wave.quantity = static_cast<std::uint16_t>(wave.timeline.size());
}

} // namespace rtype::game
