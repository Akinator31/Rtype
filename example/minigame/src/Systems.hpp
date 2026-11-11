#pragma once

#include "BallSystem.hpp"
#include "Breaker.hpp"
#include "Looks.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/input/InputSource.hpp"

#include <functional>

namespace orbit
{

/// Registers the systems of the arena on the ECS of the engine, in the order of a frame: the paddle, the
/// physics (movement, then collisions), the ball and the rules, the effects fading out and the lives, then
/// the places of the children (the lights of the ball and of the paddle). The arena only moves while its
/// scene is played, not under the pause. Everything given must outlive the engine.
void registerGameSystems(rtype::engine::Engine &engine, rtype::audio::AudioPlayer &audio, Match &match,
                         BallSounds sounds, const Looks &looks, std::function<void()> onGameOver);

/// Registers every system of the game: the menus first, which read the input before the arena, then those of
/// registerGameSystems(), then the sky, the sounds, the texts, the tweens, the places the mouse clicks on and
/// the places of the interface. Everything given must outlive the engine.
void registerSystems(rtype::engine::Engine &engine, rtype::input::InputSource &input,
                     rtype::audio::AudioPlayer &audio, const rtype::engine::Metrics &metrics,
                     const rtype::engine::InterfaceStyle &style, Match &match, BallSounds sounds,
                     const Looks &looks, std::function<void()> onGameOver);

} // namespace orbit
