#pragma once

#include "rtype/engine/Component/Animation.hpp"
#include "rtype/render/Types.hpp"

namespace rtype::client
{

/// How long the intro lasts, in seconds, from the press until the camera, the logo, its flare and the buttons
/// are all at their place: an Intro started this long ago is over.
inline constexpr float IntroLength = 5.0f;

/// The opening of the main menu, on the entity of its Camera: the camera waits in the asteroids until the
/// player presses a key, a button or clicks, then flies to the view of the menu. The IntroSystem moves the
/// camera and the elements with an IntroSlide.
struct Intro {
    /// Where the camera waits, and where it flies to.
    render::Camera from;
    render::Camera to;
    bool started = false;
    /// Time since the player pressed, in seconds.
    float time = 0.0f;
};

/// An element of the interface that moves when the intro starts: its Anchor offset goes from `from` to `to`,
/// its Scale, if it has one, from `fromScale` to `toScale`, and its Opacity, if it has one, from
/// `fromOpacity` to 1. A Hidden element shows up when the intro starts: it waits for the player to press.
struct IntroSlide {
    render::Vector2 from;
    render::Vector2 to;
    /// Time between the press and the start of the move, and how long the move lasts, in seconds.
    float delay = 0.0f;
    float duration = 1.0f;
    engine::Easing easing = engine::Easing::EaseOut;
    float fromScale = 1.0f;
    float toScale = 1.0f;
    float fromOpacity = 1.0f;
};

} // namespace rtype::client
