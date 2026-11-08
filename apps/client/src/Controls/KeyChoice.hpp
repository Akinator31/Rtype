#pragma once

#include "Settings/ControlSettings.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/input/Keys.hpp"
#include "rtype/localization/Localization.hpp"

#include <memory>
#include <optional>
#include <string>

namespace rtype::client
{

/// The choice of a key for an action, shared by a page of keys (Controls, Voice) and the scene over it that
/// asks for the key.
struct KeyChoice {
    /// The action whose key is being chosen, or nothing.
    const GameAction *action = nullptr;
    /// The key pressed, while it waits for a confirmation because another action has it.
    std::optional<input::Key> key{};
    /// That other action.
    const GameAction *owner = nullptr;
    /// What the banner of the choice takes the place of, hidden while a key is chosen: the button of the
    /// default keys of the Controls page. Nothing if the banner has a place of its own.
    std::optional<ecs::entity_t> hidden{};
};
using SharedKeyChoice = std::shared_ptr<KeyChoice>;

/// Where the rows of the actions of a page go, in game units: the left of the rows, their width, and where
/// their keys and their gamepad buttons start from their left.
struct KeyTable {
    float left = 0.0f;
    float width = 0.0f;
    float keysColumn = 0.0f;
    float buttonsColumn = 0.0f;
};

/// The key of the name of an action in the translations: "controls.fire".
[[nodiscard]] std::string actionTextKey(const GameAction &action);

/// The name of a key for the player: the character printed on it on their layout ("Z" for Key::W on AZERTY),
/// or its translated name for a key without one ("Espace").
[[nodiscard]] std::string keyText(const localization::Localization &translations, input::Key key);

/// The row of an action, at `top`: its name, its keys and its gamepad buttons. Pressing it opens the scene
/// `chooseScene` over the page (createKeyChoiceScene()) to choose its key: the row then shows the key waited
/// for, or the key chosen, and the row of the action that has it shows it in red; the rows of the other
/// actions dim.
/// @param translations, action They must outlive the row.
ecs::entity_t addActionRow(engine::InterfaceBuilder &interface, engine::Engine &engine,
                           const localization::Localization &translations, const GameAction &action,
                           const SharedKeyChoice &choice, const KeyTable &table, const char *chooseScene,
                           float top);

/// Declares the scene over a page of keys that asks for the key of the action of `choice`: a banner, its top
/// left at `left` and `top`, says "New key for: Fire", and the first key pressed goes to the action, unless
/// another action has it. The banner then says so: Enter gives it to the action, another key is tried
/// instead, Escape keeps the keys as they were (see assignKey() and findKeyOwner()). The page stays visible
/// under it.
/// @param style, translations They must outlive the engine.
void createKeyChoiceScene(engine::Engine &engine, const char *name, const engine::InterfaceStyle &style,
                          const localization::Localization &translations, const SharedKeyChoice &choice,
                          float left, float top);

} // namespace rtype::client
