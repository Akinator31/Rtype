#include "Web/Health.hpp"

#include "Common/Options.hpp"
#include "Web/LocalAccess.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/TcpConnection.hpp"

#include <cstddef>
#include <format>
#include <memory>
#include <span>
#include <system_error>
#include <thread>

namespace rtype::server
{

namespace
{

/// What the master answers once healthy: WebServer.cpp, the route /api/health.
constexpr std::string_view HealthyBody = R"({"status":"ok"})";

} // namespace

std::optional<std::string> healthProblem(std::string_view response)
{
    if (response.empty()) {
        return "the master closed the connection without answering";
    }
    const std::size_t lineEnd = response.find("\r\n");
    const std::string_view statusLine = response.substr(0, lineEnd);
    // HTTP/1.1 200 OK
    const std::size_t space = statusLine.find(' ');
    if (!statusLine.starts_with("HTTP/") || space == std::string_view::npos) {
        return "the master did not answer in HTTP";
    }
    const std::string_view status = statusLine.substr(space + 1, 3);
    if (status != "200") {
        return std::format("the master answered {}", statusLine.substr(space + 1));
    }
    const std::size_t headersEnd = response.find("\r\n\r\n");
    if (headersEnd == std::string_view::npos || response.substr(headersEnd + 4) != HealthyBody) {
        return "the master answered 200, without saying it is healthy";
    }
    return std::nullopt;
}

std::optional<std::string> askHealth(const rtype::network::Address &panel, std::chrono::milliseconds timeout)
{
    struct State {
        std::optional<rtype::network::TcpConnection> connection;
        std::string response;
        std::optional<std::string> failed;
        bool done = false;
    };
    const auto state = std::make_shared<State>();
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    rtype::network::EventLoop loop;
    rtype::network::TcpConnection::connect(
        loop, panel,
        [state, panel](std::error_code error, std::optional<rtype::network::TcpConnection> connection) {
            if (error || !connection) {
                state->failed = std::format("cannot reach the panel on {}: {}", panel, error.message());
                state->done = true;
                return;
            }
            state->connection = std::move(connection);
            state->connection->start(
                [state](std::span<const std::byte> bytes) {
                    state->response.append(reinterpret_cast<const char *>(bytes.data()), bytes.size());
                },
                // Connection: close, the master closes it once its answer is sent
                [state](std::error_code) { state->done = true; });
            const std::string request =
                std::format("GET /api/health HTTP/1.1\r\nHost: {}\r\nConnection: close\r\n\r\n", panel);
            state->connection->send(std::as_bytes(std::span(request)));
        },
        timeout);

    while (!state->done && std::chrono::steady_clock::now() < deadline) {
        if (loop.poll() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    if (state->connection) {
        state->connection->close();
    }
    if (state->failed) {
        return state->failed;
    }
    if (!state->done) {
        return std::format("the master did not answer within {} on {}",
                           std::chrono::duration_cast<std::chrono::seconds>(timeout), panel);
    }
    return healthProblem(state->response);
}

int showHealth(const MasterOptions &options, std::ostream &out)
{
    const rtype::log::Logger logger("health");
    const rtype::network::Address &listening = options.settings.web.listen;
    if (listening.port() == 0) {
        logger.error("The panel listens on a port the system chooses (listen = \"{}\" in [web]): --health "
                     "cannot find it",
                     listening);
        return Failure;
    }
    if (const std::optional<std::string> problem = askHealth(reachableAddress(listening))) {
        logger.error("Not healthy: {}", *problem);
        return Failure;
    }
    out << "healthy\n";
    return Success;
}

} // namespace rtype::server
