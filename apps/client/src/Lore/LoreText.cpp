#include "Lore/LoreText.hpp"

#include <array>
#include <fstream>
#include <string_view>

namespace rtype::client
{

namespace
{

struct LorePrefix {
    std::string_view prefix;
    LoreLineKind kind;
};

/// The longest first, since "# " is the start of "## ".
constexpr std::array<LorePrefix, 3> LorePrefixes{{
    {"### ", LoreLineKind::Subheading},
    {"## ", LoreLineKind::Heading},
    {"# ", LoreLineKind::Title},
}};

[[nodiscard]] std::string_view trim(std::string_view text)
{
    const auto start = text.find_first_not_of(" \t\r");
    if (start == std::string_view::npos) {
        return {};
    }
    const auto end = text.find_last_not_of(" \t\r");
    return text.substr(start, end - start + 1);
}

[[nodiscard]] LoreLine readLoreLine(std::string_view line)
{
    const std::string_view text = trim(line);
    if (text.empty()) {
        return {LoreLineKind::Blank, {}};
    }
    for (const LorePrefix &prefix : LorePrefixes) {
        if (text.starts_with(prefix.prefix)) {
            return {prefix.kind, std::string(trim(text.substr(prefix.prefix.size())))};
        }
    }
    return {LoreLineKind::Paragraph, std::string(text)};
}

} // namespace

std::vector<LoreLine> parseLore(std::istream &text)
{
    std::vector<LoreLine> lines;
    std::string line;
    while (std::getline(text, line)) {
        lines.push_back(readLoreLine(line));
    }
    return lines;
}

std::vector<LoreLine> readLoreFile(const std::filesystem::path &path)
{
    std::ifstream file(path);
    if (!file) {
        return {};
    }
    return parseLore(file);
}

} // namespace rtype::client
