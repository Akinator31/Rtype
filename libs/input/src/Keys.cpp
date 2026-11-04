#include "rtype/input/Keys.hpp"

#include <algorithm>
#include <array>
#include <ranges>
#include <utility>

namespace rtype::input
{

namespace
{

// Every key with its name, in the order of Key
constexpr std::array<std::pair<Key, std::string_view>, 66> names{{
    {Key::A, "A"},
    {Key::B, "B"},
    {Key::C, "C"},
    {Key::D, "D"},
    {Key::E, "E"},
    {Key::F, "F"},
    {Key::G, "G"},
    {Key::H, "H"},
    {Key::I, "I"},
    {Key::J, "J"},
    {Key::K, "K"},
    {Key::L, "L"},
    {Key::M, "M"},
    {Key::N, "N"},
    {Key::O, "O"},
    {Key::P, "P"},
    {Key::Q, "Q"},
    {Key::R, "R"},
    {Key::S, "S"},
    {Key::T, "T"},
    {Key::U, "U"},
    {Key::V, "V"},
    {Key::W, "W"},
    {Key::X, "X"},
    {Key::Y, "Y"},
    {Key::Z, "Z"},
    {Key::Num0, "Num0"},
    {Key::Num1, "Num1"},
    {Key::Num2, "Num2"},
    {Key::Num3, "Num3"},
    {Key::Num4, "Num4"},
    {Key::Num5, "Num5"},
    {Key::Num6, "Num6"},
    {Key::Num7, "Num7"},
    {Key::Num8, "Num8"},
    {Key::Num9, "Num9"},
    {Key::Up, "Up"},
    {Key::Down, "Down"},
    {Key::Left, "Left"},
    {Key::Right, "Right"},
    {Key::Space, "Space"},
    {Key::Enter, "Enter"},
    {Key::Escape, "Escape"},
    {Key::Tab, "Tab"},
    {Key::Backspace, "Backspace"},
    {Key::Delete, "Delete"},
    {Key::Home, "Home"},
    {Key::End, "End"},
    {Key::LeftShift, "LeftShift"},
    {Key::RightShift, "RightShift"},
    {Key::LeftControl, "LeftControl"},
    {Key::RightControl, "RightControl"},
    {Key::LeftAlt, "LeftAlt"},
    {Key::RightAlt, "RightAlt"},
    {Key::F1, "F1"},
    {Key::F2, "F2"},
    {Key::F3, "F3"},
    {Key::F4, "F4"},
    {Key::F5, "F5"},
    {Key::F6, "F6"},
    {Key::F7, "F7"},
    {Key::F8, "F8"},
    {Key::F9, "F9"},
    {Key::F10, "F10"},
    {Key::F11, "F11"},
    {Key::F12, "F12"},
}};

// keyName() finds a name by the value of its key
static_assert(std::ranges::all_of(std::views::iota(std::size_t{0}, names.size()),
                                  [](std::size_t index) {
                                      return names[index].first == static_cast<Key>(index);
                                  }),
              "the names must follow the order of Key");

constexpr std::array<Key, names.size()> keys = [] {
    std::array<Key, names.size()> list{};
    std::ranges::transform(names, list.begin(), [](const auto &entry) { return entry.first; });
    return list;
}();

} // namespace

std::span<const Key> allKeys()
{
    return keys;
}

std::string_view keyName(Key key)
{
    return names[static_cast<std::size_t>(key)].second;
}

std::optional<Key> keyFromName(std::string_view name)
{
    const auto found = std::ranges::find(names, name, &std::pair<Key, std::string_view>::second);
    if (found == names.end()) {
        return std::nullopt;
    }
    return found->first;
}

} // namespace rtype::input
