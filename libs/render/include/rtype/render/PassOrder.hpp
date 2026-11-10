#pragma once

namespace rtype::render
{

/// What the renderer is drawing.
enum class Pass {
    /// Nothing: between two passes.
    None,
    /// A frame of the game, in game units, into the window or an image (Renderer::beginFrame()).
    Frame,
    /// The view of an eye of a VR headset, in 3D (Renderer::beginEye()).
    Eye,
    /// The World layer laid on the board in relief, inside an eye (Renderer::beginBoard()): the drawings of a
    /// frame, in game units, drawn in 3D on the board.
    Board,
};

/// Checks that the passes of the renderer come in order: a frame or an eye at a time, each ended before the
/// next one starts, a board only inside an eye, and each drawing made in a pass that can draw it. The
/// Renderer keeps one; it holds no graphics resource, so that the tests can check the order without a window.
class PassOrder
{
private:
    Pass _current = Pass::None;

public:
    /// The pass being drawn.
    [[nodiscard]] Pass current() const { return _current; }

    /// @throws std::logic_error if a frame or an eye is already started.
    void beginFrame();
    /// @throws std::logic_error if no frame is started.
    void endFrame();
    /// @throws std::logic_error if a frame or an eye is already started.
    void beginEye();
    /// @throws std::logic_error if no eye is started, or if a board is not ended.
    void endEye();
    /// @throws std::logic_error if no eye is started, or if a board already is.
    void beginBoard();
    /// Back to the eye the board is in.
    /// @throws std::logic_error if no board is started.
    void endBoard();

    /// For the drawings of a frame (2D, and the 3D models seen through a camera): in a frame, or on a board.
    /// @throws std::logic_error if no frame nor board is started.
    void requireFrame() const;
    /// For the drawings of an eye, out of its board.
    /// @throws std::logic_error if no eye is started, or if a board is.
    void requireEye() const;
    /// Whether the pass is a board.
    [[nodiscard]] bool onBoard() const { return _current == Pass::Board; }
};

} // namespace rtype::render
