#include "rtype/render/FontCharacters.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace rtype::render
{

namespace
{

/// Reads the big-endian numbers of a font file, telling when they are out of it.
class FontReader
{
    const std::vector<unsigned char> &_file;

public:
    explicit FontReader(const std::vector<unsigned char> &file) : _file(file) {}

    [[nodiscard]] std::optional<std::uint16_t> read16(std::size_t offset) const
    {
        if (offset + 2 > _file.size()) {
            return std::nullopt;
        }
        return static_cast<std::uint16_t>((_file[offset] << 8) | _file[offset + 1]);
    }

    [[nodiscard]] std::optional<std::uint32_t> read32(std::size_t offset) const
    {
        const std::optional<std::uint16_t> high = read16(offset);
        const std::optional<std::uint16_t> low = read16(offset + 2);
        if (!high || !low) {
            return std::nullopt;
        }
        return (static_cast<std::uint32_t>(*high) << 16) | *low;
    }
};

/// Where the table of characters starts in the file.
std::optional<std::size_t> findCmapTable(const FontReader &reader)
{
    const std::optional<std::uint16_t> tableCount = reader.read16(4);
    if (!tableCount) {
        return std::nullopt;
    }
    for (std::size_t table = 0; table < *tableCount; ++table) {
        const std::size_t record = 12 + (16 * table);
        if (reader.read32(record) == 0x636D6170) { // "cmap"
            return reader.read32(record + 8);
        }
    }
    return std::nullopt;
}

/// The subtable of the table of characters for Unicode, in format 12 (every character) or 4 (the first
/// 65536), the most complete first.
std::optional<std::size_t> findUnicodeSubtable(const FontReader &reader, std::size_t cmap)
{
    const std::optional<std::uint16_t> count = reader.read16(cmap + 2);
    if (!count) {
        return std::nullopt;
    }
    std::optional<std::size_t> format4;
    for (std::size_t index = 0; index < *count; ++index) {
        const std::size_t record = cmap + 4 + (8 * index);
        const std::optional<std::uint16_t> platform = reader.read16(record);
        const std::optional<std::uint32_t> offset = reader.read32(record + 4);
        // Unicode (0) or Windows (3) platforms
        if (!platform || !offset || (*platform != 0 && *platform != 3)) {
            continue;
        }
        const std::size_t subtable = cmap + *offset;
        const std::optional<std::uint16_t> format = reader.read16(subtable);
        if (format == 12) {
            return subtable;
        }
        if (format == 4 && !format4) {
            format4 = subtable;
        }
    }
    return format4;
}

/// Whether a format 12 subtable has a glyph for the character: groups of consecutive characters.
std::optional<bool> hasCharacterInFormat12(const FontReader &reader, std::size_t subtable, int character)
{
    const std::optional<std::uint32_t> groups = reader.read32(subtable + 12);
    if (!groups) {
        return std::nullopt;
    }
    const auto code = static_cast<std::uint32_t>(character);
    for (std::size_t group = 0; group < *groups; ++group) {
        const std::size_t record = subtable + 16 + (12 * group);
        const std::optional<std::uint32_t> first = reader.read32(record);
        const std::optional<std::uint32_t> last = reader.read32(record + 4);
        if (!first || !last) {
            return std::nullopt;
        }
        if (code >= *first && code <= *last) {
            return true;
        }
    }
    return false;
}

/// Whether a format 4 subtable has a glyph for the character: segments of characters, whose glyph is found
/// by an offset or in an array, glyph 0 meaning none.
std::optional<bool> hasCharacterInFormat4(const FontReader &reader, std::size_t subtable, int character)
{
    const std::optional<std::uint16_t> doubleSegments = reader.read16(subtable + 6);
    if (!doubleSegments || character > 0xFFFF) {
        return doubleSegments ? std::optional<bool>(false) : std::nullopt;
    }
    const std::size_t ends = subtable + 14;
    const std::size_t starts = ends + *doubleSegments + 2;
    const std::size_t deltas = starts + *doubleSegments;
    const std::size_t rangeOffsets = deltas + *doubleSegments;
    const auto code = static_cast<std::uint16_t>(character);
    for (std::size_t segment = 0; segment < *doubleSegments / 2u; ++segment) {
        const std::optional<std::uint16_t> end = reader.read16(ends + (2 * segment));
        const std::optional<std::uint16_t> start = reader.read16(starts + (2 * segment));
        const std::optional<std::uint16_t> delta = reader.read16(deltas + (2 * segment));
        const std::optional<std::uint16_t> rangeOffset = reader.read16(rangeOffsets + (2 * segment));
        if (!end || !start || !delta || !rangeOffset) {
            return std::nullopt;
        }
        if (code > *end) {
            continue;
        }
        if (code < *start) {
            return false;
        }
        if (*rangeOffset == 0) {
            return static_cast<std::uint16_t>(code + *delta) != 0;
        }
        const std::size_t glyphAddress = rangeOffsets + (2 * segment) + *rangeOffset + (2u * (code - *start));
        const std::optional<std::uint16_t> glyph = reader.read16(glyphAddress);
        if (!glyph) {
            return std::nullopt;
        }
        return *glyph != 0 && static_cast<std::uint16_t>(*glyph + *delta) != 0;
    }
    return false;
}

} // namespace

std::vector<int> filterCharactersInFont(const std::vector<unsigned char> &file,
                                        const std::vector<int> &wanted)
{
    const FontReader reader(file);
    const std::optional<std::size_t> cmap = findCmapTable(reader);
    const std::optional<std::size_t> subtable = cmap ? findUnicodeSubtable(reader, *cmap) : std::nullopt;
    if (!subtable) {
        return wanted;
    }
    const bool isFormat12 = reader.read16(*subtable) == 12;
    std::vector<int> found;
    for (const int character : wanted) {
        const std::optional<bool> has = isFormat12 ? hasCharacterInFormat12(reader, *subtable, character)
                                                   : hasCharacterInFormat4(reader, *subtable, character);
        if (!has) {
            return wanted;
        }
        if (*has) {
            found.push_back(character);
        }
    }
    return found;
}

} // namespace rtype::render
