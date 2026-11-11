#pragma once

#include "Session.hpp"

#include <cstdint>
#include <vector>

namespace rtype::xr::detail
{

/// The images of the eyes: a swapchain per eye, whose images the runtime gives, and a framebuffer of rlgl
/// with its own depth buffer that draws into the image acquired. Created by the constructor, which throws
/// OpenXrError, destroyed by the destructor, while the OpenGL context of the window is still open.
class Eyes
{
    struct Eye {
        XrSwapchain swapchain = XR_NULL_HANDLE;
        int width = 0;
        int height = 0;
        /// The OpenGL textures of the swapchain.
        std::vector<unsigned int> images;
        /// Its depth renderbuffer is attached, and destroyed with it.
        unsigned int framebuffer = 0;
        std::uint32_t acquired = 0;
    };

    Session &_session;
    Rgb _background;
    std::vector<Eye> _eyes;
    std::vector<XrCompositionLayerProjectionView> _layerViews;

    void release();

public:
    Eyes(Session &session, Rgb background);
    ~Eyes();

    Eyes(const Eyes &) = delete;
    Eyes &operator=(const Eyes &) = delete;
    Eyes(Eyes &&) = delete;
    Eyes &operator=(Eyes &&) = delete;

    [[nodiscard]] int count() const { return static_cast<int>(_eyes.size()); }
    /// Acquires the image of the eye, binds it cleared, and gives what the eye sees this frame.
    EyeView begin(int eye);
    void end(int eye);
    /// What the frame shows: each eye, its image and its view.
    [[nodiscard]] const std::vector<XrCompositionLayerProjectionView> &layerViews() const
    {
        return _layerViews;
    }
};

} // namespace rtype::xr::detail
