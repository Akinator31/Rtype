#pragma once

#include "Lore/LoreText.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <array>
#include <string>
#include <vector>

namespace rtype::client
{

/// Look of a kind of line of the lore, at its natural size: on the crawl, where it is at the bottom of the
/// screen.
struct LoreLineStyle {
    render::FontId font;
    float size = 50.0f;
    /// Space added between two letters, in game units.
    float spacing = 0.0f;
};

/// Layout of the lore on the crawl.
struct LoreStyle {
    /// In the order of LoreLineKind. Only the size of Blank is used: an empty line is as high as a line of
    /// text of this size.
    std::array<LoreLineStyle, 5> kinds;
    /// Width of the column of text, in game units: the paragraphs are justified on it.
    float width = 1280.0f;
    /// Height of a line, as a factor of the size of its text.
    float lineHeight = 1.35f;
};

/// A text of the lore placed on the crawl: a word of a paragraph, or a whole line of a title or a heading.
struct LorePiece {
    std::string text;
    LoreLineKind kind = LoreLineKind::Paragraph;
    /// Where its centre is at its natural size, in game units: from the axis of the crawl to the right, and
    /// from the top of the text downwards.
    float x = 0.0f;
    float distance = 0.0f;
};

/// The lore placed on the crawl.
struct LoreLayout {
    std::vector<LorePiece> pieces;
    /// Height of the whole text, in game units.
    float length = 0.0f;
};

/// Places the lines of the lore one below the other. The paragraphs are cut into lines as wide as the column
/// and justified, like a column of a newspaper: one piece per word, so that the space between them can grow.
/// Their last line is on the left, with plain spaces. The other lines are centred and cut the same way if
/// they are wider than the column, one piece per line.
///
/// A colon, a semicolon, an exclamation or a question mark that stands alone, as French writes it after a
/// space, stays with the word before it.
[[nodiscard]] LoreLayout layOutLore(const std::vector<LoreLine> &lines, const LoreStyle &style,
                                    const engine::Metrics &metrics);

} // namespace rtype::client
