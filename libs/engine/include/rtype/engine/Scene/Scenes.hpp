#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Entity/Entity.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Scene/SceneAsset.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace rtype::engine
{

/// Identifies an open scene. Ids are never reused. A type of its own, so that a scene and an entity cannot be
/// confused.
struct SceneId {
    std::uint32_t value = 0;

    friend bool operator==(const SceneId &, const SceneId &) = default;
};

/// Makes an entity part of a scene: the engine destroys it when the scene closes. Entities without it are
/// persistent: they survive every scene change (a star-field shared by the menus and the game, for instance).
struct SceneMember {
    SceneId scene;
};

/// How a scene behaves when other scenes are opened over it.
struct SceneSettings {
    /// The scene below stays visible, like a pause menu over the game. Otherwise, it hides what is below.
    bool overlay = false;
    /// With overlay, only the world of the scenes below stays visible, not their interface: a submenu over
    /// the 3D background of the main menu, without the buttons of the main menu.
    bool hidesInterfaceBelow = false;
    /// The scene keeps updating when another scene is over it, like a network game under the pause menu.
    bool updatesWhenCovered = false;
    /// Called when the player goes back (Escape) while the scene is at the top. Empty: the scene closes.
    std::function<void()> onBack{};
    /// Loaded before the scene opens, while the loading scene is shown: see Engine::setSceneAssetLoader().
    std::vector<SceneAsset> assets{};
};

/// The stack of open scenes: home screen, game, pause menu over the game...
///
/// Every entity of a scene has a SceneMember. Scenes only keeps the stack: closing a scene removes it, and
/// the Engine destroys its entities at the start of its next update (see takeClosedScenes()). Until then,
/// they are neither visible nor updated. Systems ask the scenes what to do with an entity: receivesInput(),
/// updates(), isVisible().
///
/// The Engine registers it as the first system: at the start of each update, it records which scene receives
/// the input for the whole update. Otherwise the Escape press that closes the pause menu would, later in the
/// same update, reach the game and open the menu again.
/// @code
/// Scenes &scenes = engine.getScenes();
/// const SceneId game = scenes.open({.updatesWhenCovered = true});
/// const ecs::entity_t ship = engine.getEcs().createEntity();
/// engine.getEcs().addComponent<SceneMember>(ship, game);
/// @endcode
class Scenes : public ecs::System
{
    struct Entry {
        SceneId id;
        SceneSettings settings;
    };

    ecs::ComponentCache<SceneMember> &_members;
    std::vector<Entry> _stack;
    std::vector<SceneId> _closed;
    std::uint32_t _nextId = 1;
    std::optional<SceneId> _inputOwner;

    [[nodiscard]] const Entry *find(SceneId scene) const;

public:
    explicit Scenes(ecs::ComponentCaches &caches);

    /// Records which scene receives the input during this update.
    void update(float elapsed) override;

    /// Opens a scene over the others and returns its id.
    SceneId open(SceneSettings settings = {});
    /// Closes the scene at the top. Does nothing if no scene is open.
    void close();
    /// Closes a scene wherever it is in the stack.
    void close(SceneId scene);
    /// Closes every scene, from the top.
    void clear();
    /// Goes back from the scene at the top: calls its onBack, or closes it if it has none.
    void back();

    /// The scenes closed since the previous call, in closing order, whose entities must be destroyed. Called
    /// by the Engine at the start of its update.
    [[nodiscard]] std::vector<SceneId> takeClosedScenes();

    /// The scene at the top, if any.
    [[nodiscard]] std::optional<SceneId> top() const;
    /// Tells whether a scene is open.
    [[nodiscard]] bool isOpen(SceneId scene) const;

    /// Tells whether a scene receives the input: it was at the top at the start of the update.
    [[nodiscard]] bool receivesInput(SceneId scene) const;
    /// Tells whether a scene updates: it is at the top, or it keeps updating when covered.
    [[nodiscard]] bool updates(SceneId scene) const;
    /// Tells whether a scene is visible: it is at the top, or only overlays are over it.
    [[nodiscard]] bool isVisible(SceneId scene) const;
    /// Tells whether the interface of a scene is visible: the scene is visible, and no scene above it hides
    /// the interface below.
    [[nodiscard]] bool isInterfaceVisible(SceneId scene) const;

    /// The scene of an entity, or nothing for a persistent entity.
    [[nodiscard]] std::optional<SceneId> sceneOf(ecs::entity_t entity) const;
    /// Tells whether an entity receives the input: persistent entities never do.
    [[nodiscard]] bool receivesInput(ecs::entity_t entity) const;
    /// Tells whether an entity updates: persistent entities always do.
    [[nodiscard]] bool updates(ecs::entity_t entity) const;
    /// Tells whether an entity is visible according to its scene: persistent entities always are.
    [[nodiscard]] bool isVisible(ecs::entity_t entity) const;
    /// Tells whether an entity of the interface layer is visible according to its scene: persistent entities
    /// always are.
    [[nodiscard]] bool isInterfaceVisible(ecs::entity_t entity) const;
};

} // namespace rtype::engine
