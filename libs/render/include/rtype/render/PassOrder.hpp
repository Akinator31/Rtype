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
};

/// Checks that the passes of the renderer come in order: a frame or an eye at a time, each ended before the
/// next one starts, and each drawing made in a pass that can draw it. The Renderer keeps one; it holds no
/// graphics resource, so that the tests can check the order without a window.
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
    /// @throws std::logic_error if no eye is started.
    void endEye();

    /// For the drawings of a frame (2D, and the 3D models seen through a camera).
    /// @throws std::logic_error if no frame is started.
    void requireFrame() const;
    /// For the drawings of an eye.
    /// @throws std::logic_error if no eye is started.
    void requireEye() const;
};

} // namespace rtype::render
