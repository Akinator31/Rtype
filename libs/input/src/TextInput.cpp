#include "rtype/input/TextInput.hpp"

#include "rtype/input/Binding.hpp"

#include <algorithm>
#include <utility>

namespace rtype::input
{

namespace
{

// Control characters (line feed, tabulation, escape...) are not text: they are never added to a field
bool isControl(char32_t character)
{
    return character < 0x20 || (character >= 0x7F && character <= 0x9F);
}

bool isValid(char32_t character)
{
    return character <= 0x10FFFF && (character < 0xD800 || character > 0xDFFF);
}

// Appends a code point to a UTF-8 string: 1 to 4 bytes depending on its value
void appendUtf8(std::string &text, char32_t character)
{
    const auto byte = [](char32_t value) { return static_cast<char>(value); };
    if (character < 0x80) {
        text += byte(character);
    } else if (character < 0x800) {
        text += byte(0xC0 | (character >> 6));
        text += byte(0x80 | (character & 0x3F));
    } else if (character < 0x10000) {
        text += byte(0xE0 | (character >> 12));
        text += byte(0x80 | ((character >> 6) & 0x3F));
        text += byte(0x80 | (character & 0x3F));
    } else {
        text += byte(0xF0 | (character >> 18));
        text += byte(0x80 | ((character >> 12) & 0x3F));
        text += byte(0x80 | ((character >> 6) & 0x3F));
        text += byte(0x80 | (character & 0x3F));
    }
}

// In UTF-8, the bytes after the first one of a character all start with the bits 10
bool isContinuationByte(char byte)
{
    return (static_cast<unsigned char>(byte) & 0xC0) == 0x80;
}

std::size_t countCharacters(const std::string &text)
{
    return static_cast<std::size_t>(
        std::ranges::count_if(text, [](char byte) { return !isContinuationByte(byte); }));
}

// Decodes UTF-8 text, for instance from the clipboard. Invalid bytes are skipped.
std::u32string decodeUtf8(const std::string &text)
{
    std::u32string result;
    std::size_t index = 0;
    while (index < text.size()) {
        const auto first = static_cast<unsigned char>(text[index]);
        const std::size_t size = first < 0x80           ? 1
                                 : (first >> 5) == 0x6  ? 2
                                 : (first >> 4) == 0xE  ? 3
                                 : (first >> 3) == 0x1E ? 4
                                                        : 0;
        if (size == 0 || index + size > text.size()) {
            ++index;
            continue;
        }
        char32_t character = size == 1 ? first : first & (0x7F >> size);
        bool valid = true;
        for (std::size_t offset = 1; offset < size; ++offset) {
            if (!isContinuationByte(text[index + offset])) {
                valid = false;
                break;
            }
            character = (character << 6) | (static_cast<unsigned char>(text[index + offset]) & 0x3F);
        }
        if (valid) {
            result += character;
            index += size;
        } else {
            ++index;
        }
    }
    return result;
}

bool isSpace(char byte)
{
    return byte == ' ';
}

} // namespace

TextInput::TextInput()
{
    _edits.bind(Edit::Left, Binding::key(Key::Left));
    _edits.bind(Edit::Right, Binding::key(Key::Right));
    _edits.bind(Edit::Home, Binding::key(Key::Home));
    _edits.bind(Edit::End, Binding::key(Key::End));
    _edits.bind(Edit::EraseBefore, Binding::key(Key::Backspace));
    _edits.bind(Edit::EraseAfter, Binding::key(Key::Delete));
    _edits.bind(Edit::Shift, Binding::key(Key::LeftShift));
    _edits.bind(Edit::Shift, Binding::key(Key::RightShift));
    _edits.bind(Edit::Control, Binding::key(Key::LeftControl));
    _edits.bind(Edit::Control, Binding::key(Key::RightControl));
    // Keys are physical positions: the A key of an AZERTY keyboard is where QWERTY keyboards have Q. Both are
    // bound so that Control+A selects everything on both. C, X and V are at the same place on both.
    _edits.bind(Edit::SelectAll, Binding::key(Key::A));
    _edits.bind(Edit::SelectAll, Binding::key(Key::Q));
    _edits.bind(Edit::Copy, Binding::key(Key::C));
    _edits.bind(Edit::Cut, Binding::key(Key::X));
    _edits.bind(Edit::Paste, Binding::key(Key::V));
}

void TextInput::update(InputSource &source, float elapsed)
{
    _changed = false;
    _edits.update(source, elapsed);
    const bool select = _edits.isActive(Edit::Shift);
    const bool control = _edits.isActive(Edit::Control);

    if (_edits.wasRepeated(Edit::Left)) {
        // Without Shift, Left on a selection goes to its start instead of moving
        const Range range = selection();
        moveCursor(!select && !range.empty() ? range.begin : previousPosition(_cursor, control), select);
    }
    if (_edits.wasRepeated(Edit::Right)) {
        const Range range = selection();
        moveCursor(!select && !range.empty() ? range.end : nextPosition(_cursor, control), select);
    }
    if (_edits.wasPressed(Edit::Home)) {
        moveCursor(0, select);
    }
    if (_edits.wasPressed(Edit::End)) {
        moveCursor(_text.size(), select);
    }
    if (_edits.wasRepeated(Edit::EraseBefore)) {
        erase(false, control);
    }
    if (_edits.wasRepeated(Edit::EraseAfter)) {
        erase(true, control);
    }
    if (control && _edits.wasPressed(Edit::SelectAll)) {
        selectAll();
    }
    // A secret never reaches the clipboard, where any program could read it
    if (control && !_secret && (_edits.wasPressed(Edit::Copy) || _edits.wasPressed(Edit::Cut)) &&
        !selection().empty()) {
        source.setClipboardText(selectedText());
        if (_edits.wasPressed(Edit::Cut)) {
            replaceSelection({});
        }
    }
    if (control && _edits.wasPressed(Edit::Paste)) {
        insert(decodeUtf8(source.clipboardText()));
    }
    // Characters are read even while Control is held: on Windows, AltGr is reported as Control + Alt, and it
    // types characters such as "€" or "@" on many layouts
    insert(source.takeTypedText());
}

TextInput::Range TextInput::selection() const
{
    return {std::min(_anchor, _cursor), std::max(_anchor, _cursor)};
}

std::string TextInput::selectedText() const
{
    const Range range = selection();
    return _text.substr(range.begin, range.end - range.begin);
}

void TextInput::moveCursor(std::size_t position, bool select)
{
    _cursor = position;
    if (!select) {
        _anchor = position;
    }
}

void TextInput::replaceSelection(const std::string &replacement)
{
    const Range range = selection();
    if (range.empty() && replacement.empty()) {
        return;
    }
    const std::string removed = _text.substr(range.begin, range.end - range.begin);
    _text.replace(range.begin, range.end - range.begin, replacement);
    _length = _length - countCharacters(removed) + countCharacters(replacement);
    _cursor = range.begin + replacement.size();
    _anchor = _cursor;
    _changed = true;
}

void TextInput::insert(const std::u32string &characters)
{
    if (characters.empty()) {
        return;
    }
    // The selection is replaced: its characters do not count in the room left
    const std::size_t remaining = _length - countCharacters(selectedText());
    std::string accepted;
    std::size_t count = 0;
    for (const char32_t character : characters) {
        if (isControl(character) || !isValid(character) || (_filter && !_filter(character))) {
            continue;
        }
        if (_maxLength != 0 && remaining + count >= _maxLength) {
            break;
        }
        appendUtf8(accepted, character);
        ++count;
    }
    if (!accepted.empty()) {
        replaceSelection(accepted);
    }
}

void TextInput::erase(bool forward, bool word)
{
    if (selection().empty()) {
        // Without selection, select what has to be erased: the character or the word next to the cursor
        _anchor = forward ? nextPosition(_cursor, word) : previousPosition(_cursor, word);
    }
    replaceSelection({});
}

std::size_t TextInput::previousPosition(std::size_t position, bool word) const
{
    if (!word) {
        // Step back over the continuation bytes to the first byte of the previous character
        while (position > 0) {
            --position;
            if (!isContinuationByte(_text[position])) {
                break;
            }
        }
        return position;
    }
    // A word: skip the spaces before the cursor, then the characters until the previous space
    while (position > 0 && isSpace(_text[position - 1])) {
        --position;
    }
    while (position > 0 && !isSpace(_text[position - 1])) {
        --position;
    }
    return position;
}

std::size_t TextInput::nextPosition(std::size_t position, bool word) const
{
    if (!word) {
        if (position < _text.size()) {
            ++position;
        }
        while (position < _text.size() && isContinuationByte(_text[position])) {
            ++position;
        }
        return position;
    }
    // A word: skip the characters until the next space, then the spaces
    while (position < _text.size() && !isSpace(_text[position])) {
        ++position;
    }
    while (position < _text.size() && isSpace(_text[position])) {
        ++position;
    }
    return position;
}

void TextInput::setText(std::string text)
{
    _text = std::move(text);
    _length = countCharacters(_text);
    _cursor = _text.size();
    _anchor = _cursor;
}

void TextInput::clear()
{
    setText({});
}

void TextInput::selectAll()
{
    _anchor = 0;
    _cursor = _text.size();
}

} // namespace rtype::input
