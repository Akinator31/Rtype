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
    /// The highlight of a Panel.
    Highlight,
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

/// Turns the Model of the entity around its own vertical axis, forever: a ring of asteroids around a planet.
/// The SpinSystem moves Model::spinAngle.
struct Spin {
    /// In degrees per second; negative turns the other way.
    float speed = 0.0f;
};

/// Moves the entity along a scrolling picture of the sky (a Backdrop, a BackdropSprite), forever, to show the
/// travel of the camera: the ScrollSystem adds the speed times the elapsed time to the offset.
struct Scroll {
    /// In game units per second, the picture going to the left; negative goes to the right.
    float speed = 0.0f;
    /// How far the picture has gone, in game units.
    float offset = 0.0f;
};

/// No animation, for ModelAnimation::addedAnimation.
inline constexpr int NoAnimation = -1;

/// Plays an animation of the skinned Model of the entity, in a loop: the ModelAnimationSystem moves its time,
/// the SceneRenderer poses the model before drawing it. Play another one with switchModelAnimation(), which
/// fades from the previous one, so that the model does not jump from a pose to the other. A speed of 0 holds
/// the pose. A second animation can be mixed in for good, in step with it: a pose between two of the rig.
/// A third one can be added over them once, with addModelAnimation(): what it changes from its first frame
/// adds to their pose, such as the recoil of a gun over a flight.
/// @code
/// const int idle = resources.getModelAnimation(model, "Idle");
/// ecs.addComponent<ModelAnimation>(ship, idle);
/// switchModelAnimation(ecs.getComponent<ModelAnimation>(ship), resources.getModelAnimation(model, "Move"));
/// const int shoot = resources.getModelAnimation(model, "Shoot");
/// addModelAnimation(ecs.getComponent<ModelAnimation>(ship), shoot,
///                   resources.getModelAnimationDuration(model, shoot));
/// @endcode
struct ModelAnimation {
    int animation = 0;
    float time = 0.0f;
    int previousAnimation = 0;
    float previousTime = 0.0f;
    float fadeDuration = 0.0f;
    /// How fast its time goes by: 1 plays the animation, 0 holds its pose.
    float speed = 1.0f;
    /// Another animation played at the same time: blendAmount 0 shows only `animation`, 1 only this one, 0.5
    /// halfway between them, such as a flame halfway between Idle and Move. Not mixed in while it fades.
    int blendedAnimation = 0;
    float blendAmount = 0.0f;
    /// Played once over the others, from its start, for addedDuration seconds, then removed (NoAnimation):
    /// the bones it moves keep the pose of the others, plus what it changes from its first frame.
    int addedAnimation = NoAnimation;
    float addedTime = 0.0f;
    float addedDuration = 0.0f;

    friend bool operator==(const ModelAnimation &, const ModelAnimation &) = default;
};

/// Plays another animation from its start, fading from the current one for `fadeDuration` seconds.
inline void switchModelAnimation(ModelAnimation &animation, int next, float fadeDuration = 0.5f)
{
    animation = {next, 0.0f, animation.animation, animation.time, fadeDuration};
}

/// Plays an animation once from its start, for `duration` seconds, over the current one: its length, from
/// render::ResourceManager::getModelAnimationDuration(). It starts again if it was already played.
inline void addModelAnimation(ModelAnimation &animation, int added, float duration)
{
    animation.addedAnimation = added;
    animation.addedTime = 0.0f;
    animation.addedDuration = duration;
}

} // namespace rtype::engine
