#include "Settings/ControlSettings.hpp"

#include <optional>
#include <string>

namespace rtype::client
{

namespace
{

std::string settingsKey(const GameAction &action)
{
    return std::string(ControlsSettingsPrefix).append(action.name);
}

/// Binds these keys to the control instead of its keyboard keys; its gamepad inputs stay.
void replaceKeys(engine::Controls &controls, engine::Control control, const std::vector<input::Key> &keys)
{
    for (const input::Key key : keysOf(controls, control)) {
        controls.unbind(control, input::Binding::key(key));
    }
    for (const input::Key key : keys) {
        controls.bind(control, input::Binding::key(key));
    }
}

/// "Up,W": the names of the keys, separated by commas.
std::string joinKeys(const std::vector<input::Key> &keys)
{
    std::string text;
    for (const input::Key key : keys) {
        text.append(text.empty() ? "" : ",").append(input::keyName(key));
    }
    return text;
}

/// The keys of a text written by joinKeys(), or nothing if one of them does not exist. An empty text is no
/// key.
std::optional<std::vector<input::Key>> splitKeys(std::string_view text)
{
    std::vector<input::Key> keys;
    while (!text.empty()) {
        const std::size_t comma = text.find(',');
        const std::optional<input::Key> key = input::keyFromName(text.substr(0, comma));
        if (!key) {
            return std::nullopt;
        }
        keys.push_back(*key);
        text = comma == std::string_view::npos ? std::string_view{} : text.substr(comma + 1);
    }
    return keys;
}

} // namespace

std::vector<input::Key> keysOf(const engine::Controls &controls, engine::Control control)
{
    std::vector<input::Key> keys;
    for (const input::Binding &binding : controls.getBindings(control)) {
        if (const std::optional<input::Key> key = binding.asKey()) {
            keys.push_back(*key);
        }
    }
    return keys;
}

void assignKey(engine::Controls &controls, engine::Control control, input::Key key)
{
    for (const GameAction &action : GameActions) {
        controls.unbind(action.control, input::Binding::key(key));
    }
    replaceKeys(controls, control, {key});
}

void applyControlSettings(const engine::Settings &settings, engine::Controls &controls)
{
    for (const GameAction &action : GameActions) {
        const std::optional<std::string> kept = settings.getString(settingsKey(action));
        if (const auto keys = kept ? splitKeys(*kept) : std::nullopt) {
            replaceKeys(controls, action.control, *keys);
        }
    }
}

void keepControlSettings(engine::Settings &settings, const engine::Controls &controls)
{
    for (const GameAction &action : GameActions) {
        settings.setString(settingsKey(action), joinKeys(keysOf(controls, action.control)));
    }
}

} // namespace rtype::client
