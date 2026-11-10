#pragma once

#include "Vr/VrLayout.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"
#include "rtype/xr/Headset.hpp"

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
    /// A game is played: the player sits in the cockpit. In the menus, the board floats alone in the sky.
    bool playing = false;
};

/// What the player sees in the headset (docs/architecture/vr.md): seated in the cockpit, the sky of the scene
/// around them, the board outside the canopy with the frame of the game drawn into it. The cockpit is there
/// only during a game: in the menus, the board floats alone in the sky. The image of the board is created and
/// the cockpit loaded the first time the headset shows the game, so that a game out of the headset never
/// holds them, and the cockpit is ready when a game starts; until it is loaded, the board floats alone.
///
/// Destroy it before the window, with the renderer.
/// @code
/// vrView.update(elapsed, {sceneRenderer.light(), sceneRenderer.sky(), sessions.running()}); // each frame
/// vrView.drawEye(eye, view);                                                                // each eye
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
    std::optional<std::shared_future<render::ModelId>> _loading;
    std::optional<Cockpit> _cockpit;
    VrScene _scene;
    float _screensTime = 0.0f;

    /// Takes the cockpit once loaded, and finds its parts.
    void findCockpit();
    /// Poses the screens of the cockpit, a few times per second.
    void animateCockpit(float elapsed);

public:
    VrView(render::Renderer &renderer, render::ResourceManager &resources, VrLayout layout = {});

    /// The image the frames of the game are drawn into while the game is in the headset
    /// (Renderer::beginFrame(ImageId)).
    [[nodiscard]] render::ImageId board();
    /// Once per frame while the headset shows the game, before its eyes: starts loading the cockpit, keeps
    /// what the scene gives, and during a game moves the screens of the cockpit, which are posed once for
    /// both eyes.
    /// @param elapsed Time since the previous frame, in seconds.
    void update(float elapsed, VrScene scene);
    /// Draws what an eye sees into the image the headset bound for it: the sky, the board once a frame was
    /// drawn into it, and during a game the cockpit, then its canopy and its hologram, which the board is
    /// seen through.
    void drawEye(int eye, const xr::EyeView &view);
};

} // namespace rtype::client
