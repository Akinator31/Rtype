#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/input/InputSource.hpp"

#include <functional>
#include <vector>

namespace rtype::client
{

/// Waits for the player to press a key, on an entity of the scene that asks for one (the choice of a key on
/// the Controls page). Removed from its entity once the key is given.
struct KeyCapture {
    /// Called with the key pressed. CancelKey is never given: the scene closes on it, as on Back.
    std::function<void(input::Key)> onKey;
    /// The keys held at the previous frame: a key already held when the capture starts, such as the one that
    /// opened it, is not taken.
    std::vector<input::Key> held{};
    bool started = false;
};

/// Gives each KeyCapture of the scene that receives the input the first key pressed.
class KeyCaptureSystem : public ecs::System
{
    const engine::Scenes &_scenes;
    const input::InputSource &_input;
    ecs::Ecs &_ecs;
    ecs::ComponentCache<KeyCapture> &_captures;

    [[nodiscard]] std::vector<input::Key> heldKeys() const;
    void capture(ecs::entity_t entity, KeyCapture &capture);

public:
    /// @param scenes, input, ecs They must outlive the system.
    KeyCaptureSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, const engine::Scenes &scenes,
                     const input::InputSource &input);

    void update(float elapsed) override;
};

} // namespace rtype::client
