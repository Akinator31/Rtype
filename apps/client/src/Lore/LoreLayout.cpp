#include "Lore/LoreLayout.hpp"

#include <array>
#include <cstddef>
#include <sstream>
#include <string_view>

namespace rtype::client
{

namespace
{

/// A word of a line, and its width once drawn.
struct LoreWord {
    std::string text;
    float width = 0.0f;
};

using LoreRow = std::vector<LoreWord>;

/// The marks written after a space in French, which must not start a line.
constexpr std::array<std::string_view, 4> TrailingMarks{":", ";", "!", "?"};

[[nodiscard]] bool isTrailingMark(std::string_view word)
{
    for (const std::string_view mark : TrailingMarks) {
        if (word == mark) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::vector<std::string> splitWords(const std::string &text)
{
    std::vector<std::string> words;
    std::istringstream stream(text);
    std::string word;
    while (stream >> word) {
        if (isTrailingMark(word) && !words.empty()) {
            words.back() += " " + word;
        } else {
            words.push_back(word);
        }
    }
    return words;
}

[[nodiscard]] float measureWidth(const std::string &text, const LoreLineStyle &style,
                                 const engine::Metrics &metrics)
{
    return metrics.measureText(style.font, text, style.size, style.spacing).x;
}

[[nodiscard]] float measureWords(const LoreRow &row)
{
    float width = 0.0f;
    for (const LoreWord &word : row) {
        width += word.width;
    }
    return width;
}

/// Cuts the text into rows no wider than `width`, with `space` between two words. A word wider than the
/// column has a row of its own.
[[nodiscard]] std::vector<LoreRow> wrapWords(const std::string &text, const LoreLineStyle &style, float width,
                                             float space, const engine::Metrics &metrics)
{
    std::vector<LoreRow> rows;
    LoreRow row;
    float rowWidth = 0.0f;
    for (std::string &word : splitWords(text)) {
        const float wordWidth = measureWidth(word, style, metrics);
        const float grown = row.empty() ? wordWidth : rowWidth + space + wordWidth;
        if (!row.empty() && grown > width) {
            rows.push_back(std::move(row));
            row.clear();
            rowWidth = wordWidth;
        } else {
            rowWidth = grown;
        }
        row.push_back({std::move(word), wordWidth});
    }
    if (!row.empty()) {
        rows.push_back(std::move(row));
    }
    return rows;
}

/// The words of a row of a paragraph, one piece each, from the left of the column: spread over its whole
/// width if `justified`, else with plain spaces.
void placeParagraphRow(LoreLayout &layout, const LoreRow &row, bool justified, float width, float space,
                       float distance)
{
    const float gap = (justified && row.size() > 1)
                          ? (width - measureWords(row)) / static_cast<float>(row.size() - 1)
                          : space;
    float left = -width / 2.0f;
    for (const LoreWord &word : row) {
        layout.pieces.push_back({word.text, LoreLineKind::Paragraph, left + (word.width / 2.0f), distance});
        left += word.width + gap;
    }
}

/// A row of a title or a heading, as one piece centred on the axis.
void placeCentredRow(LoreLayout &layout, const LoreRow &row, LoreLineKind kind, float distance)
{
    std::string text;
    for (const LoreWord &word : row) {
        text += (text.empty() ? "" : " ") + word.text;
    }
    layout.pieces.push_back({std::move(text), kind, 0.0f, distance});
}

void placeLine(LoreLayout &layout, const LoreLine &line, const LoreStyle &style,
               const engine::Metrics &metrics)
{
    const LoreLineStyle &look = style.kinds[static_cast<std::size_t>(line.kind)];
    const float height = look.size * style.lineHeight;
    if (line.kind == LoreLineKind::Blank) {
        layout.length += height;
        return;
    }
    const float space = measureWidth(" ", look, metrics);
    const std::vector<LoreRow> rows = wrapWords(line.text, look, style.width, space, metrics);
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const float distance = layout.length + (height / 2.0f);
        if (line.kind == LoreLineKind::Paragraph) {
            placeParagraphRow(layout, rows[index], index + 1 < rows.size(), style.width, space, distance);
        } else {
            placeCentredRow(layout, rows[index], line.kind, distance);
        }
        layout.length += height;
    }
}

} // namespace

LoreLayout layOutLore(const std::vector<LoreLine> &lines, const LoreStyle &style,
                      const engine::Metrics &metrics)
{
    LoreLayout layout;
    for (const LoreLine &line : lines) {
        placeLine(layout, line, style, metrics);
    }
    return layout;
}

} // namespace rtype::client
