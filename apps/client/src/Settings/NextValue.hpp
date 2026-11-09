#pragma once

#include <algorithm>
#include <iterator>

namespace rtype::client
{

/// The value after this one in a list, back to the first after the last, and the first for a value not in it:
/// what a row of the Settings page shows when Right is pressed. The list must not be empty.
template <typename Values, typename Value>
[[nodiscard]] Value nextValue(const Values &values, const Value &current)
{
    const auto found = std::ranges::find(values, current);
    if (found == std::ranges::end(values) || std::next(found) == std::ranges::end(values)) {
        return *std::ranges::begin(values);
    }
    return *std::next(found);
}

/// The value before this one in a list, back to the last before the first, and the first for a value not in
/// it: what a row of the Settings page shows when Left is pressed. The list must not be empty.
template <typename Values, typename Value>
[[nodiscard]] Value previousValue(const Values &values, const Value &current)
{
    const auto found = std::ranges::find(values, current);
    if (found == std::ranges::end(values)) {
        return *std::ranges::begin(values);
    }
    if (found == std::ranges::begin(values)) {
        return *std::prev(std::ranges::end(values));
    }
    return *std::prev(found);
}

/// The next value for a direction of +1, the previous one for -1.
template <typename Values, typename Value>
[[nodiscard]] Value stepValue(const Values &values, const Value &current, int direction)
{
    return direction < 0 ? previousValue(values, current) : nextValue(values, current);
}

} // namespace rtype::client
