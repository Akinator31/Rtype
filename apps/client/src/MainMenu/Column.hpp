#pragma once

#include "MainMenu/Layout.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include <functional>
#include <string>
#include <utility>

namespace rtype::client
{

/// The top of the first button of a submenu, under its header.
inline constexpr float columnTop = 260.0f;

/// A button in the column on the left of a submenu whose text is written from a function every frame, so that
/// it follows the language and the value it shows.
inline ecs::entity_t boundButton(engine::InterfaceBuilder &interface, ecs::Ecs &ecs,
                                 std::function<std::string()> label, std::function<void()> onPress, float top)
{
    const ecs::entity_t button =
        interface.button(label(), std::move(onPress), {engine::AnchorPoint::TopLeft, screenMargin, top});
    ecs.addComponent<engine::BoundText>(button, std::move(label));
    return button;
}

} // namespace rtype::client
