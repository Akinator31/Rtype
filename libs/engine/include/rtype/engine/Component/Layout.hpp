#pragma once

namespace rtype::engine
{

/// Stacks the children of the entity in a column centred on it: the widgets of a menu.
struct Column {
    /// Space between two children, in game units.
    float spacing = 16.0f;
};

/// Place of a child in its Column, from the top. The entity must have a Parent (the column) and a Hitbox.
struct ColumnItem {
    int index = 0;
};

} // namespace rtype::engine
