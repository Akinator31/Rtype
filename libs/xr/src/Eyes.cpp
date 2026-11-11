#include "Eyes.hpp"

#include "Check.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace rtype::xr::detail
{

namespace
{

constexpr std::int64_t GlSrgb8Alpha8 = 0x8C43;
constexpr std::int64_t GlRgba8 = 0x8058;
/// The nearest and farthest the eyes see, in metres: the canopy is a few tens of centimetres from the eyes,
/// and the board a few metres.
constexpr float Near = 0.05f;
constexpr float Far = 200.0f;

/// sRGB first: the renderer writes colours with their gamma already (GL_FRAMEBUFFER_SRGB stays off), so that
/// they are stored as they are and the runtime reads them as sRGB, as on the screen.
std::int64_t chooseFormat(XrSession session)
{
    std::uint32_t count = 0;
    xrEnumerateSwapchainFormats(session, 0, &count, nullptr);
    std::vector<std::int64_t> formats(count);
    xrEnumerateSwapchainFormats(session, count, &count, formats.data());
    for (const std::int64_t wanted : {GlSrgb8Alpha8, GlRgba8}) {
        if (std::ranges::find(formats, wanted) != formats.end()) {
            return wanted;
        }
    }
    throw OpenXrError("the headset offers no RGBA8 nor SRGB8_ALPHA8 image");
}

unsigned char channel(float value)
{
    return static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
}

} // namespace

Eyes::Eyes(Session &session, Rgb background) : _session(session), _background(background)
{
    try {
        const XrInstance instance = session.instance();
        std::uint32_t count = 0;
        xrEnumerateViewConfigurationViews(instance, session.system(),
                                          XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &count, nullptr);
        std::vector<XrViewConfigurationView> configurations(count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        xrEnumerateViewConfigurationViews(instance, session.system(),
                                          XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, count, &count,
                                          configurations.data());
        if (configurations.empty()) {
            throw OpenXrError("the headset has no stereo view");
        }
        const std::int64_t format = chooseFormat(session.session());

        _eyes.resize(configurations.size());
        _layerViews.assign(configurations.size(), {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});
        for (std::size_t index = 0; index < configurations.size(); ++index) {
            Eye &eye = _eyes[index];
            eye.width = static_cast<int>(configurations[index].recommendedImageRectWidth);
            eye.height = static_cast<int>(configurations[index].recommendedImageRectHeight);

            XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            info.format = format;
            info.sampleCount = 1;
            info.width = static_cast<std::uint32_t>(eye.width);
            info.height = static_cast<std::uint32_t>(eye.height);
            info.faceCount = 1;
            info.arraySize = 1;
            info.mipCount = 1;
            check(instance, xrCreateSwapchain(session.session(), &info, &eye.swapchain), "xrCreateSwapchain");

            std::uint32_t imageCount = 0;
            xrEnumerateSwapchainImages(eye.swapchain, 0, &imageCount, nullptr);
            std::vector<XrSwapchainImageOpenGLKHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
            check(instance,
                  xrEnumerateSwapchainImages(eye.swapchain, imageCount, &imageCount,
                                             reinterpret_cast<XrSwapchainImageBaseHeader *>(images.data())),
                  "xrEnumerateSwapchainImages");
            for (const XrSwapchainImageOpenGLKHR &image : images) {
                eye.images.push_back(image.image);
            }

            eye.framebuffer = rlLoadFramebuffer();
            const unsigned int depth = rlLoadTextureDepth(eye.width, eye.height, true);
            rlFramebufferAttach(eye.framebuffer, depth, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
            rlFramebufferAttach(eye.framebuffer, eye.images.front(), RL_ATTACHMENT_COLOR_CHANNEL0,
                                RL_ATTACHMENT_TEXTURE2D, 0);
            if (!rlFramebufferComplete(eye.framebuffer)) {
                throw OpenXrError("the framebuffer of an eye is incomplete");
            }

            _layerViews[index].subImage.swapchain = eye.swapchain;
            _layerViews[index].subImage.imageRect = {{0, 0}, {eye.width, eye.height}};
        }
    } catch (...) {
        release();
        throw;
    }
}

Eyes::~Eyes()
{
    release();
}

void Eyes::release()
{
    for (Eye &eye : _eyes) {
        if (eye.framebuffer != 0) {
            // Its depth renderbuffer too
            rlUnloadFramebuffer(eye.framebuffer);
        }
        if (eye.swapchain != XR_NULL_HANDLE) {
            xrDestroySwapchain(eye.swapchain);
        }
    }
    _eyes.clear();
}

EyeView Eyes::begin(int index)
{
    Eye &eye = _eyes.at(static_cast<std::size_t>(index));
    XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    xrAcquireSwapchainImage(eye.swapchain, &acquire, &eye.acquired);
    XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wait.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(eye.swapchain, &wait);

    // What was batched for the previous target goes there, not into this eye
    rlDrawRenderBatchActive();
    rlFramebufferAttach(eye.framebuffer, eye.images.at(eye.acquired), RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlEnableFramebuffer(eye.framebuffer);
    rlViewport(0, 0, eye.width, eye.height);
    rlClearColor(channel(_background.red), channel(_background.green), channel(_background.blue), 255);
    rlClearScreenBuffers();

    const XrView &view = _session.views().at(static_cast<std::size_t>(index));
    _layerViews[static_cast<std::size_t>(index)].pose = view.pose;
    _layerViews[static_cast<std::size_t>(index)].fov = view.fov;

    const Pose pose = toPose(view.pose);
    return {.view = rigidInverse(toMatrix(pose)),
            .projection = projection(
                {view.fov.angleLeft, view.fov.angleRight, view.fov.angleUp, view.fov.angleDown}, Near, Far),
            .position = pose.position,
            .width = eye.width,
            .height = eye.height};
}

void Eyes::end(int index)
{
    rlDrawRenderBatchActive();
    rlDisableFramebuffer();
    XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrReleaseSwapchainImage(_eyes.at(static_cast<std::size_t>(index)).swapchain, &release);
}

} // namespace rtype::xr::detail
