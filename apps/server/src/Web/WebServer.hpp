#pragma once

#include "Common/Secret.hpp"
#include "Common/Settings.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"

#include <filesystem>
#include <memory>
#include <vector>

namespace rtype::server
{

/// What the web server of the master serves, and to whom.
struct WebServerSettings {
    /// Where it listens. Port 0 takes a free port, read back with localAddress().
    rtype::network::Address listen = rtype::network::Address::loopbackIpv4(8080);
    /// The folder of the built panel. Without it, or without its index.html, the panel shows a page that
    /// says it is not built.
    std::filesystem::path panelDir;
    /// The proxies whose X-Forwarded-For is believed.
    std::vector<Subnet> trustedProxies;
    /// The local access token, which `GET /?token=…` trades for a session (docs/architecture/web.md).
    Secret accessToken{};
};

/// The web server of the master (docs/architecture/web.md): the static panel, `/api/health`, the local access
/// token traded for a session cookie, and the security headers of every response.
///
/// It answers on threads of its own, two of them. A request that needs the master is posted to its event
/// loop, and answered from there without blocking either side: the loop never waits for the web, the web
/// never waits for the loop.
///
/// Created on the thread of the loop, and destroyed before it: stop() is called on the way, and the requests
/// still waiting for the loop are answered 503.
class WebServer
{
public:
    struct Impl;

private:
    std::unique_ptr<Impl> _impl;

public:
    /// Starts listening, and returns once the port is open.
    /// @throws std::system_error if the port cannot be opened: taken, or an address of another machine.
    WebServer(rtype::network::EventLoop &loop, WebServerSettings settings);
    ~WebServer();

    WebServer(const WebServer &) = delete;
    WebServer &operator=(const WebServer &) = delete;
    WebServer(WebServer &&) = delete;
    WebServer &operator=(WebServer &&) = delete;

    /// Where it listens, with the port it took when given 0.
    [[nodiscard]] rtype::network::Address localAddress() const;

    /// Closes the port and its connections, and waits for its threads. Does nothing the second time.
    void stop();
};

} // namespace rtype::server
