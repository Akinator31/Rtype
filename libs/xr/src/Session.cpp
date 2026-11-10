#include "Session.hpp"

#include "Check.hpp"
#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace rtype::xr::detail
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("xr");
    return instance;
}

constexpr XrViewConfigurationType ViewConfiguration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

std::vector<XrExtensionProperties> availableExtensions()
{
    std::uint32_t count = 0;
    if (XR_FAILED(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr))) {
        // No loader can be this broken: the runtime is missing, which xrCreateInstance() says better
        return {};
    }
    std::vector<XrExtensionProperties> extensions(count, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, count, &count, extensions.data());
    return extensions;
}

bool contains(const std::vector<XrExtensionProperties> &extensions, const char *name)
{
    return std::ranges::any_of(extensions, [name](const XrExtensionProperties &extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

std::string versionText(XrVersion version)
{
    return std::to_string(XR_VERSION_MAJOR(version)) + "." + std::to_string(XR_VERSION_MINOR(version)) + "." +
           std::to_string(XR_VERSION_PATCH(version));
}

XrPosef toXr(const Pose &pose)
{
    XrPosef result{};
    result.position = {pose.position.x, pose.position.y, pose.position.z};
    result.orientation = {pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w};
    return result;
}

XrPosef identityPose()
{
    XrPosef pose{};
    pose.orientation.w = 1.0f;
    return pose;
}

} // namespace

Session::Session(const std::string &applicationName)
{
    try {
        createInstance(applicationName);
        createSession();
    } catch (...) {
        // The destructor of an object whose constructor threw is not called
        release();
        throw;
    }
}

Session::~Session()
{
    release();
}

void Session::release()
{
    for (XrSpace *space : {&_space, &_local, &_head}) {
        if (*space != XR_NULL_HANDLE) {
            xrDestroySpace(*space);
            *space = XR_NULL_HANDLE;
        }
    }
    if (_session != XR_NULL_HANDLE) {
        xrDestroySession(_session);
        _session = XR_NULL_HANDLE;
    }
    if (_instance != XR_NULL_HANDLE) {
        xrDestroyInstance(_instance);
        _instance = XR_NULL_HANDLE;
    }
}

void Session::createInstance(const std::string &applicationName)
{
    const std::vector<XrExtensionProperties> available = availableExtensions();
    if (!available.empty() && !contains(available, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME)) {
        throw OpenXrError("the OpenXR runtime cannot draw with OpenGL (no XR_KHR_opengl_enable)");
    }
    std::vector<const char *> extensions{XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
    // The controllers of the Quest 3 and 3S have a profile of their own, when the runtime knows it
    if (contains(available, XR_META_TOUCH_CONTROLLER_PLUS_EXTENSION_NAME)) {
        extensions.push_back(XR_META_TOUCH_CONTROLLER_PLUS_EXTENSION_NAME);
        _touchControllerPlus = true;
    }

    XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
    copyName(info.applicationInfo.applicationName, applicationName);
    copyName(info.applicationInfo.engineName, "R-Type");
    info.applicationInfo.applicationVersion = 1;
    // 1.0: a runtime of 1.1 accepts it, a runtime of 1.0 would refuse 1.1
    info.applicationInfo.apiVersion = XR_API_VERSION_1_0;
    info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    info.enabledExtensionNames = extensions.data();
    const XrResult created = xrCreateInstance(&info, &_instance);
    if (created == XR_ERROR_RUNTIME_UNAVAILABLE || created == XR_ERROR_RUNTIME_FAILURE ||
        created == XR_ERROR_INSTANCE_LOST) {
        // Without an instance, the result has no name: the loader explains on the error output what it found
        throw OpenXrError("no OpenXR runtime is installed or active");
    }
    check(_instance, created, "xrCreateInstance");

    XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
    if (XR_SUCCEEDED(xrGetInstanceProperties(_instance, &properties))) {
        _runtimeName = std::string(properties.runtimeName) + " " + versionText(properties.runtimeVersion);
    }

    XrSystemGetInfo system{XR_TYPE_SYSTEM_GET_INFO};
    system.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    const XrResult found = xrGetSystem(_instance, &system, &_system);
    if (found == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
        throw OpenXrError("the OpenXR runtime " + _runtimeName + " sees no headset (is it connected?)");
    }
    check(_instance, found, "xrGetSystem");

    std::uint32_t count = 0;
    xrEnumerateEnvironmentBlendModes(_instance, _system, ViewConfiguration, 0, &count, nullptr);
    std::vector<XrEnvironmentBlendMode> modes(count);
    xrEnumerateEnvironmentBlendModes(_instance, _system, ViewConfiguration, count, &count, modes.data());
    // The space around the cockpit is ours: opaque whenever the headset offers it
    if (!modes.empty() && std::ranges::find(modes, XR_ENVIRONMENT_BLEND_MODE_OPAQUE) == modes.end()) {
        _blendMode = modes.front();
    }
}

void Session::createSession()
{
    // Required before xrCreateSession(), even if what it says is not used
    PFN_xrGetOpenGLGraphicsRequirementsKHR getRequirements = nullptr;
    check(_instance,
          xrGetInstanceProcAddr(_instance, "xrGetOpenGLGraphicsRequirementsKHR",
                                reinterpret_cast<PFN_xrVoidFunction *>(&getRequirements)),
          "xrGetInstanceProcAddr(xrGetOpenGLGraphicsRequirementsKHR)");
    XrGraphicsRequirementsOpenGLKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    check(_instance, getRequirements(_instance, _system, &requirements),
          "xrGetOpenGLGraphicsRequirementsKHR");
    logger().debug("the runtime asks for OpenGL {} to {}", versionText(requirements.minApiVersionSupported),
                   versionText(requirements.maxApiVersionSupported));

    // The context GLFW made current for the window, through raylib
#ifdef _WIN32
    XrGraphicsBindingOpenGLWin32KHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
    binding.hDC = wglGetCurrentDC();
    binding.hGLRC = wglGetCurrentContext();
    if (binding.hDC == nullptr || binding.hGLRC == nullptr) {
        throw OpenXrError("no current WGL context: open the window before the headset");
    }
#else
    XrGraphicsBindingOpenGLXlibKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    binding.xDisplay = glXGetCurrentDisplay();
    binding.glxContext = glXGetCurrentContext();
    binding.glxDrawable = glXGetCurrentDrawable();
    if (binding.xDisplay == nullptr || binding.glxContext == nullptr) {
        throw OpenXrError(
            "no current GLX context: open the window before the headset (on Wayland, the client "
            "must run through XWayland)");
    }
    int configId = 0;
    glXQueryContext(binding.xDisplay, binding.glxContext, GLX_FBCONFIG_ID, &configId);
    const int attributes[] = {GLX_FBCONFIG_ID, configId, 0};
    int configCount = 0;
    GLXFBConfig *configs =
        glXChooseFBConfig(binding.xDisplay, DefaultScreen(binding.xDisplay), attributes, &configCount);
    if (configs != nullptr && configCount > 0) {
        binding.glxFBConfig = configs[0];
        if (XVisualInfo *visual = glXGetVisualFromFBConfig(binding.xDisplay, configs[0])) {
            binding.visualid = static_cast<std::uint32_t>(visual->visualid);
            XFree(visual);
        }
    }
    if (configs != nullptr) {
        XFree(configs);
    }
#endif

    XrSessionCreateInfo info{XR_TYPE_SESSION_CREATE_INFO};
    info.next = &binding;
    info.systemId = _system;
    check(_instance, xrCreateSession(_instance, &info, &_session), "xrCreateSession");

    XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    space.poseInReferenceSpace = identityPose();
    space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    check(_instance, xrCreateReferenceSpace(_session, &space, &_space), "xrCreateReferenceSpace");
    check(_instance, xrCreateReferenceSpace(_session, &space, &_local), "xrCreateReferenceSpace");
    space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    check(_instance, xrCreateReferenceSpace(_session, &space, &_head), "xrCreateReferenceSpace");

    std::uint32_t count = 0;
    xrEnumerateViewConfigurationViews(_instance, _system, ViewConfiguration, 0, &count, nullptr);
    _views.assign(count, {XR_TYPE_VIEW});
}

void Session::placeOrigin(const XrPosef &origin)
{
    XrReferenceSpaceCreateInfo info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    info.poseInReferenceSpace = origin;
    XrSpace space = XR_NULL_HANDLE;
    // Without a new space, the old one stays: the cockpit where it was rather than nowhere
    if (succeeded(xrCreateReferenceSpace(_session, &info, &space), "xrCreateReferenceSpace")) {
        xrDestroySpace(_space);
        _space = space;
    }
}

void Session::recenterOnHead()
{
    XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};
    if (!succeeded(xrLocateSpace(_head, _local, _frame.predictedDisplayTime, &head), "xrLocateSpace")) {
        return;
    }
    constexpr XrSpaceLocationFlags Tracked =
        XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    // Not tracked now: asked again at the next frame, rather than recentred on a wrong place
    if ((head.locationFlags & Tracked) != Tracked) {
        _recenterAsked = true;
        return;
    }
    placeOrigin(toXr(levelled(toPose(head.pose))));
    logger().info("recentred on the head of the player");
}

void Session::lose(const std::string &reason)
{
    if (_state != HeadsetState::Lost) {
        logger().warning("the VR session is over: {}", reason);
    }
    _state = HeadsetState::Lost;
    _frameBegun = false;
    _shouldRender = false;
}

bool Session::succeeded(XrResult result, const char *what)
{
    if (XR_SUCCEEDED(result)) {
        return true;
    }
    if (result == XR_ERROR_SESSION_LOST || result == XR_ERROR_INSTANCE_LOST) {
        lose(std::string(what) + ": " + resultName(_instance, result));
    } else {
        logger().warning("{} failed ({})", what, resultName(_instance, result));
    }
    return false;
}

void Session::changeState(XrSessionState state)
{
    switch (state) {
    case XR_SESSION_STATE_READY: {
        XrSessionBeginInfo info{XR_TYPE_SESSION_BEGIN_INFO};
        info.primaryViewConfigurationType = ViewConfiguration;
        if (succeeded(xrBeginSession(_session, &info), "xrBeginSession")) {
            _state = HeadsetState::Running;
            logger().info("the headset shows the game");
        }
        break;
    }
    case XR_SESSION_STATE_STOPPING:
        xrEndSession(_session);
        _state = HeadsetState::Waiting;
        logger().info("the headset no longer shows the game");
        break;
    case XR_SESSION_STATE_EXITING:
        lose("the runtime ended it");
        break;
    case XR_SESSION_STATE_LOSS_PENDING:
        lose("the runtime lost the headset");
        break;
    default:
        break;
    }
}

void Session::pollEvents()
{
    if (_state == HeadsetState::Lost) {
        return;
    }
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(_instance, &event) == XR_SUCCESS) {
        if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
            lose("the runtime is going away");
            return;
        }
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            changeState(reinterpret_cast<const XrEventDataSessionStateChanged &>(event).state);
        }
        // The runtime recentred LOCAL on the head of the player (the button of its menu): the origin follows
        // it, instead of staying where the game had moved it, now somewhere else
        if (event.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING &&
            reinterpret_cast<const XrEventDataReferenceSpaceChangePending &>(event).referenceSpaceType ==
                XR_REFERENCE_SPACE_TYPE_LOCAL) {
            placeOrigin(identityPose());
            logger().info("the runtime recentred the headset");
        }
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

bool Session::beginFrame()
{
    _frameBegun = false;
    _shouldRender = false;
    if (_state != HeadsetState::Running) {
        return false;
    }
    _frame = {XR_TYPE_FRAME_STATE};
    XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};
    if (!succeeded(xrWaitFrame(_session, &wait, &_frame), "xrWaitFrame")) {
        return false;
    }
    XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};
    if (!succeeded(xrBeginFrame(_session, &begin), "xrBeginFrame")) {
        return false;
    }
    _frameBegun = true;
    if (_recenterAsked) {
        _recenterAsked = false;
        recenterOnHead();
    }
    if (_frame.shouldRender == XR_FALSE) {
        return false;
    }

    XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
    locate.viewConfigurationType = ViewConfiguration;
    locate.displayTime = _frame.predictedDisplayTime;
    locate.space = _space;
    XrViewState viewState{XR_TYPE_VIEW_STATE};
    std::uint32_t count = 0;
    if (!succeeded(xrLocateViews(_session, &locate, &viewState, static_cast<std::uint32_t>(_views.size()),
                                 &count, _views.data()),
                   "xrLocateViews")) {
        return false;
    }
    // The tracking is lost for this frame: nothing is drawn rather than a view at a wrong place
    if ((viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
        return false;
    }
    _shouldRender = true;
    return true;
}

void Session::endFrame(const std::vector<XrCompositionLayerProjectionView> &views)
{
    if (!_frameBegun) {
        return;
    }
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = _space;
    layer.viewCount = static_cast<std::uint32_t>(views.size());
    layer.views = views.data();
    const auto *layers = reinterpret_cast<const XrCompositionLayerBaseHeader *>(&layer);

    XrFrameEndInfo info{XR_TYPE_FRAME_END_INFO};
    info.displayTime = _frame.predictedDisplayTime;
    info.environmentBlendMode = _blendMode;
    info.layerCount = _shouldRender ? 1 : 0;
    info.layers = &layers;
    succeeded(xrEndFrame(_session, &info), "xrEndFrame");
    _frameBegun = false;
    _shouldRender = false;
}

} // namespace rtype::xr::detail
