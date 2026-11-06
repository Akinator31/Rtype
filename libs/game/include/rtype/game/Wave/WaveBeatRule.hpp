#pragma once

#include <string>
#include <vector>

namespace rtype::game
{

/// What a Music wave sends at each beat of one band of its music.
struct WaveBeatRule {
    /// The ids of EnemyDefinitions: one of them, drawn in turn, comes at each beat. Empty: the band is ignored.
    std::vector<std::string> enemies;
    /// Seconds: no two beats of the band closer than this, to keep a busy band playable.
    float minGap = 0.25f;
};

} // namespace rtype::game
