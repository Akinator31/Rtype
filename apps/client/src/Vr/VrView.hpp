#pragma once

#include "rtype/render/Renderer.hpp"
#include "rtype/render/Types.hpp"
#include "rtype/xr/Headset.hpp"

#include <optional>

namespace rtype::client
{

/// Where the board, the frame of the game, floats in the headset: flat, ahead of the player, in the space of
/// the headset (its origin at the head of the player when the session started, in metres, y up, -z ahead). A
/// first placement, for playing in the headset before the cockpit around it (docs/plan/vr.md, X3).
struct BoardPlacement {
    /// Its width, in metres; its height follows the shape of the game.
    float width = 4.0f;
    /// How far ahead of the eyes it is, in metres.
    float distance = 3.0f;
    /// How far its centre is under the eyes, in metres: the player looks a little down at it, as at a screen.
    float drop = 0.25f;
};

/// The resolution of the image of the board: about 38 pixels per degree for 4 m at 3 m, more than a headset
/// shows (20 to 25), so that the board stays sharp.
inline constexpr render::Vector2 BoardResolution{2560.0f, 1440.0f};

/// The transform of the board, for Renderer::drawImageQuad(): the square of side 1 around the origin scaled
/// to the board and moved ahead of the eyes, facing them.
/// @param gameSize Size of the game, in game units: the shape of the board.
[[nodiscard]] render::Matrix boardTransform(const BoardPlacement &placement, render::Vector2 gameSize);

/// What an eye sees, as the headset gives it, in the types of the renderer.
[[nodiscard]] render::EyeView toRenderEye(const xr::EyeView &eye);

/// What the player sees in the headset (docs/architecture/vr.md): the frame of the game drawn into the image
/// of the board, the board drawn in each eye. The image is created the first time it is asked for, so that a
/// game out of the headset never holds it.
///
/// Destroy it before the window, with the renderer.
class VrView
{
    render::Renderer &_renderer;
    BoardPlacement _placement;
    std::optional<render::ImageId> _board;

public:
    explicit VrView(render::Renderer &renderer, BoardPlacement placement = {});

    /// The image the frames of the game are drawn into while the game is in the headset
    /// (Renderer::beginFrame(ImageId)).
    [[nodiscard]] render::ImageId board();
    /// Draws what an eye sees into the image the headset bound for it: the board, once a frame was drawn into
    /// it, on the background of space.
    void drawEye(int eye, const xr::EyeView &view);
};

} // namespace rtype::client
