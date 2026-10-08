#pragma once

namespace rtype::engine
{

/// How a value moves from its start to its end: the shape of the animation.
enum class Easing {
    /// At constant speed.
    Linear,
    /// Fast at first, then slowing down: natural for most movements.
    EaseOut,
    /// Goes slightly past the end, then comes back to it: a small bounce.
    EaseOutBack,
};

/// Progress of an animation, from 0 to 1, for a time from 0 to 1. EaseOutBack goes above 1 before ending
/// at 1.
[[nodiscard]] float ease(Easing easing, float time);

/// What a tween animates.
enum class TweenTarget {
    Scale,
    Opacity,
};

/// Animates a value of the entity from `from` to `to` in `duration` seconds. The tween is removed when it
/// ends, unless it goes back and forth.
/// @code
/// ecs.addComponent<Tween>(button, Tween{.target = TweenTarget::Scale, .from = 1.0f, .to = 1.1f,
///                                       .duration = 0.2f, .easing = Easing::EaseOutBack});
/// @endcode
struct Tween {
    TweenTarget target = TweenTarget::Scale;
    float from = 0.0f;
    float to = 1.0f;
    /// Duration of one way, in seconds.
    float duration = 0.2f;
    Easing easing = Easing::EaseOut;
    /// Goes back and forth forever, for instance to make something blink.
    bool pingPong = false;
    /// Time elapsed since the start, in seconds.
    float time = 0.0f;
};

} // namespace rtype::engine
