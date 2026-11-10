#pragma once

#include "Vr/CockpitLook.hpp"
#include "Vr/SceneFade.hpp"
#include "Vr/VrLayout.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"
#include "rtype/xr/Headset.hpp"

#include <array>
#include <functional>
#include <future>
#include <optional>
#include <vector>

namespace rtype::client
{

/// What an eye sees, as the headset gives it, in the types of the renderer.
[[nodiscard]] render::EyeView toRenderEye(const xr::EyeView &eye);

/// What the scenes shown give to the headset each frame (engine::SceneRenderer::light() and sky()).
struct VrScene {
    /// The light of the scene, which lights the cockpit.
    render::Light light;
    /// The picture at the back of its sky, drawn around the cockpit; none leaves space plain.
    std::optional<engine::Backdrop> sky;
    /// A game is played: the player sits in the cockpit, the HUD on its screens. In the menus, the board
    /// floats alone in the sky.
    bool playing = false;
    /// Something is shown on the panel (engine::SceneRenderer::hasContent()): a menu, the pause. The panel
    /// unfolds for it, and folds back once it is gone.
    bool panel = false;
    /// The scene at the bottom of those open (engine::Scenes::bottom()): when it changes, the scenes were
    /// switched, and the view fades from one to the other.
    std::optional<engine::SceneId> scenes;
};

/// What the player sees in the headset (docs/architecture/vr.md): seated in the cockpit, the sky of the scene
/// around them, the board outside the canopy with the world of the game drawn into it, the HUD on the screens
/// of the cockpit, and the menus on a holographic panel that unfolds from the hologram, between the dashboard
/// and the board. The cockpit is there only during a game: in the menus, the board and the panel float alone
/// in the sky. The images are created and the cockpit loaded the first time the headset shows the game, so
/// that a game out of the headset never holds them, and the cockpit is ready when a game starts; until it is
/// loaded, the board floats alone.
///
/// Destroy it before the window, with the renderer.
/// @code
/// vrView.update(elapsed, {sceneRenderer.light(), sceneRenderer.sky(), sessions.running(),
///                         sceneRenderer.hasContent(Surface::Panel), scenes.bottom()}); // each frame
/// vrView.drawFrames([&](engine::Surface surface) { /* draw the surface */ });          // each frame
/// vrView.drawEye(eye, view);                                                            // each eye
/// @endcode
class VrView
{
    /// The cockpit once loaded: its model, the meshes drawn opaque and the transparent ones in their order.
    struct Cockpit {
        render::ModelId model;
        /// The transparent meshes, by index, from the farthest to the nearest.
        std::vector<int> transparent;
        /// Every mesh but one, for each transparent mesh: drawn alone.
        std::vector<std::vector<int>> allBut;
        /// The animation of the screens, -1 without one.
        int screens = -1;
    };

    render::Renderer &_renderer;
    render::ResourceManager &_resources;
    VrLayout _layout;
    std::optional<render::ImageId> _board;
    std::optional<render::ImageId> _panel;
    /// The image of each screen of the cockpit, in the order of CockpitScreens.
    std::array<std::optional<render::ImageId>, CockpitScreens.size()> _screenImages{};
    std::optional<std::shared_future<render::ModelId>> _loading;
    std::optional<Cockpit> _cockpit;
    VrScene _scene;
    float _screensTime = 0.0f;
    /// How far the panel is unfolded, from 0 (folded) to 1 (open).
    float _openness = 0.0f;
    SceneFade _fade;

    /// The image of the board, created the first time.
    [[nodiscard]] render::ImageId board();
    /// The image of the panel, created the first time.
    [[nodiscard]] render::ImageId panel();
    /// The image of a screen of the cockpit, by index in CockpitScreens, created the first time.
    [[nodiscard]] render::ImageId screen(std::size_t index);
    /// Takes the cockpit once loaded, finds its parts, and puts the images of the HUD on its screens.
    void findCockpit();
    /// Poses the screens of the cockpit, a few times per second.
    void animateCockpit(float elapsed);

public:
    /// The colour behind the menus on the panel: nearly opaque, so that the board is a glimmer behind them.
    static constexpr render::Color PanelBackground{5, 8, 11, 235};
    /// The light along the edges of the panel.
    static constexpr render::Color PanelEdge{111, 227, 240, 200};

    VrView(render::Renderer &renderer, render::ResourceManager &resources, VrLayout layout = {});

    /// Once per frame while the headset shows the game, before its frames: starts loading the cockpit, keeps
    /// what the scene gives, unfolds or folds the panel, fades when the scenes were switched, and during a
    /// game moves the screens of the cockpit, which are posed once for both eyes.
    /// @param elapsed Time since the previous frame, in seconds.
    void update(float elapsed, VrScene scene);
    /// Draws the frames of the headset, each into its image, the function drawing what each surface shows:
    /// during a game, the screens of the cockpit; the panel while it is unfolded; the board last, the World
    /// layer and the Board surface, so that the visible area of the renderer is the one of the game after it.
    /// While the scene left fades out, the panel and the board keep their images.
    void drawFrames(const std::function<void(engine::Surface)> &draw);
    /// Draws what an eye sees into the image the headset bound for it: the sky, the board once a frame was
    /// drawn into it, during a game the cockpit, then its canopy and its hologram, which the board is seen
    /// through, and the panel over everything while it is unfolded.
    void drawEye(int eye, const xr::EyeView &view);

    /// Where the toasts go: on the panel while it is open, on the board, over the game, otherwise.
    [[nodiscard]] engine::Surface toastSurface() const;
    /// How far the panel is unfolded, from 0 (folded) to 1 (open).
    [[nodiscard]] float panelOpenness() const { return _openness; }
};

} // namespace rtype::client
