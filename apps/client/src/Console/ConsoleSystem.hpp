#pragma once

#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/input/InputSource.hpp"
#include "rtype/input/Keys.hpp"

namespace rtype::client
{

/// Opens the console scene over the others when F8 is pressed, and closes it when F8 is pressed again.
///
/// It reads the key itself, whatever scene receives the input, so that the console opens from anywhere.
class ConsoleSystem : public ecs::System
{
    engine::Engine &_engine;
    input::InputSource &_input;
    bool _keyWasDown = false;

    [[nodiscard]] bool wasKeyPressed();
    void toggleConsole();

public:
    static constexpr input::Key toggleKey = input::Key::F8;

    /// @param engine, input They must outlive the system.
    ConsoleSystem(ecs::ComponentCaches &caches, engine::Engine &engine, input::InputSource &input);

    void update(float elapsed) override;
};

} // namespace rtype::client
