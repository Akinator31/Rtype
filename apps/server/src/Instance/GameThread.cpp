#include "Instance/GameThread.hpp"

#include "Instance/InputBuffer.hpp"
#include "rtype/game/Simulation.hpp"
#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace rtype::server
{

namespace
{

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("game");
    return instance;
}

/// The weapon of a ship in a SNAPSHOT: each count within what a u16 holds.
rtype::game::WeaponState toWeaponState(const rtype::game::Armed &armed)
{
    const auto clamp = [](std::uint32_t value) {
        return static_cast<std::uint16_t>(std::min<std::uint32_t>(value, 0xFFFF));
    };
    return {clamp(armed.cooldown), clamp(armed.ammo), clamp(armed.reloadLeft)};
}

template <typename... Visitors>
struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

} // namespace

GameThread::GameThread(rtype::game::Rules rules, rtype::game::GameConfig config, PublishFunction publish,
                       FailureFunction onFailure)
    : _rules(rules), _config(std::move(config)), _publish(std::move(publish)),
      _onFailure(std::move(onFailure))
{
}

GameThread::~GameThread()
{
    stop();
}

void GameThread::start()
{
    _thread = std::thread([this] { run(); });
}

void GameThread::stop()
{
    {
        const std::scoped_lock lock(_mutex);
        _stopping = true;
    }
    _wake.notify_all();
    if (_thread.joinable()) {
        _thread.join();
    }
}

void GameThread::push(Command command)
{
    const std::scoped_lock lock(_mutex);
    _commands.push_back(command);
}

void GameThread::addPlayer(std::uint8_t slot)
{
    push(AddPlayer{slot});
}

void GameThread::removePlayer(std::uint8_t slot)
{
    push(RemovePlayer{slot});
}

void GameThread::receiveInputs(std::uint8_t slot, std::uint32_t tick,
                               std::vector<rtype::game::PlayerInput> inputs)
{
    push(ReceiveInputs{slot, tick, std::move(inputs)});
}

void GameThread::run()
{
    using Clock = std::chrono::steady_clock;

    try {
        rtype::game::Simulation simulation(_rules, _config);
        std::array<InputBuffer, rtype::game::MaxPlayers> buffers{};
        std::vector<Command> commands;
        Clock::time_point next = Clock::now();

        for (;;) {
            {
                std::unique_lock lock(_mutex);
                // Waits for the tick, unless asked to stop
                if (_wake.wait_until(lock, next, [this] { return _stopping; })) {
                    return;
                }
                commands.swap(_commands);
            }

            for (const Command &command : commands) {
                std::visit(Overloaded{
                               [&](const AddPlayer &add) {
                                   if (!simulation.hasPlayer(add.slot)) {
                                       simulation.addPlayer(add.slot);
                                       buffers[add.slot].reset();
                                   }
                               },
                               [&](const RemovePlayer &remove) {
                                   simulation.removePlayer(remove.slot);
                                   if (remove.slot < buffers.size()) {
                                       buffers[remove.slot].reset();
                                   }
                               },
                               [&](const ReceiveInputs &received) {
                                   if (received.slot < buffers.size()) {
                                       buffers[received.slot].receive(received.tick, received.inputs,
                                                                      simulation.tick());
                                   }
                               },
                           },
                           command);
            }
            commands.clear();

            // The input of each player for the tick about to be simulated
            const std::uint32_t tick = simulation.tick() + 1;
            TickResult result;
            for (std::uint8_t slot = 0; slot < buffers.size(); ++slot) {
                if (simulation.hasPlayer(slot)) {
                    simulation.setInput(slot, buffers[slot].take(tick));
                }
                result.lastInputs[slot] = buffers[slot].lastApplied();
                result.inputLeads[slot] = buffers[slot].takeLead();
            }

            simulation.step();
            _tick = simulation.tick();
            for (std::uint8_t slot = 0; slot < buffers.size(); ++slot) {
                if (const std::optional<rtype::game::Armed> armed = simulation.weaponOf(slot)) {
                    result.weapons[slot] = toWeaponState(*armed);
                }
            }
            result.snapshot = simulation.snapshot();
            _publish(std::move(result));

            next += TickInterval;
            const Clock::time_point now = Clock::now();
            if (now - next > MaxLag) {
                logger().warning("The game is {} ms behind: it goes on from now",
                                 std::chrono::duration_cast<std::chrono::milliseconds>(now - next).count());
                next = now;
            }
        }
    } catch (const std::exception &error) {
        logger().error("The game stopped on an error: {}", error.what());
        if (_onFailure) {
            _onFailure();
        }
    }
}

} // namespace rtype::server
