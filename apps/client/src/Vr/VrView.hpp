#pragma once

#include "Vr/CockpitLook.hpp"
#include "Vr/SceneFade.hpp"
#include "Vr/VrControllers.hpp"
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
#include <span>
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
    /// The world of the scene is drawn in relief on the board, for each eye: the board in relief is wanted
    /// (wantsReliefBoard()), and the world of the scene can be laid on it (canDrawInRelief()). Otherwise it
    /// is drawn once into the image of the board.
    bool relief = false;
};

/// What the player sees in the headset (docs/architecture/vr.md): seated in the cockpit, the sky of the scene
/// around them, the board outside the canopy with the world of the game on it, in relief or flat, the HUD on
/// the screens of the cockpit, and the menus on a holographic panel that unfolds from the hologram, between
/// the dashboard and the board. The cockpit is there only during a game: in the menus, the board and the
/// panel float alone in the sky. The images are created and the cockpit loaded the first time the headset
/// shows the game, so that a game out of the headset never holds them, and the cockpit is ready when a game
/// starts; until it is loaded, the board floats alone.
///
/// The world of the scene is drawn by the function given at construction: once per frame into the image of
/// the board while it is flat, once per eye on the board in relief otherwise.
///
/// Destroy it before the window, with the renderer.
/// @code
/// VrView vrView(renderer, resources, [&] { sceneRenderer.draw(renderer, Layer::World); });
/// vrView.update(elapsed, {sceneRenderer.light(), sceneRenderer.sky(), sessions.running(),
///                         sceneRenderer.hasContent(Surface::Panel), scenes.bottom(),
///                         canDrawInRelief(sceneRenderer.camera())});                      // each frame
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
    std::function<void()> _drawWorld;
    VrLayout _layout;
    /// The image of the board: the world and what of the interface is on the board while it is flat, that
    /// interface alone, over a clear background, while it is in relief.
    std::optional<render::ImageId> _board;
    /// How the image of the board was last drawn: for the board in relief, or flat.
    bool _boardRelief = false;
    std::optional<render::ImageId> _panel;
    /// The image of each screen of the cockpit, in the order of CockpitScreens.
    std::array<std::optional<render::ImageId>, CockpitScreens.size()> _screenImages{};
    std::optional<std::shared_future<render::ModelId>> _loading;
    /// The starry sky of the main menu, drawn around the cockpit during a game.
    std::optional<render::TextureId> _stars;
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
    /// Draws the controllers, their rays and their dots.
    void drawHands(std::span<const HandLook> hands);
    /// How wide a ray is, and the dot where it meets the panel, in metres.
    static constexpr float RayWidth = 0.004f;
    static constexpr float DotRadius = 0.008f;

public:
    /// The colour behind what the panel shows during a game (the pause, the death screen, the results):
    /// nearly opaque, so that the battle is a glimmer behind them. In the menus, the panel is a clear
    /// hologram, the world of the scene seen through it, as on the screen (PanelGlass).
    static constexpr render::Color PanelBackground{5, 8, 11, 235};
    static constexpr render::Color PanelGlass{5, 8, 11, 0};
    /// The light along the edges of the panel, during a game only.
    static constexpr render::Color PanelEdge{111, 227, 240, 200};
    /// The controllers: their body, their edges, and the light of their rays, dim, of the hand that points,
    /// and on a widget.
    static constexpr render::Color ControllerBody{18, 24, 30, 255};
    static constexpr render::Color ControllerEdge{111, 227, 240, 255};
    static constexpr render::Color RayIdle{40, 82, 90, 255};
    static constexpr render::Color RayPointing{70, 150, 165, 255};
    static constexpr render::Color RayOnWidget{140, 240, 255, 255};

    /// @param drawWorld Draws the World layer of the scenes shown, in a frame or on the board.
    VrView(render::Renderer &renderer, render::ResourceManager &resources, std::function<void()> drawWorld,
           VrLayout layout = {});

    /// Once per frame while the headset shows the game, before its frames: starts loading the cockpit, keeps
    /// what the scene gives (during a game, the stars of the main menu as its sky), unfolds or folds the
    /// panel, fades when the scenes were switched, and during a game moves the screens of the cockpit, which
    /// are posed once for both eyes.
    /// @param elapsed Time since the previous frame, in seconds.
    void update(float elapsed, VrScene scene);
    /// Draws the frames of the headset, each into its image, the function drawing what each surface shows:
    /// during a game, the screens of the cockpit; the panel while it is unfolded; the board last, the World
    /// layer while it is flat and the Board surface, so that the visible area of the renderer is the one of
    /// the game after it. While the scene left fades out, the panel and the board keep their images, unless
    /// the board is now drawn another way (in relief or flat).
    void drawFrames(const std::function<void(engine::Surface)> &draw);
    /// Draws what an eye sees into the image the headset bound for it: the sky, the board once a frame was
    /// drawn into it (in relief, the world laid on it for this eye, then the image of the board over it),
    /// during a game the cockpit, then its canopy and its hologram, which the board is seen through, the
    /// panel over everything while it is unfolded, and last the controllers in the hands, their rays and
    /// where they meet the panel.
    void drawEye(int eye, const xr::EyeView &view, std::span<const HandLook> hands = {});

    /// The panel as the controllers point at it, once it is open and while no fade hides it: nothing while it
    /// unfolds or folds back.
    [[nodiscard]] std::optional<PanelSurface> panelSurface() const;

    /// Where the toasts go: on the panel while it is open, on the board, over the game, otherwise.
    [[nodiscard]] engine::Surface toastSurface() const;
    /// How far the panel is unfolded, from 0 (folded) to 1 (open).
    [[nodiscard]] float panelOpenness() const { return _openness; }
};

} // namespace rtype::client
