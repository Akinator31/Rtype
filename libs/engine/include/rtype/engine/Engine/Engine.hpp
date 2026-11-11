#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Component/Toast.hpp"
#include "rtype/engine/Control/Controls.hpp"
#include "rtype/engine/Event/Event.hpp"
#include "rtype/engine/Quest/QuestSystem.hpp"
#include "rtype/engine/Scene/SceneAssetLoader.hpp"
#include "rtype/engine/Scene/SceneAssets.hpp"
#include "rtype/engine/Scene/SceneLoadState.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/engine/ThreadManager.hpp"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rtype::engine
{

/// Owns the ECS, the events (ADR 0008), the scenes (ADR 0009), the controls (the virtual gamepad) and the
/// save (the values kept from one run to the next).
///
/// At the start of each update, the engine destroys the entities of the scenes closed since the previous one,
/// whoever closed them (a scene request, a menu button calling Scenes::close(), Back).
///
/// Named scenes are declared once with createScene(), then opened by name: switchScene() replaces every open
/// scene, pushScene() opens one over them, popScene() closes the one at the top. Like a trigger, these
/// requests take effect at the start of the next update(). Each named scene has two events, created with it:
/// "<name>:open" and "<name>:close", triggered when the scene opens and closes.
/// @code
/// Engine engine;
/// engine.createScene("game", {.updatesWhenCovered = true}, [](Engine &engine, SceneId scene) {
///     const ecs::entity_t ship = engine.getEcs().createEntity();
///     engine.getEcs().addComponent<SceneMember>(ship, scene);
/// });
/// engine.subscribeToEvent("game:open", physics, Reaction::Enable);
/// engine.switchScene("game");
/// @endcode
class Engine
{
    /// The state to give back to a system after a Duration::Once reaction.
    struct Restore {
        rtype::ecs::System *system;
        bool enabled;
    };

    /// What a named scene is opened with.
    struct SceneDefinition {
        SceneSettings settings;
        std::function<void(Engine &, SceneId)> build;
    };

    enum class SceneChange { Switch, Push, Pop };

    struct SceneRequest {
        SceneChange change;
        std::string name;
    };

    /// A named scene opened by the engine, until it closes.
    struct NamedScene {
        SceneId id;
        std::string name;
        /// Acquired before it opened, released when it closes.
        std::vector<SceneAsset> assets;
    };

    /// A scene request waiting for the assets of its scene.
    struct PendingScene {
        SceneRequest request;
        std::vector<SceneAsset> assets{};
        SceneLoadState state = SceneLoadState::Loading;
        std::string error{};
    };

    rtype::ecs::Ecs _ecs;
    Scenes &_scenes;
    std::unordered_map<std::string, Event> _events;
    std::vector<Subscription> _pendingReactions;
    std::vector<Restore> _restores;
    std::unordered_map<std::string, SceneDefinition> _sceneDefinitions;
    std::vector<SceneRequest> _sceneRequests;
    std::vector<NamedScene> _namedScenes;
    Controls _controls;
    input::InputSource *_inputSource = nullptr;
    ThreadManager _threads;
    Settings _settings;
    SceneAssets _sceneAssets;
    std::string _loadingScene;
    std::optional<PendingScene> _pendingScene;
    /// The scenes with SceneSettings::keepsAssets opened once: the engine holds their assets for good.
    std::unordered_set<std::string> _scenesKeepingAssets;

    void applySceneRequests();
    void openScene(const std::string &name, std::vector<SceneAsset> assets = {});
    void beginSceneRequest(const SceneRequest &request);
    void finishPendingScene();
    void failPendingScene(std::string error);
    void applyReadyScene(const SceneRequest &request, std::vector<SceneAsset> assets);
    void cancelPendingScene();
    void closeLoadingScene();
    void destroyClosedScenes();
    void applyPendingReactions();
    void restoreOnceReactions();

public:
    /// Registers the Scenes system, first, as it must be.
    Engine();
    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;
    Engine(Engine &&) = delete;
    Engine &operator=(Engine &&) = delete;
    ~Engine() = default;

    [[nodiscard]] rtype::ecs::Ecs &getEcs();
    [[nodiscard]] Scenes &getScenes();
    [[nodiscard]] Controls &getControls();
    [[nodiscard]] ThreadManager &getThreads() { return _threads; }
    [[nodiscard]] const ThreadManager &getThreads() const { return _threads; }
    /// The settings start empty, without a file: the client gives them one with Settings::setPath(), then
    /// reads it.
    [[nodiscard]] Settings &getSettings();

    /// Lets the scenes declare assets (SceneSettings::assets). Before a scene with assets opens, the engine
    /// requests them from the loader and shows the loading scene until they are all loaded. An asset shared
    /// by several open scenes is loaded once, and unloaded when the last scene that needs it closes.
    /// @param loader Must outlive the engine, or unloadSceneAssets() must be called before it is destroyed.
    /// @param loadingScene Shown while the assets load. It must exist and have no assets.
    /// @throws std::invalid_argument if the loading scene does not exist or has assets.
    /// @throws std::logic_error if scene assets are already loaded or loading.
    void setSceneAssetLoader(SceneAssetLoader &loader, const std::string &loadingScene);
    /// Tells whether a scene waits for its assets, and whether one of them could not load.
    [[nodiscard]] SceneLoadState getSceneLoadState() const;
    /// Why an asset of the waiting scene could not load. Empty unless getSceneLoadState() is Failed.
    [[nodiscard]] const std::string &getSceneLoadError() const;
    /// Loads the assets of the waiting scene again. Does nothing unless getSceneLoadState() is Failed.
    void retrySceneLoad();
    /// Closes every scene and unloads their assets, those kept by SceneSettings::keepsAssets too: at
    /// shutdown, before the loader and what it loads with are destroyed.
    void unloadSceneAssets();

    /// Sets where the controls are read from, usually the real devices. It must outlive the engine. Without
    /// a source, the controls are never updated and stay released.
    void setInputSource(input::InputSource &source);

    /// Creates an event. If it already exists, returns it unchanged. The reference stays valid as
    /// long as the engine lives.
    Event &createEvent(const std::string &name);

    /// The event must exist (see hasEvent()), otherwise throws std::out_of_range.
    [[nodiscard]] Event &getEvent(const std::string &name);

    [[nodiscard]] bool hasEvent(const std::string &name) const;

    /// Subscribes a system to an event. The event must exist (see getEvent()).
    void subscribeToEvent(const std::string &name, rtype::ecs::System &system, Reaction reaction,
                          Duration duration = Duration::Permanent);

    /// Triggers an event: the reactions of its current subscribers are applied at the start of the next
    /// update(), in the order of the triggers. Does nothing if the event does not exist.
    void triggerEvent(const std::string &name);

    /// Makes a quest wait for its events: disabled, it runs at the update after one of them is triggered
    /// (Reaction::Enable, Duration::Once). Creates its events that do not exist yet. A quest already
    /// completed stays disabled, subscribed to nothing.
    void addQuest(QuestSystem &quest);

    /// Declares a named scene, and creates its events "<name>:open" and "<name>:close". If it already exists,
    /// keeps it unchanged.
    /// @param build Called each time the scene opens, with its id: it creates the entities of the scene, each
    /// with a SceneMember. May be empty.
    void createScene(const std::string &name, SceneSettings settings = {},
                     std::function<void(Engine &, SceneId)> build = {});

    [[nodiscard]] bool hasScene(const std::string &name) const;

    /// Tells whether a named scene is open. Requests take effect at the next update(): the scene is not open
    /// yet right after switchScene() or pushScene(), and still open right after popScene().
    [[nodiscard]] bool isSceneOpen(const std::string &name) const;

    /// At the next update(), closes every open scene, then opens this one. The scene must exist (see
    /// hasScene()), otherwise throws std::out_of_range.
    void switchScene(const std::string &name);
    /// At the next update(), opens this scene over the others. The scene must exist, otherwise throws
    /// std::out_of_range.
    void pushScene(const std::string &name);
    /// At the next update(), closes the scene at the top, if any.
    void popScene();

    /// Shows a short message in a corner of the screen for a few seconds, over every scene, even after a
    /// scene switch. Its look and timing come from InterfaceStyle::toast; it needs a ToastSystem.
    /// @code
    /// engine.showToast("Settings saved");
    /// engine.showToast("Connection lost", ToastType::Error);
    /// @endcode
    /// @return The entity of the toast, destroyed once it has faded out.
    ecs::entity_t showToast(const std::string &text, ToastType type = ToastType::Info);
    /// Shows a toast that reads as a line: the subject, the message, the actor, then the icon of its type.
    /// @code
    /// engine.showToast("VORTEX", "has joined the room", "", ToastType::Info);
    /// engine.showToast("ORBITAL MODULE", "taken by", "KAZE", ToastType::Success);
    /// @endcode
    ecs::entity_t showToast(const std::string &subject, const std::string &message, const std::string &actor,
                            ToastType type = ToastType::Info);
    /// Shows an announcement, centred at the top of the screen: a title with an icon, a line of light, and a
    /// line of detail. It has the timing of the toasts, and stacks with the other announcements.
    /// @code
    /// engine.showAnnouncement("PLAYER 2 JOINED", "Room - 2 / 4 pilots");
    /// @endcode
    ecs::entity_t showAnnouncement(const std::string &title, const std::string &detail,
                                   ToastType type = ToastType::Info);
    /// Shows a banner across the whole screen, for what the player must not miss: a title and a detail.
    /// @param duration Time it stays fully shown, in seconds: ToastStyle::duration when empty.
    /// @code
    /// engine.showBanner("BOSS APPROACHING", "GET THE BEAM READY", ToastType::Error);
    /// engine.showBanner("WAVE 03", "BYDOS INCOMING", ToastType::Info, 1.5f);
    /// @endcode
    ecs::entity_t showBanner(const std::string &title, const std::string &detail,
                             ToastType type = ToastType::Info, std::optional<float> duration = std::nullopt);

    /// Runs one frame: updates the controls from the input source, if any, so that the whole frame reads the
    /// same state; destroys the entities of the closed scenes and applies the scene requests (which
    /// triggers the events of the scenes opened and closed), then the pending reactions, updates the ECS, and
    /// gives back their previous state to the systems changed by a Duration::Once reaction. A permanent
    /// reaction cancels that restore.
    /// @param elapsed Time since the previous update, in seconds, given to every system.
    void update(float elapsed);
};

} // namespace rtype::engine
