#include "Controls/KeyCaptureSystem.hpp"

#include "Settings/ControlSettings.hpp"

#include <algorithm>
#include <iterator>

namespace rtype::client
{

KeyCaptureSystem::KeyCaptureSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, const engine::Scenes &scenes,
                                   const input::InputSource &input)
    : _scenes(scenes), _input(input), _ecs(ecs), _captures(use<KeyCapture>(caches))
{
}

std::vector<input::Key> KeyCaptureSystem::heldKeys() const
{
    std::vector<input::Key> keys;
    std::ranges::copy_if(input::allKeys(), std::back_inserter(keys),
                         [this](input::Key key) { return _input.isKeyDown(key); });
    return keys;
}

void KeyCaptureSystem::capture(ecs::entity_t entity, KeyCapture &capture)
{
    std::vector<input::Key> held = heldKeys();
    const auto pressed = std::ranges::find_if(held, [&capture](input::Key key) {
        return key != CancelKey && std::ranges::find(capture.held, key) == capture.held.end();
    });
    if (capture.started && pressed != held.end()) {
        _ecs.defer([onKey = capture.onKey, key = *pressed] { onKey(key); });
        _ecs.removeComponent<KeyCapture>(entity);
        return;
    }
    capture.held = std::move(held);
    capture.started = true;
}

void KeyCaptureSystem::update(float /*elapsed*/)
{
    for (ecs::entity_t entity = 0; entity < _captures.size(); ++entity) {
        if (_captures.has(entity) && _scenes.receivesInput(entity)) {
            capture(entity, _captures.get(entity));
        }
    }
}

} // namespace rtype::client
