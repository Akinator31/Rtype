#pragma once

#include "Campaign/Transmission.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Control/Controls.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/input/InputSource.hpp"

namespace rtype::client
{

/// Plays the Transmissions of the scenes that update, with their sounds (engine::SoundEffect):
///
/// 1. a moment of silence on the black screen;
/// 2. the radio crackles, and the signal blinks a few times with a beep, then the channel connects;
/// 3. the lines are written one after the other, a key sound for each letter, a cursor at the end of the line
///    being written, and a blip at the end of each line; now and then the radio flickers;
/// 4. the end of the transmission shows with its over and out, then the hint tells the player to go on.
///
/// Space, Enter, the bottom face button or a click while the scene receives the input skips to the end, then
/// goes on: the onEnd of the transmission is called once. Escape is left to the scene, which closes.
///
/// Register it before the SoundSystem, which plays the sounds it gives, and before the AnchorSystem, which
/// places the lines as they grow.
class TransmissionSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    const engine::Scenes &_scenes;
    const engine::Controls &_controls;
    const input::InputSource &_input;
    ecs::ComponentCache<Transmission> &_transmissions;
    ecs::ComponentCache<engine::Text> &_texts;
    ecs::ComponentCache<engine::Opacity> &_opacities;
    ecs::ComponentCache<ecs::Hidden> &_hidden;
    /// Whether Enter or the mouse was held at the previous update: holding them does not press again.
    bool _wasHeld = true;

    /// What a phase does at each update, and when the player presses during it, in tables indexed by
    /// TransmissionPhase.
    using Step = void (TransmissionSystem::*)(Transmission &, float);
    using Press = void (TransmissionSystem::*)(Transmission &);

    [[nodiscard]] bool wasPressed();
    void press(Transmission &transmission);
    void advance(Transmission &transmission, float elapsed);
    // The steps of the phases
    void silence(Transmission &transmission, float elapsed);
    void signal(Transmission &transmission, float elapsed);
    void type(Transmission &transmission, float elapsed);
    void close(Transmission &transmission, float elapsed);
    void idle(Transmission &transmission, float elapsed);
    // The presses of the phases
    void reveal(Transmission &transmission);
    void skipClosing(Transmission &transmission);
    void finish(Transmission &transmission);
    void ignore(Transmission &transmission);
    void show(const Transmission &transmission);
    void enter(Transmission &transmission, TransmissionPhase phase);
    void setHidden(ecs::entity_t entity, bool hidden);
    void setOpacity(ecs::entity_t entity, float opacity);
    void play(ecs::entity_t entity, audio::SoundId sound, float volume, float pitch = 1.0f);

public:
    /// The black screen lasts this long, in seconds.
    static constexpr float SilenceDuration = 1.0f;
    /// The signal blinks this many times, each blink on then off, then stays this long before the typing.
    static constexpr int SignalBlinks = 3;
    static constexpr float BlinkOn = 0.4f;
    static constexpr float BlinkOff = 0.25f;
    static constexpr float SignalHold = 0.9f;
    /// The pause after a line, after an empty line, and before the end of the transmission, in seconds.
    static constexpr float LinePause = 0.35f;
    static constexpr float BlankPause = 0.5f;
    static constexpr float ClosingDelay = 0.8f;
    /// The hint to go on shows this long after the end of the transmission.
    static constexpr float WaitingDelay = 1.2f;
    /// The cursor blinks with this period, in seconds.
    static constexpr float CursorPeriod = 0.5f;

    static constexpr float StaticVolume = 0.7f;
    static constexpr float BeepVolume = 0.45f;
    static constexpr float ConnectVolume = 0.55f;
    static constexpr float TypeVolume = 0.35f;
    static constexpr float LineVolume = 0.35f;
    static constexpr float EndVolume = 0.6f;

    /// @param scenes, controls, input They must outlive the system.
    TransmissionSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, const engine::Scenes &scenes,
                       const engine::Controls &controls, const input::InputSource &input);

    void update(float elapsed) override;
};

} // namespace rtype::client
