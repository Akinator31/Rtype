#pragma once

#include <filesystem>
#include <istream>
#include <string>
#include <vector>

namespace rtype::client
{

/// What a line of the lore is, which gives its look in the crawl. In the order of LoreStyle::kinds.
enum class LoreLineKind {
    /// "# " at the start of the line: the title of the story, the largest.
    Title,
    /// "## ": the name of a chapter.
    Heading,
    /// "### ": a small line above a title or a heading, such as the year of a chapter.
    Subheading,
    /// Any other line: a paragraph, cut into lines as wide as the crawl and justified.
    Paragraph,
    /// An empty line: a space as high as a line of a paragraph.
    Blank,
};

/// A line of the lore file, without its prefix.
struct LoreLine {
    LoreLineKind kind = LoreLineKind::Paragraph;
    std::string text;
};

/// Reads the layout of the lore, written in a text file in UTF-8: one block per line, its kind given by its
/// prefix (see LoreLineKind). The spaces around the text of a line are removed.
[[nodiscard]] std::vector<LoreLine> parseLore(std::istream &text);

/// Reads a lore file with parseLore().
/// @return Nothing if the file cannot be read.
[[nodiscard]] std::vector<LoreLine> readLoreFile(const std::filesystem::path &path);

} // namespace rtype::client
