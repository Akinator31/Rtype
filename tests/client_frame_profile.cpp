#include "Diagnostics/FrameProfile.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using rtype::client::FrameProfile;
using rtype::client::FrameSample;

namespace
{

constexpr double usualFrame = 0.01;

/// Records frames of `usualFrame` until a second is over: the profile then knows the usual frame.
void recordUsualSecond(FrameProfile &profile)
{
    for (int frame = 0; frame < 100; ++frame) {
        profile.record({.total = usualFrame, .update = 0.002, .draw = 0.003, .present = 0.005});
    }
}

} // namespace

TEST_CASE("The frame profile reports each second, part by part", "[client][profile]")
{
    std::vector<std::string> lines;
    FrameProfile profile([&lines](const std::string &line) { lines.push_back(line); });
    for (int frame = 0; frame < 99; ++frame) {
        profile.record({.total = usualFrame});
    }
    CHECK(lines.empty());
    profile.record(
        {.total = usualFrame, .poses = {.poses = 4, .glowPoses = 2, .vertices = 1000, .seconds = 0.001}});
    REQUIRE(lines.size() == 1);
    CHECK(lines.front().starts_with("100 frames, 0 slow | frame 10.0 ms (longest 10.0)"));
    CHECK(lines.front().find("poses 0.0 a frame (4), glow 0.0 (2), 10 vertices (1000)") != std::string::npos);
}

TEST_CASE("The frame profile reports a frame twice as long as the usual ones", "[client][profile]")
{
    std::vector<std::string> lines;
    FrameProfile profile([&lines](const std::string &line) { lines.push_back(line); });
    recordUsualSecond(profile);
    lines.clear();

    profile.record({.total = 0.019});
    CHECK(lines.empty());
    profile.record({.total = 0.03,
                    .update = 0.002,
                    .draw = 0.025,
                    .present = 0.003,
                    .poses = {.poses = 9, .glowPoses = 3, .vertices = 500000, .seconds = 0.02},
                    .animatedModels = 7});
    REQUIRE(lines.size() == 1);
    CHECK(lines.front() ==
          "slow frame: 30.0 ms (usual 10.0 ms) | update 2.0 ms, draw 25.0 ms, present 3.0 ms | "
          "poses 9 (3 for the glow), 500000 vertices, 20.0 ms, 7 animated models");
}

TEST_CASE("A short frame is never slow, however short the usual ones", "[client][profile]")
{
    std::vector<std::string> lines;
    FrameProfile profile([&lines](const std::string &line) { lines.push_back(line); });
    for (int frame = 0; frame < 1000; ++frame) {
        profile.record({.total = 0.001});
    }
    lines.clear();
    profile.record({.total = FrameProfile::SlowAtLeast * 0.9});
    CHECK(lines.empty());
}

TEST_CASE("The frame profile starts again from nothing once reset", "[client][profile]")
{
    std::vector<std::string> lines;
    FrameProfile profile([&lines](const std::string &line) { lines.push_back(line); });
    recordUsualSecond(profile);
    profile.reset();
    lines.clear();
    profile.record({.total = 0.5});
    CHECK(lines.empty());
}
