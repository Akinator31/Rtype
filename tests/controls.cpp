// The virtual gamepad of the engine, driven by a fake input source.

#include "FakeInput.hpp"
#include "rtype/engine/Control/Controls.hpp"
#include "rtype/engine/Engine/Engine.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using rtype::engine::Control;
using rtype::engine::Controls;
using rtype::engine::Engine;
using rtype::input::AxisDirection;
using rtype::input::Binding;
using rtype::input::GamepadAxis;
using rtype::input::GamepadButton;
using rtype::input::Key;

TEST_CASE("The keyboard and the gamepad drive the same controls by default", "[engine][controls]")
{
    FakeInput input;
    Controls controls;

    input.keys = {Key::Space};
    controls.update(input);
    CHECK(controls.isDown(Control::FaceDown));

    input.keys.clear();
    input.buttons = {{0, GamepadButton::FaceDown}};
    controls.update(input);
    CHECK(controls.isDown(Control::FaceDown));

    input.buttons = {{0, GamepadButton::DPadUp}};
    controls.update(input);
    CHECK(controls.isDown(Control::Up));

    input.buttons.clear();
    input.keys = {Key::W};
    controls.update(input);
    CHECK(controls.isDown(Control::Up));

    input.keys = {Key::Escape};
    controls.update(input);
    CHECK(controls.isDown(Control::Start));
    CHECK_FALSE(controls.isDown(Control::Up));
}

TEST_CASE("A control is pressed and released for one update only", "[engine][controls]")
{
    FakeInput input;
    Controls controls;

    input.keys = {Key::Space};
    controls.update(input);
    CHECK(controls.wasPressed(Control::FaceDown));

    controls.update(input, 0.5f);
    CHECK(controls.isDown(Control::FaceDown));
    CHECK_FALSE(controls.wasPressed(Control::FaceDown));
    CHECK(controls.getHeldDuration(Control::FaceDown) == Catch::Approx(0.5f));

    input.keys.clear();
    controls.update(input);
    CHECK(controls.wasReleased(Control::FaceDown));
    CHECK_FALSE(controls.isDown(Control::FaceDown));

    controls.update(input);
    CHECK_FALSE(controls.wasReleased(Control::FaceDown));
}

TEST_CASE("The direction is digital on the keyboard and analog on the stick", "[engine][controls]")
{
    FakeInput input;
    Controls controls;

    controls.update(input);
    CHECK(controls.getDirection().x == 0.0f);
    CHECK(controls.getDirection().y == 0.0f);

    input.keys = {Key::Right, Key::Up};
    controls.update(input);
    CHECK(controls.getDirection().x == 1.0f);
    CHECK(controls.getDirection().y == -1.0f);

    // Left and Right together cancel each other
    input.keys = {Key::Right, Key::Left};
    controls.update(input);
    CHECK(controls.getDirection().x == 0.0f);

    input.keys.clear();
    input.axes = {{{0, GamepadAxis::LeftX}, -1.0f}, {{0, GamepadAxis::LeftY}, 0.1f}};
    controls.update(input);
    CHECK(controls.getDirection().x == -1.0f);
    // Inside the dead zone
    CHECK(controls.getDirection().y == 0.0f);

    controls.setDeadZone(0.0f);
    controls.update(input);
    CHECK(controls.getDirection().y == Catch::Approx(0.1f));
    CHECK(controls.getStrength(Control::Down) == Catch::Approx(0.1f));
}

TEST_CASE("Controls can be remapped", "[engine][controls]")
{
    FakeInput input;
    Controls controls;

    controls.clear(Control::FaceDown);
    CHECK(controls.getBindings(Control::FaceDown).empty());
    controls.bind(Control::FaceDown, Binding::key(Key::K));

    input.keys = {Key::Space};
    controls.update(input);
    CHECK_FALSE(controls.isDown(Control::FaceDown));

    input.keys = {Key::K};
    controls.update(input);
    CHECK(controls.isDown(Control::FaceDown));

    controls.unbind(Control::FaceDown, Binding::key(Key::K));
    controls.update(input);
    CHECK_FALSE(controls.isDown(Control::FaceDown));
    CHECK(controls.getBindings(Control::FaceDown).empty());
}

TEST_CASE("The engine updates its controls from its input source", "[engine][controls]")
{
    FakeInput input;
    Engine engine;

    // Without a source, the controls stay released
    engine.update(0.0f);
    CHECK_FALSE(engine.getControls().isDown(Control::FaceDown));

    engine.setInputSource(input);
    input.buttons = {{0, GamepadButton::FaceDown}};
    engine.update(0.0f);
    CHECK(engine.getControls().wasPressed(Control::FaceDown));

    input.axes = {{{0, GamepadAxis::RightTrigger}, 1.0f}};
    input.buttons.clear();
    engine.update(0.0f);
    CHECK(engine.getControls().wasReleased(Control::FaceDown));
    CHECK(engine.getControls().isDown(Control::RightTrigger));
}

TEST_CASE("Binding the same input twice has no effect", "[engine][controls]")
{
    Controls controls;
    const std::size_t count = controls.getBindings(Control::Up).size();
    controls.bind(Control::Up, Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Negative));
    CHECK(controls.getBindings(Control::Up).size() == count);
}
