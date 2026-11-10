#pragma once

#include "Game/GameSessionSystem.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/engine/System/InterfaceSystem.hpp"
#include "rtype/input/InputSource.hpp"

namespace rtype::client
{

/// The systems of the client that the rest of it reads.
struct ClientSystems {
    /// The system that plays the games.
    GameSessionSystem &sessions;
    /// The menus: the widget under the pointer, which the controllers of a VR headset light their ray on.
    const engine::InterfaceSystem &interface;
};

/// Registers the systems of the client, in the order they run each frame, after the scenes.
/// @param input The input the systems read: the devices, with the controllers of a VR headset over them.
/// It must outlive the engine, as audio, metrics and style.
ClientSystems registerClientSystems(engine::Engine &engine, input::InputSource &input,
                                    audio::AudioPlayer &audio, const engine::RendererMetrics &metrics,
                                    const engine::InterfaceStyle &style);

} // namespace rtype::client
