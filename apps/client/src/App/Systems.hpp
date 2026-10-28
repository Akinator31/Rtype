#pragma once

#include "Game/GameSessionSystem.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/input/Devices.hpp"

namespace rtype::client
{

/// Registers the systems of the client, in the order they run each frame, after the scenes.
/// @param devices, audio, metrics, style They must outlive the engine.
/// @return The system that plays the games.
GameSessionSystem &registerClientSystems(engine::Engine &engine, input::Devices &devices,
                                         audio::AudioPlayer &audio, const engine::RendererMetrics &metrics,
                                         const engine::InterfaceStyle &style);

} // namespace rtype::client
