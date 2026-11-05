#pragma once

#include <algorithm>
#include <iterator>

namespace rtype::client
{

/// The value after this one in a list, back to the first after the last, and the first for a value not in it:
/// what a button of the Settings page shows when it is pressed. The list must not be empty.
template <typename Values, typename Value>
[[nodiscard]] Value nextValue(const Values &values, const Value &current)
{
    const auto found = std::ranges::find(values, current);
    if (found == std::ranges::end(values) || std::next(found) == std::ranges::end(values)) {
        return *std::ranges::begin(values);
    }
    return *std::next(found);
}

} // namespace rtype::client
