// The TextInput is tested with a fake input source, whose typed text is set by the test.

#include "FakeInput.hpp"
#include "rtype/input/TextInput.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <utility>

using rtype::input::Key;
using rtype::input::TextInput;

TEST_CASE("Typed characters are added at the end of the text", "[input][text]")
{
    TextInput input;
    FakeInput source;

    source.typed = U"R-Ty";
    input.update(source, 0.0f);
    source.typed = U"pe";
    input.update(source, 0.0f);

    REQUIRE(input.text() == "R-Type");
    REQUIRE(input.length() == 6);
    REQUIRE(input.wasChanged());
}

TEST_CASE("Accented characters are stored in UTF-8", "[input][text]")
{
    TextInput input;
    FakeInput source;

    source.typed = U"Élodie €";
    input.update(source, 0.0f);

    REQUIRE(input.text() == "\xC3\x89lodie \xE2\x82\xAC");
    REQUIRE(input.length() == 8);
}

TEST_CASE("Backspace removes the last character, even when it takes several bytes", "[input][text]")
{
    TextInput input;
    input.setText("caf\xC3\xA9");
    FakeInput source;
    REQUIRE(input.length() == 4);

    source.keys = {Key::Backspace};
    input.update(source, 0.0f);

    REQUIRE(input.text() == "caf");
    REQUIRE(input.length() == 3);
}

TEST_CASE("Backspace repeats while it is held", "[input][text]")
{
    TextInput input;
    input.setText("abcdef");
    FakeInput source;
    source.keys = {Key::Backspace};

    input.update(source, 0.0f);
    REQUIRE(input.text() == "abcde");
    input.update(source, 0.2f);
    REQUIRE(input.text() == "abcde");

    // Default repeat: after 0.4 s, then every 0.1 s
    input.update(source, 0.2f);
    REQUIRE(input.text() == "abcd");
    input.update(source, 0.1f);
    REQUIRE(input.text() == "abc");
}

TEST_CASE("Backspace on an empty text does nothing", "[input][text]")
{
    TextInput input;
    FakeInput source;
    source.keys = {Key::Backspace};

    input.update(source, 0.0f);

    REQUIRE(input.text().empty());
    REQUIRE(input.length() == 0);
    REQUIRE_FALSE(input.wasChanged());
}

TEST_CASE("Characters beyond the maximum length are ignored", "[input][text]")
{
    TextInput input;
    input.setMaxLength(3);
    FakeInput source;

    source.typed = U"abcdef";
    input.update(source, 0.0f);

    REQUIRE(input.text() == "abc");
}

TEST_CASE("The filter decides which characters can be typed", "[input][text]")
{
    TextInput input;
    input.setFilter(
        [](char32_t character) { return (character >= U'0' && character <= U'9') || character == U'.'; });
    FakeInput source;

    source.typed = U"127.0.a0.1";
    input.update(source, 0.0f);

    REQUIRE(input.text() == "127.0.0.1");
}

TEST_CASE("Control characters are never added", "[input][text]")
{
    TextInput input;
    FakeInput source;

    source.typed = U"a\tb\nc\x1B";
    input.update(source, 0.0f);

    REQUIRE(input.text() == "abc");
}

TEST_CASE("The text is unchanged when nothing is typed", "[input][text]")
{
    TextInput input;
    input.setText("Player");
    FakeInput source;

    input.update(source, 0.016f);

    REQUIRE(input.text() == "Player");
    REQUIRE_FALSE(input.wasChanged());
}

namespace
{

// Presses the keys for one update, then releases them for one update, like a quick key press
void press(TextInput &input, FakeInput &source, std::set<Key> keys)
{
    source.keys = std::move(keys);
    input.update(source, 0.0f);
    source.keys.clear();
    input.update(source, 0.0f);
}

} // namespace

TEST_CASE("Characters are inserted at the cursor", "[input][text]")
{
    TextInput input;
    input.setText("Rype");
    FakeInput source;

    press(input, source, {Key::Home});
    press(input, source, {Key::Right});
    source.typed = U"-T";
    input.update(source, 0.0f);

    REQUIRE(input.text() == "R-Type");
    REQUIRE(input.cursor() == 3);
}

TEST_CASE("The cursor moves over whole characters", "[input][text]")
{
    TextInput input;
    input.setText("\xC3\xA9t\xC3\xA9");
    FakeInput source;

    press(input, source, {Key::Left});
    REQUIRE(input.cursor() == 3);
    press(input, source, {Key::Left});
    REQUIRE(input.cursor() == 2);
    press(input, source, {Key::Left});
    REQUIRE(input.cursor() == 0);
    press(input, source, {Key::Left});
    REQUIRE(input.cursor() == 0);
}

TEST_CASE("Delete erases the character after the cursor", "[input][text]")
{
    TextInput input;
    input.setText("abc");
    FakeInput source;

    press(input, source, {Key::Home});
    press(input, source, {Key::Delete});

    REQUIRE(input.text() == "bc");
    REQUIRE(input.cursor() == 0);
}

TEST_CASE("Control jumps and erases whole words", "[input][text]")
{
    TextInput input;
    input.setText("hello big world");
    FakeInput source;

    press(input, source, {Key::LeftControl, Key::Left});
    REQUIRE(input.cursor() == 10);
    press(input, source, {Key::LeftControl, Key::Left});
    REQUIRE(input.cursor() == 6);

    press(input, source, {Key::LeftControl, Key::Backspace});
    REQUIRE(input.text() == "big world");

    press(input, source, {Key::End});
    press(input, source, {Key::LeftControl, Key::Backspace});
    REQUIRE(input.text() == "big ");
}

TEST_CASE("Shift selects text while moving the cursor", "[input][text]")
{
    TextInput input;
    input.setText("R-Type");
    FakeInput source;

    press(input, source, {Key::LeftShift, Key::Left});
    press(input, source, {Key::LeftShift, Key::Left});
    press(input, source, {Key::LeftShift, Key::Left});
    press(input, source, {Key::LeftShift, Key::Left});

    REQUIRE(input.selectedText() == "Type");
    REQUIRE(input.selection().begin == 2);
    REQUIRE(input.selection().end == 6);

    press(input, source, {Key::Left});
    REQUIRE(input.selection().empty());
    REQUIRE(input.cursor() == 2);
}

TEST_CASE("Typing replaces the selection", "[input][text]")
{
    TextInput input;
    input.setText("R-Type");
    FakeInput source;

    press(input, source, {Key::LeftShift, Key::Home});
    source.typed = U"Gradius";
    input.update(source, 0.0f);

    REQUIRE(input.text() == "Gradius");
}

TEST_CASE("Backspace erases the selection", "[input][text]")
{
    TextInput input;
    input.setText("R-Type");
    FakeInput source;

    press(input, source, {Key::LeftShift, Key::Left});
    press(input, source, {Key::LeftShift, Key::Left});
    press(input, source, {Key::Backspace});

    REQUIRE(input.text() == "R-Ty");
}

TEST_CASE("Control+A selects everything, on QWERTY and AZERTY keyboards", "[input][text]")
{
    FakeInput source;

    TextInput qwerty;
    qwerty.setText("R-Type");
    press(qwerty, source, {Key::LeftControl, Key::A});
    REQUIRE(qwerty.selectedText() == "R-Type");

    // The A key of an AZERTY keyboard is where QWERTY keyboards have Q
    TextInput azerty;
    azerty.setText("R-Type");
    press(azerty, source, {Key::RightControl, Key::Q});
    REQUIRE(azerty.selectedText() == "R-Type");
}

TEST_CASE("Control+C copies the selection without changing the text", "[input][text]")
{
    TextInput input;
    input.setText("127.0.0.1");
    FakeInput source;

    press(input, source, {Key::LeftControl, Key::A});
    press(input, source, {Key::LeftControl, Key::C});

    REQUIRE(source.clipboard == "127.0.0.1");
    REQUIRE(input.text() == "127.0.0.1");
}

TEST_CASE("Control+C without selection leaves the clipboard unchanged", "[input][text]")
{
    TextInput input;
    input.setText("abc");
    FakeInput source;
    source.clipboard = "kept";

    press(input, source, {Key::LeftControl, Key::C});

    REQUIRE(source.clipboard == "kept");
}

TEST_CASE("Control+X cuts the selection", "[input][text]")
{
    TextInput input;
    input.setText("R-Type");
    FakeInput source;

    press(input, source, {Key::LeftShift, Key::Home});
    press(input, source, {Key::LeftControl, Key::X});

    REQUIRE(source.clipboard == "R-Type");
    REQUIRE(input.text().empty());
}

TEST_CASE("A secret text is never copied nor cut to the clipboard, but can be pasted", "[input][text]")
{
    TextInput input;
    input.setSecret(true);
    input.setText("hunter2hunter2");
    FakeInput source;
    source.clipboard = "kept";

    press(input, source, {Key::LeftControl, Key::A});
    press(input, source, {Key::LeftControl, Key::C});
    CHECK(source.clipboard == "kept");
    press(input, source, {Key::LeftControl, Key::X});
    CHECK(source.clipboard == "kept");
    CHECK(input.text() == "hunter2hunter2");

    press(input, source, {Key::LeftControl, Key::V});
    CHECK(input.text() == "kept");
}

TEST_CASE("Control+V pastes the clipboard at the cursor, filtered", "[input][text]")
{
    TextInput input;
    input.setFilter(
        [](char32_t character) { return (character >= U'0' && character <= U'9') || character == U'.'; });
    input.setMaxLength(9);
    FakeInput source;
    // Copied from a web page: with a line feed and a space around the address, and too long
    source.clipboard = " 127.0.0.1\n2 ";

    press(input, source, {Key::LeftControl, Key::V});

    REQUIRE(input.text() == "127.0.0.1");
}

TEST_CASE("Pasted accented text is kept", "[input][text]")
{
    TextInput input;
    FakeInput source;
    source.clipboard = "\xC3\x89lodie";

    press(input, source, {Key::LeftControl, Key::V});

    REQUIRE(input.text() == "\xC3\x89lodie");
    REQUIRE(input.length() == 6);
}

TEST_CASE("The maximum length takes into account the replaced selection", "[input][text]")
{
    TextInput input;
    input.setMaxLength(4);
    input.setText("abcd");
    FakeInput source;

    press(input, source, {Key::LeftShift, Key::Left});
    source.typed = U"xyz";
    input.update(source, 0.0f);

    REQUIRE(input.text() == "abcx");
}

TEST_CASE("Moving the cursor is not a change of the text", "[input][text]")
{
    TextInput input;
    input.setText("abc");
    FakeInput source;

    source.keys = {Key::Left};
    input.update(source, 0.0f);

    REQUIRE(input.cursor() == 2);
    REQUIRE_FALSE(input.wasChanged());
}
