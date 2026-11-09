#include "rtype/discord/Session.hpp"

#include "rtype/log/Logger.hpp"

// The C++ wrappers of the SDK are inline in its header, compiled once for libs/discord: here
#define DISCORDPP_IMPLEMENTATION
#include <discordpp.h>

#include <string>

namespace rtype::discord
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("discord");
    return instance;
}

} // namespace

struct Session::State {
    std::shared_ptr<discordpp::Client> client;
};

Session::Session() : _state(std::make_unique<State>()) {}

Session::~Session() = default;

bool Session::started() const
{
    return _state->client != nullptr;
}

discordpp::Client &Session::client()
{
    if (!_state->client) {
        _state->client = std::make_shared<discordpp::Client>();
        _state->client->AddLogCallback(
            [](std::string message, discordpp::LoggingSeverity severity) {
                // The SDK logs no secret at these levels: neither codes nor tokens
                while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
                    message.pop_back();
                }
                if (severity == discordpp::LoggingSeverity::Error) {
                    logger().error("{}", message);
                } else {
                    logger().warning("{}", message);
                }
            },
            discordpp::LoggingSeverity::Warning);
    }
    return *_state->client;
}

void Session::poll()
{
    if (started()) {
        discordpp::RunCallbacks();
    }
}

} // namespace rtype::discord
