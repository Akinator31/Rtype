#pragma once

#include <cstdint>

namespace rtype::render
{

/// Identifier of a loaded resource, cheap to copy and store in components.
///
/// The tag type only gives each kind of resource its own handle type, so that a texture handle cannot be
/// passed where a font handle is expected.
template <typename Tag>
struct Handle {
    std::uint32_t index;

    friend bool operator==(Handle, Handle) = default;
};

} // namespace rtype::render
