#include "Controllers.hpp"

#include "Check.hpp"
#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <initializer_list>
#include <vector>

namespace rtype::xr::detail
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("xr");
    return instance;
}

struct Binding {
    XrAction action;
    const char *path;
};

} // namespace

Controllers::Controllers(Session &session) : _session(session)
{
    try {
        createActions();
        suggestBindings();
        createSpaces();
        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attach.countActionSets = 1;
        attach.actionSets = &_actionSet;
        check(session.instance(), xrAttachSessionActionSets(session.session(), &attach),
              "xrAttachSessionActionSets");
    } catch (...) {
        release();
        throw;
    }
}

Controllers::~Controllers()
{
    release();
}

void Controllers::release()
{
    for (int hand = 0; hand < HandCount; ++hand) {
        for (XrSpace *space : {&_gripSpaces.at(hand), &_aimSpaces.at(hand)}) {
            if (*space != XR_NULL_HANDLE) {
                xrDestroySpace(*space);
                *space = XR_NULL_HANDLE;
            }
        }
    }
    if (_actionSet != XR_NULL_HANDLE) {
        // Its actions too
        xrDestroyActionSet(_actionSet);
        _actionSet = XR_NULL_HANDLE;
    }
}

void Controllers::createActions()
{
    const XrInstance instance = _session.instance();
    XrActionSetCreateInfo set{XR_TYPE_ACTION_SET_CREATE_INFO};
    copyName(set.actionSetName, "gameplay");
    copyName(set.localizedActionSetName, "Gameplay");
    check(instance, xrCreateActionSet(instance, &set, &_actionSet), "xrCreateActionSet");

    xrStringToPath(instance, "/user/hand/left", &_hands[0]);
    xrStringToPath(instance, "/user/hand/right", &_hands[1]);

    const auto create = [&](XrAction &action, XrActionType type, const char *name, const char *label) {
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
        info.actionType = type;
        copyName(info.actionName, name);
        copyName(info.localizedActionName, label);
        info.countSubactionPaths = HandCount;
        info.subactionPaths = _hands.data();
        check(instance, xrCreateAction(_actionSet, &info, &action), name);
    };
    // The names the runtime shows when the player rebinds the controllers in its own menu (SteamVR)
    create(_grip, XR_ACTION_TYPE_POSE_INPUT, "hand_pose", "Hand");
    create(_aim, XR_ACTION_TYPE_POSE_INPUT, "aim_pose", "Pointer");
    create(_squeeze, XR_ACTION_TYPE_FLOAT_INPUT, "squeeze", "Grip");
    create(_trigger, XR_ACTION_TYPE_FLOAT_INPUT, "trigger", "Trigger");
    create(_thumbstick, XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick", "Thumbstick");
    create(_thumbstickClick, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_click", "Thumbstick click");
    create(_primary, XR_ACTION_TYPE_BOOLEAN_INPUT, "primary", "A / X");
    create(_secondary, XR_ACTION_TYPE_BOOLEAN_INPUT, "secondary", "B / Y");
    create(_menu, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", "Menu");
    create(_haptic, XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", "Vibration");
}

void Controllers::suggestBindings()
{
    const XrInstance instance = _session.instance();
    // A profile the runtime refuses is not an error: the game is played with the profiles it takes, or
    // without controllers, with a keyboard or a gamepad
    const auto suggest = [&](const char *profile, std::initializer_list<Binding> list) {
        std::vector<XrActionSuggestedBinding> bindings;
        for (const Binding &binding : list) {
            XrPath path = XR_NULL_PATH;
            xrStringToPath(instance, binding.path, &path);
            bindings.push_back({binding.action, path});
        }
        XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        xrStringToPath(instance, profile, &suggested.interactionProfile);
        suggested.countSuggestedBindings = static_cast<std::uint32_t>(bindings.size());
        suggested.suggestedBindings = bindings.data();
        const XrResult result = xrSuggestInteractionProfileBindings(instance, &suggested);
        if (XR_FAILED(result)) {
            logger().warning("the runtime refuses the bindings of {} ({})", profile,
                             resultName(instance, result));
        }
    };

    // The Touch controllers: the same paths for the Quest 2 and Pro ones and for the Touch Plus of the Quest
    // 3, which a runtime may not convert into each other
    const std::initializer_list<Binding> touch = {
        {_grip, "/user/hand/left/input/grip/pose"},
        {_grip, "/user/hand/right/input/grip/pose"},
        {_aim, "/user/hand/left/input/aim/pose"},
        {_aim, "/user/hand/right/input/aim/pose"},
        {_squeeze, "/user/hand/left/input/squeeze/value"},
        {_squeeze, "/user/hand/right/input/squeeze/value"},
        {_trigger, "/user/hand/left/input/trigger/value"},
        {_trigger, "/user/hand/right/input/trigger/value"},
        {_thumbstick, "/user/hand/left/input/thumbstick"},
        {_thumbstick, "/user/hand/right/input/thumbstick"},
        {_thumbstickClick, "/user/hand/left/input/thumbstick/click"},
        {_thumbstickClick, "/user/hand/right/input/thumbstick/click"},
        {_primary, "/user/hand/left/input/x/click"},
        {_primary, "/user/hand/right/input/a/click"},
        {_secondary, "/user/hand/left/input/y/click"},
        {_secondary, "/user/hand/right/input/b/click"},
        {_menu, "/user/hand/left/input/menu/click"},
        {_haptic, "/user/hand/left/output/haptic"},
        {_haptic, "/user/hand/right/output/haptic"},
    };
    suggest("/interaction_profiles/oculus/touch_controller", touch);
    if (_session.hasTouchControllerPlus()) {
        suggest("/interaction_profiles/meta/touch_controller_plus", touch);
    }
    // The Valve Index controllers, through SteamVR: no menu button (its system button belongs to SteamVR), so
    // the left thumbstick pushed in opens the menu
    suggest("/interaction_profiles/valve/index_controller",
            {
                {_grip, "/user/hand/left/input/grip/pose"},
                {_grip, "/user/hand/right/input/grip/pose"},
                {_aim, "/user/hand/left/input/aim/pose"},
                {_aim, "/user/hand/right/input/aim/pose"},
                {_squeeze, "/user/hand/left/input/squeeze/value"},
                {_squeeze, "/user/hand/right/input/squeeze/value"},
                {_trigger, "/user/hand/left/input/trigger/value"},
                {_trigger, "/user/hand/right/input/trigger/value"},
                {_thumbstick, "/user/hand/left/input/thumbstick"},
                {_thumbstick, "/user/hand/right/input/thumbstick"},
                {_menu, "/user/hand/left/input/thumbstick/click"},
                {_thumbstickClick, "/user/hand/right/input/thumbstick/click"},
                {_primary, "/user/hand/left/input/a/click"},
                {_primary, "/user/hand/right/input/a/click"},
                {_secondary, "/user/hand/left/input/b/click"},
                {_secondary, "/user/hand/right/input/b/click"},
                {_haptic, "/user/hand/left/output/haptic"},
                {_haptic, "/user/hand/right/output/haptic"},
            });
    // The HTC Vive wands: the touchpad is the thumbstick, pressed it is the face button; the grip is a click,
    // read as fully pressed
    suggest("/interaction_profiles/htc/vive_controller",
            {
                {_grip, "/user/hand/left/input/grip/pose"},
                {_grip, "/user/hand/right/input/grip/pose"},
                {_aim, "/user/hand/left/input/aim/pose"},
                {_aim, "/user/hand/right/input/aim/pose"},
                {_squeeze, "/user/hand/left/input/squeeze/click"},
                {_squeeze, "/user/hand/right/input/squeeze/click"},
                {_trigger, "/user/hand/left/input/trigger/value"},
                {_trigger, "/user/hand/right/input/trigger/value"},
                {_thumbstick, "/user/hand/left/input/trackpad"},
                {_thumbstick, "/user/hand/right/input/trackpad"},
                {_primary, "/user/hand/left/input/trackpad/click"},
                {_primary, "/user/hand/right/input/trackpad/click"},
                {_menu, "/user/hand/left/input/menu/click"},
                {_secondary, "/user/hand/right/input/menu/click"},
                {_haptic, "/user/hand/left/output/haptic"},
                {_haptic, "/user/hand/right/output/haptic"},
            });
    // The profile every runtime has: select clicks, menu opens the menu
    suggest("/interaction_profiles/khr/simple_controller",
            {
                {_grip, "/user/hand/left/input/grip/pose"},
                {_grip, "/user/hand/right/input/grip/pose"},
                {_aim, "/user/hand/left/input/aim/pose"},
                {_aim, "/user/hand/right/input/aim/pose"},
                {_trigger, "/user/hand/left/input/select/click"},
                {_trigger, "/user/hand/right/input/select/click"},
                {_menu, "/user/hand/left/input/menu/click"},
                {_menu, "/user/hand/right/input/menu/click"},
                {_haptic, "/user/hand/left/output/haptic"},
                {_haptic, "/user/hand/right/output/haptic"},
            });
}

void Controllers::createSpaces()
{
    for (int hand = 0; hand < HandCount; ++hand) {
        XrActionSpaceCreateInfo info{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        info.subactionPath = _hands.at(hand);
        info.poseInActionSpace.orientation.w = 1.0f;
        info.action = _grip;
        check(_session.instance(), xrCreateActionSpace(_session.session(), &info, &_gripSpaces.at(hand)),
              "xrCreateActionSpace(grip)");
        info.action = _aim;
        check(_session.instance(), xrCreateActionSpace(_session.session(), &info, &_aimSpaces.at(hand)),
              "xrCreateActionSpace(aim)");
    }
}

float Controllers::readFloat(XrAction action, int hand) const
{
    XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
    info.action = action;
    info.subactionPath = _hands.at(hand);
    XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
    if (XR_FAILED(xrGetActionStateFloat(_session.session(), &info, &state)) || state.isActive == XR_FALSE) {
        return 0.0f;
    }
    return state.currentState;
}

bool Controllers::readBoolean(XrAction action, int hand) const
{
    XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
    info.action = action;
    info.subactionPath = _hands.at(hand);
    XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
    return XR_SUCCEEDED(xrGetActionStateBoolean(_session.session(), &info, &state)) &&
           state.isActive == XR_TRUE && state.currentState == XR_TRUE;
}

bool Controllers::locate(XrSpace space, XrTime time, Pose &pose) const
{
    constexpr XrSpaceLocationFlags Tracked =
        XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
    if (XR_FAILED(xrLocateSpace(space, _session.space(), time, &location)) ||
        (location.locationFlags & Tracked) != Tracked) {
        return false;
    }
    pose = toPose(location.pose);
    return true;
}

void Controllers::update(XrTime time)
{
    XrActiveActionSet active{_actionSet, XR_NULL_PATH};
    XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
    sync.countActiveActionSets = 1;
    sync.activeActionSets = &active;
    // XR_SESSION_NOT_FOCUSED succeeds and makes every action inactive: the runtime shows its menu, and the
    // game must not read what the player does in it
    const XrResult result = xrSyncActions(_session.session(), &sync);
    if (XR_FAILED(result) || result == XR_SESSION_NOT_FOCUSED) {
        forget();
        return;
    }
    for (int hand = 0; hand < HandCount; ++hand) {
        HandState state;
        state.squeeze = readFloat(_squeeze, hand);
        state.trigger = readFloat(_trigger, hand);
        XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
        info.action = _thumbstick;
        info.subactionPath = _hands.at(hand);
        XrActionStateVector2f stick{XR_TYPE_ACTION_STATE_VECTOR2F};
        if (XR_SUCCEEDED(xrGetActionStateVector2f(_session.session(), &info, &stick)) &&
            stick.isActive == XR_TRUE) {
            state.stickX = stick.currentState.x;
            state.stickY = stick.currentState.y;
        }
        state.stickClick = readBoolean(_thumbstickClick, hand);
        state.primary = readBoolean(_primary, hand);
        state.secondary = readBoolean(_secondary, hand);
        state.menu = readBoolean(_menu, hand);
        state.tracked =
            locate(_gripSpaces.at(hand), time, state.grip) && locate(_aimSpaces.at(hand), time, state.aim);
        _states.at(hand) = state;
    }
}

void Controllers::vibrate(int hand, const Vibration &vibration)
{
    XrHapticVibration haptic{XR_TYPE_HAPTIC_VIBRATION};
    haptic.amplitude = std::clamp(vibration.amplitude, 0.0f, 1.0f);
    haptic.duration =
        vibration.seconds > 0.0f ? static_cast<XrDuration>(vibration.seconds * 1e9f) : XR_MIN_HAPTIC_DURATION;
    haptic.frequency = vibration.frequency > 0.0f ? vibration.frequency : XR_FREQUENCY_UNSPECIFIED;
    XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};
    info.action = _haptic;
    info.subactionPath = _hands.at(hand);
    // A controller that does not vibrate, or a session not focused, is not an error: the game goes on
    xrApplyHapticFeedback(_session.session(), &info, reinterpret_cast<const XrHapticBaseHeader *>(&haptic));
}

} // namespace rtype::xr::detail
