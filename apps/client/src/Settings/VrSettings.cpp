#include "Settings/VrSettings.hpp"

#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace rtype::client
{

namespace
{

/// The value offered nearest to the one kept, or the default if there is none or it is not a number.
template <typename Value, std::size_t Count>
Value readNearest(const engine::Settings &settings, std::string_view key,
                  const std::array<Value, Count> &offered, Value fallback)
{
    const std::optional<float> value = settings.getFloat(key);
    if (!value || !std::isfinite(*value)) {
        return fallback;
    }
    return *std::ranges::min_element(offered, [&value](Value first, Value second) {
        return std::abs(static_cast<float>(first) - *value) < std::abs(static_cast<float>(second) - *value);
    });
}

} // namespace

std::string_view mirrorName(VrMirror mirror)
{
    switch (mirror) {
    case VrMirror::FlatGame:
        return "flat_game";
    case VrMirror::Nothing:
        return "nothing";
    case VrMirror::LeftEye:
        break;
    }
    return "left_eye";
}

std::optional<VrMirror> mirrorFromName(std::string_view name)
{
    const auto found = std::ranges::find(VrMirrors, name, mirrorName);
    if (found == VrMirrors.end()) {
        return std::nullopt;
    }
    return *found;
}

VrPreferences readVrSettings(const engine::Settings &settings)
{
    VrPreferences preferences;
    preferences.enabled = settings.getBool(VrSettingsKey).value_or(preferences.enabled);
    preferences.flatBoard = settings.getBool(VrFlatBoardKey).value_or(preferences.flatBoard);
    preferences.boardDistance =
        readNearest(settings, VrBoardDistanceKey, BoardDistances, preferences.boardDistance);
    preferences.boardWidth = readNearest(settings, VrBoardWidthKey, BoardWidths, preferences.boardWidth);
    preferences.seatHeight = readNearest(settings, VrSeatHeightKey, SeatHeights, preferences.seatHeight);
    preferences.snapTurn = readNearest(settings, VrSnapTurnKey, SnapTurns, preferences.snapTurn);
    if (const std::optional<std::string> mirror = settings.getString(VrMirrorKey)) {
        if (const std::optional<VrMirror> known = mirrorFromName(*mirror)) {
            preferences.mirror = *known;
        } else {
            log::Logger("settings")
                .warning("Unknown VR mirror \"{}\" in the settings: {} is kept", *mirror,
                         mirrorName(preferences.mirror));
        }
    }
    return preferences;
}

void keepVrSettings(engine::Settings &settings, const VrPreferences &preferences)
{
    settings.setBool(VrSettingsKey, preferences.enabled);
    settings.setBool(VrFlatBoardKey, preferences.flatBoard);
    settings.setFloat(VrBoardDistanceKey, preferences.boardDistance);
    settings.setFloat(VrBoardWidthKey, preferences.boardWidth);
    settings.setFloat(VrSeatHeightKey, preferences.seatHeight);
    settings.setString(VrMirrorKey, mirrorName(preferences.mirror));
    settings.setInt(VrSnapTurnKey, preferences.snapTurn);
}

VrLayout layoutOf(const VrPreferences &preferences)
{
    VrLayout layout;
    layout.boardDistance = preferences.boardDistance;
    layout.boardWidth = preferences.boardWidth;
    layout.seatHeight = preferences.seatHeight;
    return layout;
}

} // namespace rtype::client
