#pragma once

#include "rtype/render/Types.hpp"

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

/// A point of the screen, or of an entity: a corner, the middle of a side, or the centre.
enum class AnchorPoint {
    TopLeft,
    Top,
    TopRight,
    Left,
    Center,
    Right,
    BottomLeft,
    Bottom,
    BottomRight,
};

/// Places the entity on the screen: its own point `point` is put on the same point of the screen, then moved
/// by the offset. With TopLeft and an offset of {20, 20}, the top left corner of the entity is 20 units from
/// the top left corner of the screen; with BottomRight, use {-20, -20}. Computed by the AnchorSystem from the
/// size of what the entity draws, so the entity must not have a Parent.
struct Anchor {
    AnchorPoint point = AnchorPoint::Center;
    /// Offset, in game units: x to the right, y downwards.
    float x = 0.0f;
    float y = 0.0f;
};

/// The size of an entity that draws itself without a Text, Sprite, Box or Hitbox (a gauge), so that it can be
/// placed by an Anchor.
struct Extent {
    float width = 0.0f;
    float height = 0.0f;
};

/// Where the point is, from 0 (left or top) to 1 (right or bottom), in each direction.
[[nodiscard]] render::Vector2 getAnchorFraction(AnchorPoint point);

/// Centre of an element of this size placed by the anchor on the screen, as the AnchorSystem places it.
[[nodiscard]] render::Vector2 computeAnchoredPosition(render::Rectangle screen, const Anchor &anchor,
                                                      render::Vector2 size);

} // namespace rtype::engine
