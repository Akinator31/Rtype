#include "Instance/GameThread.hpp"

#include "rtype/game/Simulation.hpp"
#include "rtype/log/Logger.hpp"

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

template <typename... Visitors>
struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

} // namespace

GameThread::GameThread(rtype::game::Rules rules, PublishFunction publish, FailureFunction onFailure)
    : _rules(rules), _publish(std::move(publish)), _onFailure(std::move(onFailure))
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

void GameThread::setInput(std::uint8_t slot, rtype::game::PlayerInput input, std::uint32_t tick)
{
    push(SetInput{slot, input, tick});
}

void GameThread::run()
{
    using Clock = std::chrono::steady_clock;

    try {
        rtype::game::Simulation simulation(_rules);
        std::array<std::uint32_t, rtype::game::MaxPlayers> lastInputs{};
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
                                       lastInputs[add.slot] = 0;
                                   }
                               },
                               [&](const RemovePlayer &remove) { simulation.removePlayer(remove.slot); },
                               [&](const SetInput &set) {
                                   if (set.slot < lastInputs.size() && set.tick >= lastInputs[set.slot]) {
                                       simulation.setInput(set.slot, set.input);
                                       lastInputs[set.slot] = set.tick;
                                   }
                               },
                           },
                           command);
            }
            commands.clear();

            simulation.step();
            _tick = simulation.tick();
            _publish(TickResult{simulation.snapshot(), lastInputs});

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
