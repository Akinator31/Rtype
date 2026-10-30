#pragma once

#include "rtype/input/ActionMap.hpp"
#include "rtype/input/InputSource.hpp"

#include <cstddef>
#include <functional>
#include <string>

namespace rtype::input
{

/// The text of an input field, such as a server address or a player name, edited with the keyboard.
///
/// It behaves like the text fields of common applications:
/// - typed characters are inserted at the cursor, and replace the selection if there is one;
/// - Left and Right move the cursor, Home and End go to the start and the end, Control jumps a word;
/// - holding Shift while moving the cursor selects text, Control+A selects everything;
/// - Backspace and Delete erase the selection, or the character before or after the cursor (a word with
///   Control);
/// - Control+C copies the selection, Control+X cuts it, Control+V pastes the clipboard.
///
/// Keys that move the cursor or erase repeat while they are held. The text is stored in UTF-8, ready to be
/// drawn; the cursor and the selection are given as byte offsets in it.
///
/// A secret input, for a password, never puts its text in the clipboard: Control+C and Control+X do nothing.
/// Pasting still works. Whoever draws it shows a mask instead of the text (engine::fieldContent()).
/// @code
/// TextInput address;
/// address.setMaxLength(21);
/// address.setFilter([](char32_t c) { return (c >= U'0' && c <= U'9') || c == U'.' || c == U':'; });
///
/// // Once per frame, while the field has the focus
/// address.update(devices, window.frameTime());
/// renderer.drawText(font, address.text(), position, 48.0f, colors::White);
/// @endcode
class TextInput
{
public:
    /// Decides whether a character can be typed or pasted in the field.
    using Filter = std::function<bool(char32_t character)>;

    /// A part of the text, as byte offsets in text(): from `begin` included to `end` excluded.
    struct Range {
        std::size_t begin = 0;
        std::size_t end = 0;

        /// Tells whether the range contains no text.
        [[nodiscard]] bool empty() const { return begin == end; }
    };

private:
    enum class Edit {
        Left,
        Right,
        Home,
        End,
        EraseBefore,
        EraseAfter,
        Shift,
        Control,
        SelectAll,
        Copy,
        Cut,
        Paste,
    };

    std::string _text;
    std::size_t _length = 0;
    std::size_t _cursor = 0;
    // The selection goes from the anchor to the cursor: without selection, both are at the same place
    std::size_t _anchor = 0;
    std::size_t _maxLength = 0;
    Filter _filter;
    ActionMap<Edit> _edits;
    bool _changed = false;
    bool _secret = false;

    void moveCursor(std::size_t position, bool select);
    void replaceSelection(const std::string &replacement);
    void insert(const std::u32string &characters);
    void erase(bool forward, bool word);
    [[nodiscard]] std::size_t previousPosition(std::size_t position, bool word) const;
    [[nodiscard]] std::size_t nextPosition(std::size_t position, bool word) const;

public:
    TextInput();

    /// Reads the keys and the characters typed since the previous frame. Call it once per frame while the
    /// field has the focus: characters typed meanwhile are lost, not delayed.
    /// @param source Gives the keys, the typed characters and the clipboard.
    /// @param elapsed Time since the previous frame, in seconds, to repeat the keys that are held.
    void update(InputSource &source, float elapsed);

    /// The text, in UTF-8.
    [[nodiscard]] const std::string &text() const { return _text; }
    /// Number of characters of the text, which can be less than its size in bytes.
    [[nodiscard]] std::size_t length() const { return _length; }
    /// Tells whether the text changed at the last update. Moving the cursor or selecting is not a change.
    [[nodiscard]] bool wasChanged() const { return _changed; }

    /// Position of the cursor, as a byte offset in text(): the text before the cursor is
    /// `text().substr(0, cursor())`, for instance to measure where to draw the cursor.
    [[nodiscard]] std::size_t cursor() const { return _cursor; }
    /// The selected part of the text, empty if nothing is selected.
    [[nodiscard]] Range selection() const;
    /// The selected text, in UTF-8.
    [[nodiscard]] std::string selectedText() const;

    /// Replaces the text, for instance with the value saved last time, and puts the cursor at its end. It is
    /// not filtered nor shortened.
    /// @param text Text in UTF-8.
    void setText(std::string text);
    /// Empties the text.
    void clear();
    /// Selects the whole text, for instance when the field gets the focus, so that typing replaces it.
    void selectAll();

    /// Sets the maximum number of characters: typed and pasted characters are ignored once it is reached. 0,
    /// the default, means no limit.
    void setMaxLength(std::size_t characters) { _maxLength = characters; }

    /// Sets which characters can be typed or pasted, on top of the control characters (line feed,
    /// tabulation...), which are always refused. By default, every other character is accepted.
    void setFilter(Filter filter) { _filter = std::move(filter); }

    /// Makes the text secret, for a password: it is never copied nor cut to the clipboard.
    void setSecret(bool secret) { _secret = secret; }
    /// Tells whether the text is secret: drawn as a mask, never copied.
    [[nodiscard]] bool isSecret() const { return _secret; }
};

} // namespace rtype::input
