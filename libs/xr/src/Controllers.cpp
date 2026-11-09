#include "Controllers.hpp"

#include "Check.hpp"
#include "rtype/log/Logger.hpp"

#include <cstring>
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
    std::strncpy(set.actionSetName, "gameplay", XR_MAX_ACTION_SET_NAME_SIZE - 1);
    std::strncpy(set.localizedActionSetName, "Gameplay", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
    check(instance, xrCreateActionSet(instance, &set, &_actionSet), "xrCreateActionSet");

    xrStringToPath(instance, "/user/hand/left", &_hands[0]);
    xrStringToPath(instance, "/user/hand/right", &_hands[1]);

    const auto create = [&](XrAction &action, XrActionType type, const char *name, const char *label) {
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
        info.actionType = type;
        std::strncpy(info.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
        std::strncpy(info.localizedActionName, label, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
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
        {_grip, "/user/hand/left/input/grip/pose"},        {_grip, "/user/hand/right/input/grip/pose"},
        {_aim, "/user/hand/left/input/aim/pose"},          {_aim, "/user/hand/right/input/aim/pose"},
        {_squeeze, "/user/hand/left/input/squeeze/value"}, {_squeeze, "/user/hand/right/input/squeeze/value"},
        {_trigger, "/user/hand/left/input/trigger/value"}, {_trigger, "/user/hand/right/input/trigger/value"},
        {_thumbstick, "/user/hand/left/input/thumbstick"}, {_thumbstick, "/user/hand/right/input/thumbstick"},
        {_primary, "/user/hand/left/input/x/click"},       {_primary, "/user/hand/right/input/a/click"},
        {_secondary, "/user/hand/left/input/y/click"},     {_secondary, "/user/hand/right/input/b/click"},
        {_menu, "/user/hand/left/input/menu/click"},       {_haptic, "/user/hand/left/output/haptic"},
        {_haptic, "/user/hand/right/output/haptic"},
    };
    suggest("/interaction_profiles/oculus/touch_controller", touch);
    if (_session.hasTouchControllerPlus()) {
        suggest("/interaction_profiles/meta/touch_controller_plus", touch);
    }
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

} // namespace rtype::xr::detail
