// Tests of the web server of the master (docs/architecture/web.md): a real WebServer with its event loop on a
// thread, as in the master, asked over HTTP with libcurl; and real masters, for admin-url.txt and the rules
// they check at start.

#include "Common/Options.hpp"
#include "Common/Secret.hpp"
#include "GameServers.hpp"
#include "Http.hpp"
#include "Master/Master.hpp"
#include "Server.hpp"
#include "Web/Health.hpp"
#include "Web/LocalAccess.hpp"
#include "Web/Visitor.hpp"
#include "Web/WebServer.hpp"
#include "rtype/log/Log.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Tls.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

using namespace rtype::server;
using namespace std::chrono_literals;
using Catch::Matchers::ContainsSubstring;
using rtype::network::Address;
using rtype::network::EventLoop;
using testing::http;
using testing::HttpRequest;
using testing::HttpResponse;

namespace
{

constexpr std::string_view ContentSecurityPolicy =
    "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; "
    "font-src 'self'; connect-src 'self'; base-uri 'none'; form-action 'self'; frame-ancestors 'none'";

struct TempDirectory {
    std::filesystem::path path;

    explicit TempDirectory(std::string_view name)
        : path(std::filesystem::temp_directory_path() / ("rtype_tests_web_" + std::string(name)))
    {
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }
    ~TempDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    TempDirectory(const TempDirectory &) = delete;
    TempDirectory &operator=(const TempDirectory &) = delete;

    std::filesystem::path write(const std::string &name, std::string_view content) const
    {
        const std::filesystem::path file = path / name;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary) << content;
        return file;
    }
};

/// Captures the messages logged during a test, from every thread, at every level.
class Capture : public rtype::log::Sink
{
private:
    mutable std::mutex _mutex;
    std::string _text;

public:
    void write(const rtype::log::Record &record) override
    {
        const std::lock_guard lock(_mutex);
        _text.append(record.message).append("\n");
    }

    [[nodiscard]] std::string text() const
    {
        const std::lock_guard lock(_mutex);
        return _text;
    }
};

struct Captured {
    std::shared_ptr<Capture> sink = std::make_shared<Capture>();
    rtype::log::Level previous = rtype::log::level();

    Captured()
    {
        rtype::log::setSink(sink);
        rtype::log::setLevel(rtype::log::Level::Trace);
    }
    ~Captured()
    {
        rtype::log::setLevel(previous);
        rtype::log::setSink(nullptr);
    }
    Captured(const Captured &) = delete;
    Captured &operator=(const Captured &) = delete;
};

/// A WebServer and its loop on a thread of their own, as in the master.
struct Web {
    EventLoop loop;
    std::optional<WebServer> server;
    std::thread thread;
    std::string base;
    Secret token = newAccessToken();

    explicit Web(WebServerSettings settings = {})
    {
        settings.listen = Address::loopbackIpv4(0);
        settings.accessToken = token;
        server.emplace(loop, std::move(settings));
        thread = std::thread([this] { loop.run(); });
        base = std::format("http://{}", server->localAddress());
    }
    ~Web()
    {
        server->stop();
        loop.stop();
        thread.join();
    }
    Web(const Web &) = delete;
    Web &operator=(const Web &) = delete;

    [[nodiscard]] HttpResponse get(std::string_view path, const HttpRequest &request = {}) const
    {
        return http(base + std::string(path), request);
    }

    /// Opens the panel with the token, and returns the cookie of the session, `rtype_session=…`.
    [[nodiscard]] std::string signIn() const
    {
        const HttpResponse response = get("/?token=" + formatSecret(token));
        REQUIRE(response.status == 303);
        const std::string cookie = response.header("set-cookie");
        return cookie.substr(0, cookie.find(';'));
    }
};

WebServerSettings withPanel(const std::filesystem::path &folder)
{
    WebServerSettings settings;
    settings.panelDir = folder;
    return settings;
}

std::vector<std::string> sessionHeader(const std::string &cookie)
{
    return {"Cookie: " + cookie};
}

void checkSecurityHeaders(const HttpResponse &response)
{
    INFO("Status " << response.status << ": " << response.body);
    REQUIRE(response.status != 0);
    CHECK(response.header("content-security-policy") == ContentSecurityPolicy);
    CHECK(response.header("x-content-type-options") == "nosniff");
    CHECK(response.header("referrer-policy") == "no-referrer");
}

} // namespace

// --- The visitor behind the proxies ---

TEST_CASE("An address is in a subnet when its first bits are those of the subnet", "[server][web]")
{
    const Subnet docker{*Address::parse("172.16.0.0", 0), 12};
    CHECK(inSubnet(*Address::parse("172.16.0.1", 0), docker));
    CHECK(inSubnet(*Address::parse("172.31.255.255", 4000), docker));
    CHECK_FALSE(inSubnet(*Address::parse("172.32.0.1", 0), docker));
    CHECK_FALSE(inSubnet(*Address::parse("10.0.0.1", 0), docker));
    // An IPv4 address seen by a listener on [::]
    CHECK(inSubnet(*Address::parse("::ffff:172.17.0.2", 0), docker));
    CHECK_FALSE(inSubnet(*Address::parse("::1", 0), docker));

    const Subnet one{*Address::parse("::1", 0), 128};
    CHECK(inSubnet(*Address::parse("::1", 0), one));
    CHECK_FALSE(inSubnet(*Address::parse("::2", 0), one));
    CHECK(inSubnet(*Address::parse("203.0.113.9", 0), Subnet{*Address::parse("0.0.0.0", 0), 0}));
}

TEST_CASE("X-Forwarded-For is read from the right, and only from a trusted proxy", "[server][web]")
{
    const std::vector<Subnet> proxies = {{*Address::parse("172.16.0.0", 0), 12},
                                         {*Address::parse("127.0.0.1", 0), 32}};
    const Address proxy = *Address::parse("172.17.0.2", 51000);
    const Address stranger = *Address::parse("198.51.100.4", 51000);

    // Without a proxy, the peer is the visitor, whatever it writes
    CHECK(visitorAddress(stranger, "203.0.113.9", proxies) == *Address::parse("198.51.100.4", 0));
    CHECK(visitorAddress(stranger, "", proxies) == *Address::parse("198.51.100.4", 0));
    CHECK(visitorAddress(stranger, "203.0.113.9", {}) == *Address::parse("198.51.100.4", 0));
    // One proxy: the address it appended
    CHECK(visitorAddress(proxy, "203.0.113.9", proxies) == *Address::parse("203.0.113.9", 0));
    // The visitor wrote the entry on the left: never believed
    CHECK(visitorAddress(proxy, "6.6.6.6, 203.0.113.9", proxies) == *Address::parse("203.0.113.9", 0));
    // Two proxies, Traefik behind a local nginx
    CHECK(visitorAddress(*Address::parse("127.0.0.1", 0), "6.6.6.6, 203.0.113.9, 172.18.0.5", proxies) ==
          *Address::parse("203.0.113.9", 0));
    // Only proxies: the farthest one
    CHECK(visitorAddress(proxy, "172.18.0.5", proxies) == *Address::parse("172.18.0.5", 0));
    // An entry that is not an address stops at the proxy that wrote it
    CHECK(visitorAddress(proxy, "203.0.113.9, nonsense", proxies) == *Address::parse("172.17.0.2", 0));
    CHECK(visitorAddress(proxy, "", proxies) == *Address::parse("172.17.0.2", 0));
    CHECK(visitorAddress(proxy, " 2001:db8::7 ", proxies) == *Address::parse("2001:db8::7", 0));
}

// --- The web server ---

TEST_CASE("The health of the master is answered by its loop, without blocking either side", "[server][web]")
{
    Web web;

    const HttpResponse health = web.get("/api/health");
    CHECK(health.status == 200);
    CHECK(health.header("content-type") == "application/json");
    CHECK(health.body == R"({"status":"ok"})");

    // The loop busy: the answer waits for it, and comes once the loop has worked
    std::mutex mutex;
    std::condition_variable condition;
    bool busy = true;
    std::atomic<bool> worked = false;
    web.loop.post([&] {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return !busy; });
        worked = true;
    });
    auto answer = std::async(std::launch::async, [&] { return web.get("/api/health"); });
    CHECK(answer.wait_for(200ms) == std::future_status::timeout);
    // Meanwhile, what does not need the loop is answered
    CHECK(web.get("/api/session").status == 401);
    {
        const std::lock_guard lock(mutex);
        busy = false;
    }
    condition.notify_all();
    const HttpResponse late = answer.get();
    CHECK(late.status == 200);
    CHECK(worked);
}

TEST_CASE("The answer of /api/health says whether the master is healthy", "[server][web]")
{
    CHECK_FALSE(
        healthProblem("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\"}"));
    CHECK_FALSE(healthProblem("HTTP/1.0 200 OK\r\n\r\n{\"status\":\"ok\"}"));

    const std::vector<std::pair<std::string_view, std::string_view>> unhealthy{
        {"", "without answering"},
        {"SSH-2.0-OpenSSH_9.9\r\n", "did not answer in HTTP"},
        {"HTTP/1.1 503 Service Unavailable\r\n\r\n{\"error\":\"stopping\"}",
         "answered 503 Service Unavailable"},
        {"HTTP/1.1 404 Not Found\r\n\r\n", "answered 404"},
        {"HTTP/1.1 200 OK\r\n\r\n<!doctype html>", "without saying it is healthy"},
        // Cut before its body
        {"HTTP/1.1 200 OK\r\nContent-Length: 15\r\n", "without saying it is healthy"},
    };
    for (const auto &[response, problem] : unhealthy) {
        CAPTURE(response);
        const std::optional<std::string> found = healthProblem(response);
        REQUIRE(found);
        CHECK_THAT(*found, ContainsSubstring(std::string(problem)));
    }
}

TEST_CASE("The health check asks the loop of the master, and gives up on one that is stuck", "[server][web]")
{
    Web web;
    CHECK(askHealth(web.server->localAddress()) == std::nullopt);

    // The loop stuck past the time the check waits: not healthy
    std::promise<void> release;
    std::shared_future<void> released = release.get_future().share();
    web.loop.post([released] { released.wait(); });
    const std::optional<std::string> stuck = askHealth(web.server->localAddress(), 300ms);
    release.set_value();
    REQUIRE(stuck);
    CHECK_THAT(*stuck, ContainsSubstring("did not answer within"));
    // Healthy again once it works
    CHECK(askHealth(web.server->localAddress()) == std::nullopt);
}

TEST_CASE("The health check of a master that does not run fails at once", "[server][web]")
{
    std::uint16_t port = 0;
    {
        EventLoop loop;
        {
            const rtype::network::TcpListener free(loop, Address::loopbackIpv4(0));
            port = free.localAddress().port();
        }
        loop.poll();
    }
    const auto start = std::chrono::steady_clock::now();
    const std::optional<std::string> problem = askHealth(Address::loopbackIpv4(port));
    REQUIRE(problem);
    CHECK_THAT(*problem, ContainsSubstring(std::format("cannot reach the panel on 127.0.0.1:{}", port)));
    CHECK(std::chrono::steady_clock::now() - start < HealthTimeout);
}

TEST_CASE("The option --health finds the panel at the address of the settings file", "[server][web]")
{
    const TempDirectory directory("health");
    Web web;
    const Captured captured;
    std::ostringstream out;
    std::istringstream in;
    const auto health = [&](const std::string &listen) {
        out.str("");
        const std::string file =
            directory.write("server.toml", std::format("[web]\nlisten = \"{}\"\n", listen)).string();
        return run(std::vector<std::string_view>{"--health", "--settings", file}, out, in);
    };

    CHECK(health(std::format("{}", web.server->localAddress())) == Success);
    CHECK(out.str() == "healthy\n");
    // A panel that listens everywhere, with accounts, is asked on the loopback
    MasterOptions everywhere;
    everywhere.settings.web.listen = Address::anyIpv4(web.server->localAddress().port());
    CHECK(showHealth(everywhere, out) == Success);

    // No port to ask
    CHECK(health("127.0.0.1:0") == Failure);
    CHECK_THAT(captured.sink->text(), ContainsSubstring("--health cannot find it"));
    CHECK(out.str().empty());

    // The master gone
    const std::string address = std::format("{}", web.server->localAddress());
    web.server->stop();
    CHECK(health(address) == Failure);
    CHECK_THAT(captured.sink->text(), ContainsSubstring("Not healthy: cannot reach the panel on " + address));
    CHECK(out.str().empty());
}

TEST_CASE("A request still waiting for the loop when the server stops is answered 503", "[server][web]")
{
    std::optional<Web> web(std::in_place);
    std::promise<void> release;
    std::shared_future<void> released = release.get_future().share();
    web->loop.post([released] { released.wait(); });
    auto answer = std::async(std::launch::async, [&] { return web->get("/api/health"); });
    std::this_thread::sleep_for(200ms);

    web->server->stop();
    const HttpResponse response = answer.get();
    CHECK(response.status == 503);
    release.set_value();
    web.reset();
}

TEST_CASE("The files of the panel are served, and its index.html for the paths of its pages", "[server][web]")
{
    const TempDirectory panel("panel");
    panel.write("index.html", "<!doctype html><title>Panel</title>");
    panel.write("assets/app.js", "console.log('panel');");
    panel.write("assets/style.CSS", "body {}");
    panel.write("fonts/Orbitron.woff2", std::string("wOF2\0\1", 6));
    Web web(withPanel(panel.path));

    const HttpResponse index = web.get("/");
    CHECK(index.status == 200);
    CHECK(index.body == "<!doctype html><title>Panel</title>");
    CHECK(index.header("content-type") == "text/html; charset=utf-8");
    CHECK(index.header("cache-control") == "no-cache");

    const HttpResponse script = web.get("/assets/app.js");
    CHECK(script.status == 200);
    CHECK(script.body == "console.log('panel');");
    CHECK(script.header("content-type") == "text/javascript; charset=utf-8");
    CHECK(web.get("/assets/style.CSS").header("content-type") == "text/css; charset=utf-8");
    const HttpResponse font = web.get("/fonts/Orbitron.woff2");
    CHECK(font.header("content-type") == "font/woff2");
    CHECK(font.body == std::string("wOF2\0\1", 6));

    // A page of the panel: its index.html, which routes it
    for (const std::string_view page : {"/games", "/games/7", "/players/", "/index"}) {
        INFO(page);
        const HttpResponse response = web.get(page);
        CHECK(response.status == 200);
        CHECK(response.body == "<!doctype html><title>Panel</title>");
    }
    // A file that is not there is not a page
    CHECK(web.get("/assets/missing.js").status == 404);
    // Only GET
    const HttpResponse post = web.get("/assets/app.js", {.method = "POST", .headers = {}});
    CHECK(post.status == 405);
    CHECK(post.header("allow") == "GET");
}

TEST_CASE("A connection kept alive gets each page and file it asks, as a browser does", "[server][web]")
{
    const TempDirectory panel("panel_keep_alive");
    panel.write("index.html", "<!doctype html><title>Panel</title>");
    panel.write("assets/app.js", "console.log('panel');");
    Web web(withPanel(panel.path));

    // The token traded, then the page it redirects to, then its script, then other pages: one connection, as
    // a browser keeps it. Crow ends the response of the catch-all route itself: ended twice, the next request
    // on the connection got its bare 404
    bool reused = false;
    const std::vector<HttpResponse> responses = testing::httpSequence(
        {web.base + "/?token=" + formatSecret(web.token), web.base + "/", web.base + "/assets/app.js",
         web.base + "/games", web.base + "/players", web.base + "/api/health", web.base + "/actions"},
        &reused);
    CHECK(reused);
    REQUIRE(responses.size() == 7);
    CHECK(responses[0].status == 303);
    for (const std::size_t page : {1U, 3U, 4U, 6U}) {
        INFO(page);
        CHECK(responses[page].status == 200);
        CHECK(responses[page].body == "<!doctype html><title>Panel</title>");
    }
    CHECK(responses[2].body == "console.log('panel');");
    CHECK(responses[5].status == 200);
}

TEST_CASE("Without its files, the panel says it is not built", "[server][web]")
{
    const TempDirectory directory("unbuilt");
    std::filesystem::create_directories(directory.path / "empty");
    for (const std::filesystem::path &folder : {directory.path / "missing", directory.path / "empty"}) {
        INFO(folder.string());
        Web web(withPanel(folder));
        for (const std::string_view path : {"/", "/games/7"}) {
            const HttpResponse response = web.get(path);
            CHECK(response.status == 200);
            CHECK(response.header("content-type") == "text/html; charset=utf-8");
            CHECK_THAT(response.body, ContainsSubstring("The panel is not built"));
        }
    }
}

TEST_CASE("No address reaches a file out of the folder of the panel", "[server][web]")
{
    const TempDirectory directory("escape");
    directory.write("secret.txt", "the secret");
    directory.write("panel/index.html", "index");
    Web web(withPanel(directory.path / "panel"));

    for (const std::string_view path :
         {"/../secret.txt", "/%2e%2e/secret.txt", "/%2E%2E%2Fsecret.txt", "/assets/../../secret.txt",
          "/..%5csecret.txt", "/%00", "/%zz", "/./../secret.txt"}) {
        INFO(path);
        const HttpResponse response = web.get(path);
        CHECK(response.status != 200);
        CHECK_THAT(response.body, !ContainsSubstring("the secret"));
    }
}

TEST_CASE("Every response carries the headers of security", "[server][web]")
{
    const TempDirectory panel("headers");
    panel.write("index.html", "index");
    panel.write("app.js", "js");
    Web web(withPanel(panel.path));
    const std::string session = web.signIn();

    checkSecurityHeaders(web.get("/"));
    checkSecurityHeaders(web.get("/app.js"));
    checkSecurityHeaders(web.get("/games/7"));
    checkSecurityHeaders(web.get("/missing.js"));
    checkSecurityHeaders(web.get("/../x"));
    checkSecurityHeaders(web.get("/", {.method = "DELETE", .headers = {}}));
    checkSecurityHeaders(web.get("/api/health"));
    checkSecurityHeaders(web.get("/api/health", {.method = "POST", .headers = {}}));
    checkSecurityHeaders(web.get("/api/session"));
    checkSecurityHeaders(web.get("/api/session", {.method = "GET", .headers = sessionHeader(session)}));
    checkSecurityHeaders(web.get("/api/nothing"));
    checkSecurityHeaders(web.get("/api/nothing", {.method = "GET", .headers = sessionHeader(session)}));
    checkSecurityHeaders(web.get("/?token=" + formatSecret(web.token)));
    checkSecurityHeaders(web.get("/?token=wrong"));

    const TempDirectory unbuilt("headers_unbuilt");
    Web bare(withPanel(unbuilt.path / "missing"));
    checkSecurityHeaders(bare.get("/"));
    CHECK(web.get("/api/health").header("cache-control") == "no-store");
}

TEST_CASE("The access token is traded for a session cookie, then the address without it", "[server][web]")
{
    Web web;

    const HttpResponse opened = web.get("/?token=" + formatSecret(web.token));
    CHECK(opened.status == 303);
    CHECK(opened.header("location") == "/");
    CHECK(opened.header("cache-control") == "no-store");
    const std::string cookie = opened.header("set-cookie");
    CHECK(cookie.starts_with("rtype_session="));
    CHECK_THAT(cookie, ContainsSubstring("; Path=/"));
    CHECK_THAT(cookie, ContainsSubstring("; HttpOnly"));
    CHECK_THAT(cookie, ContainsSubstring("; SameSite=Strict"));
    CHECK_THAT(cookie, ContainsSubstring("; Max-Age=43200"));
    // The session is not the token
    CHECK_THAT(cookie, !ContainsSubstring(formatSecret(web.token)));

    const std::string session = cookie.substr(0, cookie.find(';'));
    const HttpResponse described =
        web.get("/api/session", {.method = "GET", .headers = sessionHeader(session)});
    CHECK(described.status == 200);
    CHECK(described.header("content-type") == "application/json");
    // Its CSRF token, which every POST of the API carries: neither the session nor the access token
    REQUIRE(described.body.starts_with(R"({"csrfToken":")"));
    const std::string csrf = described.body.substr(14, 64);
    CHECK(parseSecret(csrf).has_value());
    CHECK(described.body == R"({"csrfToken":")" + csrf + R"("})");
    CHECK(csrf != formatSecret(web.token));
    CHECK_THAT(session, !ContainsSubstring(csrf));
    CHECK(web.get("/api/session", {.method = "GET", .headers = {"Cookie: theme=dark; " + session + "; a=b"}})
              .body == described.body);
    // Every other route of the API needs it
    CHECK(web.get("/api/nothing").status == 401);
    CHECK(web.get("/api/nothing", {.method = "GET", .headers = sessionHeader(session)}).status == 404);
    CHECK(web.get("/api/health").status == 200);

    CHECK(web.get("/api/session").status == 401);
    CHECK(
        web.get("/api/session", {.method = "GET",
                                 .headers = sessionHeader("rtype_session=" + formatSecret(newAccessToken()))})
            .status == 401);
    CHECK(web.get("/api/session",
                  {.method = "GET", .headers = sessionHeader("rtype_session=" + formatSecret(web.token))})
              .status == 401);
    CHECK(web.get("/api/session", {.method = "GET", .headers = sessionHeader("rtype_session=nonsense")})
              .status == 401);

    // Opened again: a second session, the first one still valid
    const std::string second = web.signIn();
    CHECK(second != session);
    CHECK(web.get("/api/session", {.method = "GET", .headers = sessionHeader(session)}).status == 200);
    CHECK(web.get("/api/session", {.method = "GET", .headers = sessionHeader(second)}).status == 200);
}

TEST_CASE("A wrong access token is refused, without a cookie", "[server][web]")
{
    Web web;
    std::string wrong = formatSecret(web.token);
    wrong[0] = wrong[0] == '0' ? '1' : '0';

    for (const std::string &token : {wrong, std::string("nonsense"), std::string(""),
                                     formatSecret(web.token) + "0", "%20" + formatSecret(web.token)}) {
        INFO(token);
        const HttpResponse response = web.get("/?token=" + token);
        CHECK(response.status == 403);
        CHECK(response.header("set-cookie").empty());
        CHECK_THAT(response.body, ContainsSubstring("admin-url.txt"));
    }
}

TEST_CASE("Neither the access token nor a session reaches the logs", "[server][web]")
{
    const Captured captured;
    std::string session;
    std::string wrong = formatSecret(newAccessToken());
    std::string token;
    {
        Web web;
        token = formatSecret(web.token);
        session = web.signIn();
        CHECK(web.get("/api/session", {.method = "GET", .headers = sessionHeader(session)}).status == 200);
        CHECK(web.get("/?token=" + wrong).status == 403);
        CHECK(web.get("/games?token=" + token).status == 200);
    }

    const std::string logs = captured.sink->text();
    // Logged, at every level, by Crow and by the server
    CHECK_THAT(logs, ContainsSubstring("GET / 303 from 127.0.0.1"));
    CHECK_THAT(logs, ContainsSubstring("Request: "));
    CHECK_THAT(logs, ContainsSubstring("opened with the local access token"));
    CHECK_THAT(logs, ContainsSubstring("wrong access token"));
    CHECK_THAT(logs, !ContainsSubstring(token));
    CHECK_THAT(logs, !ContainsSubstring(wrong));
    CHECK_THAT(logs, !ContainsSubstring(session.substr(session.find('=') + 1)));
}

TEST_CASE("A forged X-Forwarded-For is never believed", "[server][web]")
{
    const Captured captured;
    {
        // From an address that is not a trusted proxy: the header is ignored
        Web web;
        CHECK(web.get("/", {.method = "GET", .headers = {"X-Forwarded-For: 203.0.113.9"}}).status == 200);
    }
    CHECK_THAT(captured.sink->text(), ContainsSubstring("GET / 200 from 127.0.0.1"));
    CHECK_THAT(captured.sink->text(), !ContainsSubstring("203.0.113.9"));

    {
        // From a trusted proxy: the address it appended, never the one on its left
        WebServerSettings settings;
        settings.trustedProxies = {{*Address::parse("127.0.0.0", 0), 8}};
        Web web(settings);
        CHECK(web.get("/games", {.method = "GET", .headers = {"X-Forwarded-For: 6.6.6.6, 198.51.100.7"}})
                  .status == 200);
        // Two headers: their order is lost, none is believed
        CHECK(web.get("/players", {.method = "GET",
                                   .headers = {"X-Forwarded-For: 6.6.6.6", "X-Forwarded-For: 198.51.100.8"}})
                  .status == 200);
    }
    CHECK_THAT(captured.sink->text(), ContainsSubstring("GET /games 200 from 198.51.100.7"));
    CHECK_THAT(captured.sink->text(), ContainsSubstring("GET /players 200 from 127.0.0.1"));
    CHECK_THAT(captured.sink->text(), !ContainsSubstring("6.6.6.6"));
}

TEST_CASE("A port taken stops the web server at start", "[server][web]")
{
    EventLoop loop;
    const rtype::network::TcpListener taken(loop, Address::loopbackIpv4(0));
    WebServerSettings settings;
    settings.listen = taken.localAddress();

    CHECK_THROWS_AS(WebServer(loop, settings), std::system_error);
}

TEST_CASE("The address of the panel in admin-url.txt", "[server][web]")
{
    const Secret token = newAccessToken();
    CHECK(reachableAddress(Address::anyIpv4(8080)) == Address::loopbackIpv4(8080));
    CHECK(reachableAddress(Address::anyIpv6(8080)) == *Address::parse("::1", 8080));
    CHECK(reachableAddress(*Address::parse("192.0.2.7", 8080)) == *Address::parse("192.0.2.7", 8080));
    CHECK(panelAddress(Address::loopbackIpv4(8080), "") == "http://127.0.0.1:8080");
    CHECK(panelAddress(Address::anyIpv4(8080), "") == "http://127.0.0.1:8080");
    CHECK(panelAddress(Address::anyIpv6(8080), "") == "http://[::1]:8080");
    CHECK(panelAddress(Address::anyIpv4(8080), "https://admin.example.org") == "https://admin.example.org");
    CHECK(accessUrl("http://127.0.0.1:8080", token) == "http://127.0.0.1:8080/?token=" + formatSecret(token));

    const TempDirectory directory("url");
    const std::filesystem::path file = directory.path / AdminUrlFileName;
    directory.write(std::string(AdminUrlFileName), "old");
#ifndef _WIN32
    std::filesystem::permissions(file, std::filesystem::perms::all);
#endif
    writeAccessUrl(file, "http://127.0.0.1:8080/?token=x");
    std::ifstream stream(file);
    std::string line;
    std::getline(stream, line);
    CHECK(line == "http://127.0.0.1:8080/?token=x");
#ifndef _WIN32
    // Replaced, not written through: readable by its owner only
    CHECK(std::filesystem::status(file).permissions() ==
          (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
#endif
}

// --- In the master ---

namespace
{

/// The arguments of a master that keeps everything in `directory`, its panel on a free port.
std::vector<std::string> masterArguments(const TempDirectory &directory, const std::string &settings)
{
    const std::uint16_t gamePort = testing::freePorts(1);
    return {"--port",       "0",
            "--bind",       "127.0.0.1",
            "--game-ports", std::format("{}-{}", gamePort, gamePort),
            "--key",        (directory.path / "server.key").string(),
            "--database",   "sqlite:" + (directory.path / "rtype.db").string(),
            "--config",     RTYPE_TEST_CONFIG,
            "--settings",   settings};
}

int runMasterWith(const std::vector<std::string> &arguments)
{
    const std::vector<std::string_view> views(arguments.begin(), arguments.end());
    std::ostringstream out;
    std::istringstream in;
    return run(views, out, in);
}

} // namespace

TEST_CASE("A master writes admin-url.txt beside its key, whose address opens the panel", "[server][web]")
{
    const TempDirectory directory("master");
    const std::string settings = testing::webOnAnyPort(directory.path / "web.toml").string();
    const std::vector<std::string> arguments = masterArguments(directory, settings);
    const Captured captured;

    std::atomic<int> code = -1;
    std::thread master([&] { code = runMasterWith(arguments); });
    // Stops the master however the test ends
    struct Stop {
        std::thread &master;
        std::atomic<int> &code;
        ~Stop()
        {
            if (master.joinable()) {
                if (code == -1) {
                    std::raise(SIGTERM);
                }
                master.join();
            }
        }
    } stop{master, code};

    const std::filesystem::path file = directory.path / AdminUrlFileName;
    std::string url;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (url.empty() && code == -1 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(20ms);
        std::ifstream stream(file);
        std::string line;
        if (std::getline(stream, line) && !stream.eof()) {
            url = line;
        }
    }
    REQUIRE_FALSE(url.empty());
#ifndef _WIN32
    CHECK(std::filesystem::status(file).permissions() ==
          (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
#endif
    REQUIRE(url.starts_with("http://127.0.0.1:"));
    const std::string base = url.substr(0, url.find("/?token="));
    // Answered by the loop: it runs, the stop signals are handled
    REQUIRE(http(base + "/api/health").status == 200);
    // What the health check of the Docker image asks
    const auto port = static_cast<std::uint16_t>(std::stoul(base.substr(base.rfind(':') + 1)));
    CHECK(askHealth(Address::loopbackIpv4(port)) == std::nullopt);

    // The done-when of the step: the page "panel not built", with a session cookie
    const HttpResponse opened = http(url);
    REQUIRE(opened.status == 303);
    const std::string cookie = opened.header("set-cookie");
    const std::string session = cookie.substr(0, cookie.find(';'));
    const HttpResponse page = http(base + "/", {.method = "GET", .headers = sessionHeader(session)});
    CHECK(page.status == 200);
    CHECK_THAT(page.body, ContainsSubstring("The panel is not built"));
    CHECK(http(base + "/api/session", {.method = "GET", .headers = sessionHeader(session)}).status == 200);
    // The done-when of the administration API: its routes answer from the master
    const HttpResponse server =
        http(base + "/api/server", {.method = "GET", .headers = sessionHeader(session)});
    CHECK(server.status == 200);
    CHECK_THAT(server.body, ContainsSubstring(R"("version":")"));
    const HttpResponse games =
        http(base + "/api/games", {.method = "GET", .headers = sessionHeader(session)});
    CHECK(games.status == 200);
    CHECK(games.body == "[]");
    CHECK(http(base + "/api/server").status == 401);

    std::raise(SIGTERM);
    master.join();
    CHECK(code == Success);
    const std::string logs = captured.sink->text();
    CHECK_THAT(logs, ContainsSubstring(file.string()));
    CHECK_THAT(logs, !ContainsSubstring(url.substr(url.find('=') + 1)));
}

TEST_CASE("A master does not start when its panel cannot open its port", "[server][web]")
{
    const TempDirectory directory("taken");
    EventLoop loop;
    const rtype::network::TcpListener taken(loop, Address::loopbackIpv4(0));
    const std::string settings =
        directory.write("web.toml", std::format("[web]\nlisten = \"{}\"\n", taken.localAddress())).string();
    std::vector<std::string> arguments = masterArguments(directory, settings);
    arguments.emplace_back("--check");
    const Captured captured;

    CHECK(runMasterWith(arguments) == Failure);
    CHECK_THAT(captured.sink->text(), ContainsSubstring("Cannot serve the panel on"));
}

TEST_CASE("A master serves its panel on the port its settings give", "[server][web]")
{
    const TempDirectory directory("port");
    std::uint16_t port = 0;
    {
        EventLoop loop;
        {
            const rtype::network::TcpListener free(loop, Address::loopbackIpv4(0));
            port = free.localAddress().port();
        }
        loop.poll();
    }
    const std::string settings =
        directory.write("web.toml", std::format("[web]\nlisten = \"127.0.0.1:{}\"\n", port)).string();
    std::vector<std::string> arguments = masterArguments(directory, settings);
    arguments.emplace_back("--check");
    const Captured captured;

    // Before its loop runs: the port is checked, then given to Crow
    CHECK(runMasterWith(arguments) == Success);
    CHECK_THAT(captured.sink->text(), ContainsSubstring(std::format("Panel on HTTP 127.0.0.1:{}", port)));
}

TEST_CASE("A master does not start when the folder of its panel is missing", "[server][web]")
{
    const TempDirectory directory("folder");
    const std::string settings =
        directory.write("web.toml", "[web]\nlisten = \"127.0.0.1:0\"\npanel_dir = \"missing\"\n").string();
    std::vector<std::string> arguments = masterArguments(directory, settings);
    arguments.emplace_back("--check");

    CHECK(runMasterWith(arguments) == Failure);
    std::filesystem::create_directories(directory.path / "missing");
    CHECK(runMasterWith(arguments) == Success);
}

TEST_CASE("Without accounts, a master serves its panel on the loopback only", "[server][web]")
{
    const TempDirectory directory("loopback");
    std::ofstream(directory.path / "server.key")
        << rtype::network::ServerIdentity::generate().privateKeyPem();
    const std::string settings = testing::webOnAnyPort(directory.path / "web.toml").string();
    std::vector<std::string> arguments = masterArguments(directory, settings);
    arguments.emplace_back("--check");
    const std::vector<std::string_view> views(arguments.begin(), arguments.end());
    Options options = parseOptions(views, loadSettings(settings, rtype::log::Logger("tests")));
    REQUIRE(runMaster(options.master) == Success);

    // Past the settings file, which refuses it already: the master checks it again
    options.master.settings.web.listen = Address::anyIpv4(0);
    CHECK(runMaster(options.master) == Failure);
    options.master.settings.web.listen = *Address::parse("::", 0);
    CHECK(runMaster(options.master) == Failure);
    // With accounts, the panel has its sign-in: it may listen beyond
    options.master.settings.accounts = AccountsSettings{};
    CHECK(runMaster(options.master) == Success);
}
