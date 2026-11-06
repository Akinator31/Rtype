#pragma once

#include "rtype/game/Wave/WaveDefinition.hpp"

#include <filesystem>
#include <vector>

namespace rtype::game
{

/// Analyses the music of a Music wave, found under `assets` at the path `wave.music`, and fills its `timeline`
/// and `quantity`: an enemy of the rule of the band at each beat of the band, `wave.lead` seconds before it,
/// at a position that changes from one beat to the next. Throws std::runtime_error if the music cannot be
/// read, or holds no beat the wave uses.
void fillMusicTimeline(WaveDefinition &wave, const std::filesystem::path &assets);

} // namespace rtype::game
