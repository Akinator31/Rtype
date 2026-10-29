#include "Web/WebServer.hpp"

#include "Web/LocalAccess.hpp"
#include "Web/Visitor.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/TcpListener.hpp"

// Crow includes windows.h: without its min and max macros, which break std::min
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#include <crow.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>

namespace rtype::server
{

namespace
{

using Clock = std::chrono::steady_clock;

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("web");
    return instance;
}

/// Scripts from the panel only; inline styles allowed, as Chakra UI inserts its styles at run time
/// (docs/architecture/web.md#the-headers).
constexpr std::string_view ContentSecurityPolicy =
    "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; "
    "font-src 'self'; connect-src 'self'; base-uri 'none'; form-action 'self'; frame-ancestors 'none'";

constexpr std::string_view SessionCookie = "rtype_session";
constexpr std::chrono::hours SessionLifetime{12};
/// Opening the address of admin-url.txt again makes a new session: the oldest go beyond this many.
constexpr std::size_t MaxSessions = 64;

/// A log of Crow without the query strings it holds: a token given in an address never reaches a log.
std::string withoutQueries(std::string_view message)
{
    std::string result;
    result.reserve(message.size());
    for (std::size_t i = 0; i < message.size(); ++i) {
        result.push_back(message[i]);
        if (message[i] == '?') {
            result.append("...");
            while (i + 1 < message.size() && message[i + 1] != ' ' && message[i + 1] != '\n') {
                ++i;
            }
        }
    }
    return result;
}

/// Crow logs through ours: its Info, one line per request and response, is our Debug.
class CrowLogs final : public crow::ILogHandler
{
public:
    void log(const std::string &message, crow::LogLevel level) override
    {
        rtype::log::Level ours = rtype::log::Level::Error;
        switch (level) {
        case crow::LogLevel::Debug:
            ours = rtype::log::Level::Trace;
            break;
        case crow::LogLevel::Info:
            ours = rtype::log::Level::Debug;
            break;
        case crow::LogLevel::Warning:
            ours = rtype::log::Level::Warning;
            break;
        case crow::LogLevel::Error:
        case crow::LogLevel::Critical:
            ours = rtype::log::Level::Error;
            break;
        }
        if (logger().isEnabled(ours)) {
            logger().log(ours, "{}", withoutQueries(message));
        }
    }
};

void installCrowLogs()
{
    static std::once_flag once;
    std::call_once(once, [] {
        static CrowLogs handler;
        crow::logger::setHandler(&handler);
        // Crow writes everything, our level chooses: its level is a global that its threads read
        crow::logger::setLogLevel(crow::LogLevel::Debug);
    });
}

/// Every response gets the headers of security and a line in the logs, with the address of the visitor.
struct Guard {
    struct context {
        rtype::network::Address visitor;
    };

    std::vector<Subnet> trustedProxies;

    void before_handle(crow::request &request, crow::response & /*response*/, context &context)
    {
        const rtype::network::Address peer =
            rtype::network::Address::parse(request.remote_ip_address, 0).value_or(rtype::network::Address{});
        // A visitor can send its own X-Forwarded-For beside the one of the proxy: with two, the order between
        // them is lost, so none is believed
        const bool one = request.headers.count("X-Forwarded-For") == 1;
        context.visitor =
            visitorAddress(peer, one ? request.get_header_value("X-Forwarded-For") : "", trustedProxies);
    }

    void after_handle(crow::request &request, crow::response &response, context &context)
    {
        response.set_header("Content-Security-Policy", std::string(ContentSecurityPolicy));
        response.set_header("X-Content-Type-Options", "nosniff");
        response.set_header("Referrer-Policy", "no-referrer");
        if (request.url.starts_with("/api/")) {
            response.set_header("Cache-Control", "no-store");
        }
        // The path only: the query string can hold the token
        logger().debug("{} {} {} from {}", crow::method_name(request.method), request.url, response.code,
                       context.visitor.ip());
    }
};

using App = crow::App<Guard>;

/// A response computed away from the threads of Crow.
struct Answer {
    int status = 200;
    std::string contentType;
    std::string body;
};

/// Answers a request posted to the loop. Can be called from any thread, once.
using Reply = std::function<void(Answer)>;

Answer json(int status, std::string body)
{
    return {status, "application/json", std::move(body)};
}

Answer error(int status, std::string_view message)
{
    return json(status, std::format(R"({{"error":"{}"}})", message));
}

void apply(crow::response &response, const Answer &answer)
{
    response.code = answer.status;
    response.set_header("Content-Type", answer.contentType);
    response.body = answer.body;
}

/// A page of the server itself, in the colours of the game. `text` is ours, never the visitor's.
Answer page(int status, std::string_view title, std::string_view text)
{
    return {
        status, "text/html; charset=utf-8",
        std::format("<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
                    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                    "<title>{0} - R-Type server</title>\n<style>\n"
                    "body {{ background: #0c0c1c; color: #fff; font-family: sans-serif; max-width: 40rem; "
                    "margin: 4rem auto; padding: 0 1rem; line-height: 1.5; }}\n"
                    "h1 {{ color: #ffd400; }}\n</style>\n</head>\n<body>\n<h1>{0}</h1>\n<p>{1}</p>\n"
                    "</body>\n</html>\n",
                    title, text)};
}

Answer notBuilt()
{
    return page(200, "The panel is not built",
                "This server runs without the files of its administration panel. Build it from web/admin, "
                "then put it in admin/ next to r-type_server, or give its folder as panel_dir in the [web] "
                "section of the settings.");
}

/// The value of the cookie `name` in a Cookie header, `a=1; rtype_session=…`.
std::optional<std::string_view> cookie(std::string_view header, std::string_view name)
{
    while (!header.empty()) {
        const std::size_t end = header.find(';');
        std::string_view pair = header.substr(0, end);
        header = end == std::string_view::npos ? std::string_view{} : header.substr(end + 1);
        while (pair.starts_with(' ')) {
            pair.remove_prefix(1);
        }
        if (pair.starts_with(name) && pair.size() > name.size() && pair[name.size()] == '=') {
            return pair.substr(name.size() + 1);
        }
    }
    return std::nullopt;
}

/// The path of an address, its `%xx` decoded; nothing if one is invalid or decodes to a NUL.
std::optional<std::string> decodePath(std::string_view path)
{
    const auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };
    std::string result;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] != '%') {
            result.push_back(path[i]);
            continue;
        }
        if (i + 2 >= path.size() || digit(path[i + 1]) < 0 || digit(path[i + 2]) < 0) {
            return std::nullopt;
        }
        const char decoded = static_cast<char>((digit(path[i + 1]) << 4) | digit(path[i + 2]));
        if (decoded == '\0') {
            return std::nullopt;
        }
        result.push_back(decoded);
        i += 2;
    }
    return result;
}

std::string_view contentType(const std::filesystem::path &file)
{
    static const std::unordered_map<std::string, std::string_view> Types = {
        {".html", "text/html; charset=utf-8"},
        {".js", "text/javascript; charset=utf-8"},
        {".mjs", "text/javascript; charset=utf-8"},
        {".css", "text/css; charset=utf-8"},
        {".json", "application/json"},
        {".map", "application/json"},
        {".txt", "text/plain; charset=utf-8"},
        {".svg", "image/svg+xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".webp", "image/webp"},
        {".ico", "image/x-icon"},
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf", "font/ttf"},
        {".otf", "font/otf"},
        {".wasm", "application/wasm"},
    };
    std::string extension = file.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return std::tolower(c); });
    const auto found = Types.find(extension);
    return found == Types.end() ? "application/octet-stream" : found->second;
}

std::optional<std::string> readFile(const std::filesystem::path &file)
{
    std::ifstream stream(file, std::ios::binary);
    std::string content{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (!stream && !stream.eof()) {
        return std::nullopt;
    }
    return content;
}

/// The requests posted to the loop and not answered yet, shared with the tasks on the loop, which can
/// outlive the server.
struct Pending {
    struct Request {
        crow::response *response = nullptr;
        /// The thread of Crow of its connection, the only one that may end its response: ended anywhere else,
        /// the response can free its connection while the connection still runs.
        asio::io_context *context = nullptr;
    };

    std::mutex mutex;
    std::condition_variable answered;
    /// Set by stop(): a reply from then on is dropped, and stop() answers what is left.
    bool stopped = false;
    std::uint64_t next = 0;
    /// By id, not by response: a connection reuses its response for its next request.
    std::unordered_map<std::uint64_t, Request> requests;

    /// Answers request `id` if it still waits. Called on its thread of Crow.
    void finish(std::uint64_t id, const Answer &answer)
    {
        crow::response *response = nullptr;
        {
            const std::lock_guard lock(mutex);
            const auto found = requests.find(id);
            if (found == requests.end()) {
                return;
            }
            response = found->second.response;
            requests.erase(found);
        }
        apply(*response, answer);
        response->end();
        answered.notify_all();
    }
};

struct Session {
    Secret id{};
    Clock::time_point expires;
};

} // namespace

struct WebServer::Impl {
    rtype::network::EventLoop &loop;
    WebServerSettings settings;
    std::shared_ptr<Pending> pending = std::make_shared<Pending>();
    std::mutex sessionsMutex;
    std::vector<Session> sessions;
    App app;
    std::future<void> running;
    rtype::network::Address local;
    bool stopped = false;

    Impl(rtype::network::EventLoop &eventLoop, WebServerSettings webSettings)
        : loop(eventLoop), settings(std::move(webSettings))
    {
        installCrowLogs();
        // Opened once here for its error: Crow only logs why it could not listen. On a loop of its own, run
        // until the port is closed again: the loop of the master does not run yet, and a listener closes on
        // its loop
        {
            rtype::network::EventLoop probing;
            {
                const rtype::network::TcpListener probe(probing, settings.listen);
            }
            probing.poll();
        }
        app.get_middleware<Guard>().trustedProxies = settings.trustedProxies;
        // The master handles Ctrl+C and SIGTERM itself, and stops this server
        app.signal_clear();
        app.bindaddr(settings.listen.ip())
            .port(settings.listen.port())
            .concurrency(2)
            .server_name("r-type_server");
        routes();
        running = app.run_async();
        app.wait_for_server_start(std::chrono::seconds(10));
        if (!app.is_bound()) {
            stop();
            throw std::system_error(std::make_error_code(std::errc::address_in_use),
                                    std::format("cannot listen on {}", settings.listen));
        }
        local = settings.listen.withPort(app.port());
    }

    ~Impl() { stop(); }

    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;
    Impl(Impl &&) = delete;
    Impl &operator=(Impl &&) = delete;

    void stop()
    {
        if (stopped) {
            return;
        }
        stopped = true;
        std::vector<std::pair<std::uint64_t, asio::io_context *>> left;
        {
            const std::lock_guard lock(pending->mutex);
            pending->stopped = true;
            for (const auto &[id, waiting] : pending->requests) {
                left.emplace_back(id, waiting.context);
            }
        }
        // The requests still waiting for the loop are answered by Crow's threads while they run: a connection
        // with a response never ended would never be freed
        for (const auto &[id, context] : left) {
            asio::post(*context,
                       [shared = pending, id] { shared->finish(id, error(503, "the server is stopping")); });
        }
        if (!left.empty()) {
            std::unique_lock lock(pending->mutex);
            pending->answered.wait_for(lock, std::chrono::seconds(2),
                                       [&] { return pending->requests.empty(); });
            lock.unlock();
            // The time for the responses to leave, before the connections close
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        app.stop();
        if (running.valid()) {
            running.wait();
        }
    }

    void routes()
    {
        CROW_ROUTE(app, "/api/health")
        ([this](const crow::request &request, crow::response &response) {
            // Answered by the loop: a master whose loop is stuck is not healthy
            onLoop(request, response, [](const Reply &reply) { reply(json(200, R"({"status":"ok"})")); });
        });
        CROW_ROUTE(app, "/api/session")
        ([this](const crow::request &request, crow::response &response) {
            if (signedIn(request)) {
                response.code = 204;
            } else {
                apply(response, error(401, "no session: open the address in admin-url.txt"));
            }
            response.end();
        });
        CROW_CATCHALL_ROUTE(app)
        ([this](const crow::request &request, crow::response &response) {
            apply(response, serve(request, response));
            response.end();
        });
    }

    /// Runs `work` on the loop, which replies; the reply is sent from the thread of Crow of the request.
    void onLoop(const crow::request &request, crow::response &response,
                std::function<void(const Reply &)> work)
    {
        std::uint64_t id = 0;
        bool refused = false;
        {
            const std::lock_guard lock(pending->mutex);
            refused = pending->stopped;
            if (!refused) {
                id = ++pending->next;
                pending->requests.emplace(id, Pending::Request{&response, request.io_context});
            }
        }
        if (refused) {
            apply(response, error(503, "the server is stopping"));
            response.end();
            return;
        }
        asio::io_context *context = request.io_context;
        loop.post([shared = pending, context, id, work = std::move(work)] {
            work([shared, context, id](Answer answer) {
                const std::lock_guard lock(shared->mutex);
                if (shared->stopped) {
                    return;
                }
                asio::post(*context,
                           [shared, id, answer = std::move(answer)] { shared->finish(id, answer); });
            });
        });
    }

    [[nodiscard]] bool signedIn(const crow::request &request)
    {
        const auto value = cookie(request.get_header_value("Cookie"), SessionCookie);
        const auto id = value ? parseSecret(*value) : std::nullopt;
        if (!id) {
            return false;
        }
        const Clock::time_point now = Clock::now();
        const std::lock_guard lock(sessionsMutex);
        std::erase_if(sessions, [&](const Session &session) { return session.expires <= now; });
        // Each one compared in constant time
        bool found = false;
        for (const Session &session : sessions) {
            found = sameSecret(session.id, *id) || found;
        }
        return found;
    }

    Answer serve(const crow::request &request, crow::response &response)
    {
        const std::string &path = request.url;
        if (path == "/api" || path.starts_with("/api/")) {
            return signedIn(request) ? error(404, "no such route")
                                     : error(401, "no session: open the address in admin-url.txt");
        }
        if (request.method != crow::HTTPMethod::Get) {
            response.set_header("Allow", "GET");
            return page(405, "Method not allowed", "The panel only answers GET.");
        }
        if (path == "/" && request.url_params.get("token") != nullptr) {
            return exchangeToken(request, response);
        }
        return servePanel(path, response);
    }

    /// `GET /?token=…`: the local access token traded for a session cookie, then the address without it.
    Answer exchangeToken(const crow::request &request, crow::response &response)
    {
        const std::string visitor = app.get_context<Guard>(request).visitor.ip();
        const auto token = parseSecret(request.url_params.get("token"));
        if (!token || !sameSecret(*token, settings.accessToken)) {
            logger().warning("A wrong access token was given to the panel, from {}", visitor);
            return page(403, "This link is not valid",
                        "The server writes a new link to admin-url.txt, beside its key, at every start: open "
                        "that one.");
        }
        const Secret id = newAccessToken();
        {
            const std::lock_guard lock(sessionsMutex);
            if (sessions.size() >= MaxSessions) {
                sessions.erase(sessions.begin());
            }
            sessions.push_back({id, Clock::now() + SessionLifetime});
        }
        logger().info("The panel was opened with the local access token, from {}", visitor);
        response.set_header(
            "Set-Cookie",
            std::format("{}={}; Path=/; Max-Age={}; HttpOnly; SameSite=Strict", SessionCookie,
                        formatSecret(id),
                        std::chrono::duration_cast<std::chrono::seconds>(SessionLifetime).count()));
        response.set_header("Location", "/");
        response.set_header("Cache-Control", "no-store");
        return {303, "text/plain; charset=utf-8", "Signed in\n"};
    }

    /// A file of the panel, or its index.html for the paths of its pages.
    Answer servePanel(const std::string &path, crow::response &response)
    {
        const auto decoded = decodePath(path);
        if (!decoded || decoded->find('\\') != std::string::npos) {
            return page(404, "Not found", "There is nothing at this address.");
        }
        std::filesystem::path relative;
        std::string_view rest = *decoded;
        while (!rest.empty()) {
            const std::size_t slash = rest.find('/');
            const std::string_view segment = rest.substr(0, slash);
            rest = slash == std::string_view::npos ? std::string_view{} : rest.substr(slash + 1);
            if (segment == "..") {
                return page(404, "Not found", "There is nothing at this address.");
            }
            if (!segment.empty() && segment != ".") {
                relative /= std::filesystem::path(std::u8string(segment.begin(), segment.end()));
            }
        }
        std::error_code ignored;
        const std::filesystem::path file = settings.panelDir / relative;
        if (!relative.empty() && std::filesystem::is_regular_file(file, ignored)) {
            if (const auto content = readFile(file)) {
                return {200, std::string(contentType(file)), *content};
            }
            return page(500, "Cannot read the file", "The server cannot read this file of the panel.");
        }
        // A file that is not there: the pages of the panel have no extension
        if (relative.has_extension()) {
            return page(404, "Not found", "There is nothing at this address.");
        }
        const std::filesystem::path index = settings.panelDir / "index.html";
        if (!std::filesystem::is_regular_file(index, ignored)) {
            return notBuilt();
        }
        if (const auto content = readFile(index)) {
            // Always asked again: it names the files of the build, which change with each one
            response.set_header("Cache-Control", "no-cache");
            return {200, "text/html; charset=utf-8", *content};
        }
        return page(500, "Cannot read the file", "The server cannot read the index.html of the panel.");
    }
};

WebServer::WebServer(rtype::network::EventLoop &loop, WebServerSettings settings)
    : _impl(std::make_unique<Impl>(loop, std::move(settings)))
{
}

WebServer::~WebServer() = default;

rtype::network::Address WebServer::localAddress() const
{
    return _impl->local;
}

void WebServer::stop()
{
    _impl->stop();
}

} // namespace rtype::server
