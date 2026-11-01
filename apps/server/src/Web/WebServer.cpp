#include "Web/WebServer.hpp"

#include "Accounts/PanelAccounts.hpp"
#include "Web/AdminJson.hpp"
#include "Web/LocalAccess.hpp"
#include "Web/Visitor.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Text.hpp"

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
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <iterator>
#include <limits>
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
/// The state of a sign-in with Discord, kept by the browser until Discord sends it back: `SameSite=Lax`, as
/// the return from Discord is a navigation from another site, which a `Strict` cookie would not follow.
constexpr std::string_view DiscordCookie = "rtype_discord";
constexpr std::string_view DiscordPath = "/auth/discord";
/// The header every POST of the API carries, with the token GET /api/session gives.
constexpr std::string_view CsrfHeader = "X-CSRF-Token";
/// The accounts of GET /api/accounts, by default and at most; the longest search.
constexpr std::size_t DefaultAccounts = 50;
constexpr std::size_t MaxAccounts = 200;
constexpr std::size_t MaxSearchLength = 254;
/// The longest login and password of a sign-in to the panel, in bytes.
constexpr std::size_t MaxLoginLength = 254;
constexpr std::size_t MaxPasswordBytes = 512;
/// The actions of GET /api/actions, by default and at most.
constexpr std::size_t DefaultActions = 100;
constexpr std::size_t MaxActions = 500;
constexpr std::chrono::hours SessionLifetime{PanelSessionLifetime};
/// How long a sign-in with Discord started waits for its return.
constexpr std::chrono::minutes DiscordLifetime{10};
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
    /// Beside the usual ones: Set-Cookie, Location. Added, so that a header can come twice.
    std::vector<std::pair<std::string, std::string>> headers = {};
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

Answer jsonOf(int status, const crow::json::wvalue &value)
{
    return json(status, value.dump());
}

void apply(crow::response &response, const Answer &answer)
{
    response.code = answer.status;
    response.set_header("Content-Type", answer.contentType);
    for (const auto &[name, value] : answer.headers) {
        response.add_header(name, value);
    }
    response.body = answer.body;
}

/// The answer that sends the browser to `location` once it set its cookies.
Answer redirect(std::string location, std::vector<std::pair<std::string, std::string>> headers = {})
{
    headers.emplace_back("Location", std::move(location));
    headers.emplace_back("Cache-Control", "no-store");
    return {303, "text/plain; charset=utf-8", "See the next address\n", std::move(headers)};
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

/// A session opened with the local access token, kept in memory.
struct Session {
    Secret id{};
    /// What every POST of the API carries in X-CSRF-Token: a page of another site cannot read it.
    Secret csrf{};
    Clock::time_point expires;
};

/// A session of an account found valid lately: what the upgrade of a WebSocket checks, which cannot wait for
/// the database. The loop checks it again once the WebSocket is open.
struct KnownSession {
    Secret token{};
    long long accountId = 0;
    Clock::time_point expires;
};

/// Who a request of the API comes from, once its session is checked.
struct SignedIn {
    /// `local`, or the id of the account: the actor of what it does.
    std::string actor;
    /// Nothing for the local access token.
    std::optional<PanelAccount> account;
    Secret csrf{};
    /// The value of its cookie. Secret.
    Secret session{};
};

/// What a WebSocket was accepted with, from its upgrade to its opening: the userdata of its connection.
struct Accepted {
    Secret session{};
    /// 0 for the local access token.
    long long accountId = 0;
};

/// The WebSockets of /api/events, shared with the subscriptions on the loop, which can outlive the server.
struct Subscribers {
    struct Subscriber {
        /// Never reused: a new connection can get the address of a closed one.
        std::uint64_t token = 0;
        /// The subscription of the administration, 0 until it is made on the loop.
        std::uint64_t subscription = 0;
        Accepted accepted;
    };

    std::mutex mutex;
    /// Cleared by stop(): Crow may then free its connections without closing them first.
    std::unordered_map<crow::websocket::connection *, Subscriber> connections;
    std::uint64_t lastToken = 0;

    /// Sends a text to a connection if it is still the one of `token`. A connection is in the map until its
    /// close handler, which takes the lock: it is alive while the lock is held.
    void send(crow::websocket::connection *connection, std::uint64_t token, std::string text)
    {
        const std::lock_guard lock(mutex);
        const auto found = connections.find(connection);
        if (found != connections.end() && found->second.token == token) {
            connection->send_text(std::move(text));
        }
    }

    /// Closes the WebSockets whose session no longer opens the panel. Crow closes them on its threads.
    void close(const std::function<bool(const Accepted &)> &which)
    {
        static constexpr std::uint16_t PolicyViolation = 1008;
        const std::lock_guard lock(mutex);
        for (const auto &[connection, subscriber] : connections) {
            if (which(subscriber.accepted)) {
                connection->close("signed out", PolicyViolation);
            }
        }
    }
};

/// The origin of an address, `https://admin.example.org` for `https://admin.example.org/panel`.
std::string originOf(std::string_view url)
{
    const std::size_t scheme = url.find("://");
    if (scheme == std::string_view::npos) {
        return std::string(url);
    }
    return std::string(url.substr(0, url.find('/', scheme + 3)));
}

/// A number of the query, or `fallback` without it; nothing if it is not one.
std::optional<std::uint64_t> queryNumber(const crow::request &request, const char *name,
                                         std::uint64_t fallback)
{
    const char *value = request.url_params.get(name);
    if (value == nullptr) {
        return fallback;
    }
    const std::string_view text(value);
    std::uint64_t number = 0;
    const auto [end, problem] = std::from_chars(text.data(), text.data() + text.size(), number);
    if (problem != std::errc{} || end != text.data() + text.size() || text.empty()) {
        return std::nullopt;
    }
    return number;
}

/// The body of a POST: a JSON object, or an empty object for an empty body when `optional`.
std::optional<crow::json::rvalue> body(const crow::request &request, bool optional)
{
    if (optional && request.body.empty()) {
        return crow::json::load("{}");
    }
    crow::json::rvalue read = crow::json::load(request.body);
    if (!read || read.t() != crow::json::type::Object) {
        return std::nullopt;
    }
    return read;
}

/// A string field of a body, "" without it; nothing if it is not text of at most `maxLength` bytes.
std::optional<std::string> textField(const crow::json::rvalue &object, const char *name,
                                     std::size_t maxLength)
{
    if (!object.has(name)) {
        return std::string{};
    }
    const crow::json::rvalue &field = object[name];
    if (field.t() != crow::json::type::String) {
        return std::nullopt;
    }
    std::string text = field.s();
    if (text.size() > maxLength || !rtype::network::isValidText(text)) {
        return std::nullopt;
    }
    return text;
}

/// An integer field of a body, nothing without it or if it is not one.
std::optional<std::int64_t> integerField(const crow::json::rvalue &object, const char *name)
{
    if (!object.has(name) || object[name].t() != crow::json::type::Number ||
        object[name].nt() == crow::json::num_type::Floating_point ||
        object[name].nt() == crow::json::num_type::Double_precision_floating_point) {
        return std::nullopt;
    }
    return object[name].i();
}

/// The optional reason of a body that may be empty.
std::optional<std::string> reasonOf(const crow::request &request)
{
    const auto read = body(request, true);
    return read ? textField(*read, "reason", MaxAdminReasonLength) : std::nullopt;
}

constexpr std::string_view ReasonOnly =
    "the body is a JSON object, with an optional reason of at most 200 bytes";

Answer noSession()
{
    return error(401, "no session: sign in, or open the address in admin-url.txt");
}

/// What the panel tells of a refused account, as JSON or as a page.
struct Refused {
    int status;
    std::string_view title;
    std::string_view text;
};

Refused refusedBecause(PanelRefusal refusal)
{
    switch (refusal) {
    case PanelRefusal::InvalidCredentials:
        return {401, "Not signed in", "wrong login or password"};
    case PanelRefusal::EmailNotVerified:
        return {403, "Not signed in",
                "the email of this account is not verified: verify it in the game first"};
    case PanelRefusal::NotAdmin:
        return {403, "Not an administrator", "this account is not an administrator of this server"};
    case PanelRefusal::Banned:
        return {403, "Banned", "this account is banned"};
    case PanelRefusal::TooManyAttempts:
        return {429, "Too many attempts", "too many attempts: try again in a few minutes"};
    case PanelRefusal::Busy:
        return {503, "Busy", "the server is too busy: try again in a moment"};
    case PanelRefusal::SessionExpired:
        return {401, "Not signed in", "no session: sign in, or open the address in admin-url.txt"};
    case PanelRefusal::DiscordExpired:
        return {403, "Sign-in expired",
                "this sign-in with Discord is used, expired, or was started in another browser: start again "
                "from the panel"};
    case PanelRefusal::DiscordRefused:
        return {403, "Not signed in",
                "Discord did not sign in an account of this server: sign in to the game with Discord first"};
    case PanelRefusal::DiscordUnavailable:
        return {503, "Discord cannot be reached", "Discord cannot be reached: try again in a moment"};
    }
    return {403, "Not signed in", "refused"};
}

crow::json::wvalue accountJson(const PanelAccount &account)
{
    crow::json::wvalue json;
    json["id"] = static_cast<std::int64_t>(account.id);
    json["username"] = account.username;
    json["role"] = std::string(roleName(account.role));
    json["locked"] = account.listed;
    json["discord"] = discordJson(account.discord);
    return json;
}

/// The answer of an action on an account.
Answer changed(const AccountChanged &changed)
{
    switch (changed.outcome) {
    case AccountChanged::Outcome::Done: {
        crow::json::wvalue done;
        done["account"] = changed.account ? toJson(*changed.account) : crow::json::wvalue(nullptr);
        done["removed"] = static_cast<std::uint64_t>(changed.removed);
        return jsonOf(200, done);
    }
    case AccountChanged::Outcome::NoAccount:
        return error(404, "no such account");
    case AccountChanged::Outcome::Locked:
        return error(409,
                     "this administrator is in [admin] admins of the settings: the panel can neither demote, "
                     "ban nor delete it");
    case AccountChanged::Outcome::Failed:
        break;
    }
    return error(503, "the database cannot change the account now");
}

} // namespace

struct WebServer::Impl {
    rtype::network::EventLoop &loop;
    WebServerSettings settings;
    /// Behind HTTPS, the cookies are sent over HTTPS only.
    bool secure = false;
    std::shared_ptr<Pending> pending = std::make_shared<Pending>();
    std::mutex sessionsMutex;
    std::vector<Session> sessions;
    std::vector<KnownSession> known;
    std::shared_ptr<Subscribers> subscribers = std::make_shared<Subscribers>();
    /// The thread of the loop only.
    Administration *administration = nullptr;
    PanelAccounts *panelAccounts = nullptr;
    App app;
    std::future<void> running;
    rtype::network::Address local;
    bool stopped = false;

    Impl(rtype::network::EventLoop &eventLoop, WebServerSettings webSettings)
        : loop(eventLoop), settings(std::move(webSettings)),
          secure(settings.publicUrl.starts_with("https://"))
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
        {
            // Nothing is sent to a WebSocket from now on: Crow closes them, and may free them without telling
            const std::lock_guard lock(subscribers->mutex);
            subscribers->connections.clear();
        }
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

    // --- Cookies and sessions ---

    /// A Set-Cookie header: HttpOnly always, Secure behind HTTPS. A `maxAge` of 0 removes the cookie.
    [[nodiscard]] std::pair<std::string, std::string> setCookie(std::string_view name, std::string_view value,
                                                                std::chrono::seconds maxAge,
                                                                std::string_view sameSite,
                                                                std::string_view path = "/") const
    {
        return {"Set-Cookie", std::format("{}={}; Path={}; Max-Age={}; HttpOnly; SameSite={}{}", name, value,
                                          path, maxAge.count(), sameSite, secure ? "; Secure" : "")};
    }

    [[nodiscard]] std::pair<std::string, std::string> sessionCookie(const Secret &session) const
    {
        return setCookie(SessionCookie, formatSecret(session),
                         std::chrono::duration_cast<std::chrono::seconds>(SessionLifetime), "Strict");
    }

    [[nodiscard]] static rtype::network::Address visitorOf(App &app, const crow::request &request)
    {
        return app.get_context<Guard>(request).visitor;
    }

    /// The session of the local access token whose cookie this is, if it is one.
    [[nodiscard]] std::optional<Session> localSession(const Secret &id)
    {
        const Clock::time_point now = Clock::now();
        const std::lock_guard lock(sessionsMutex);
        std::erase_if(sessions, [&](const Session &session) { return session.expires <= now; });
        // Each one compared in constant time
        std::optional<Session> found;
        for (const Session &session : sessions) {
            if (sameSecret(session.id, id)) {
                found = session;
            }
        }
        return found;
    }

    /// The account of a session found valid lately, if any.
    [[nodiscard]] std::optional<long long> knownAccount(const Secret &token)
    {
        const Clock::time_point now = Clock::now();
        const std::lock_guard lock(sessionsMutex);
        std::erase_if(known, [&](const KnownSession &session) { return session.expires <= now; });
        std::optional<long long> found;
        for (const KnownSession &session : known) {
            if (sameSecret(session.token, token)) {
                found = session.accountId;
            }
        }
        return found;
    }

    void remember(const Secret &token, long long accountId)
    {
        const Clock::time_point expires = Clock::now() + SessionLifetime;
        const std::lock_guard lock(sessionsMutex);
        for (KnownSession &session : known) {
            if (sameSecret(session.token, token)) {
                session.expires = expires;
                return;
            }
        }
        if (known.size() >= MaxSessions * 4) {
            known.erase(known.begin());
        }
        known.push_back({token, accountId, expires});
    }

    /// Forgets a session of an account, and closes its WebSockets.
    void forgetSession(const Secret &token)
    {
        {
            const std::lock_guard lock(sessionsMutex);
            std::erase_if(known,
                          [&](const KnownSession &session) { return sameSecret(session.token, token); });
        }
        subscribers->close([&](const Accepted &accepted) { return sameSecret(accepted.session, token); });
    }

    /// Forgets every session of an account no longer administrator, and closes its WebSockets.
    void forgetAccount(long long accountId)
    {
        {
            const std::lock_guard lock(sessionsMutex);
            std::erase_if(known, [&](const KnownSession &session) { return session.accountId == accountId; });
        }
        subscribers->close([&](const Accepted &accepted) { return accepted.accountId == accountId; });
    }

    // --- The routes ---

    void routes()
    {
        CROW_ROUTE(app, "/api/health")
        ([this](const crow::request &request, crow::response &response) {
            // Answered by the loop: a master whose loop is stuck is not healthy
            onLoop(request, response, [](const Reply &reply) { reply(json(200, R"({"status":"ok"})")); });
        });
        CROW_ROUTE(app, "/api/session")
        ([this](const crow::request &request, crow::response &response) {
            withSession(request, response, false, [](const SignedIn &who, const Reply &reply) {
                crow::json::wvalue body;
                body["csrfToken"] = formatSecret(who.csrf);
                body["account"] = who.account ? accountJson(*who.account) : crow::json::wvalue(nullptr);
                reply(jsonOf(200, body));
            });
        });
        signInRoutes();
        discordRoutes();
        administrationRoutes();
        accountRoutes();
        eventsRoute();
        // Crow ends the response of the catch-all route itself, after the handler. Ended here too, it would
        // be left completed once sent, and the next request on the connection would get Crow's bare 404
        CROW_CATCHALL_ROUTE(app)
        ([this](const crow::request &request, crow::response &response) {
            apply(response, serve(request, response));
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

    using SignedWork = std::function<void(const SignedIn &who, const Reply &reply)>;

    /// A route of the API needs a session, and a change its CSRF token, before anything else is read: answers
    /// 401 or 403 otherwise. The session of the local access token is in memory; the session of an account is
    /// resumed from the database, on the loop, so that a ban or a new role counts at once.
    void withSession(const crow::request &request, crow::response &response, bool change, SignedWork work)
    {
        const auto value = cookie(request.get_header_value("Cookie"), SessionCookie);
        const std::optional<Secret> id = value ? parseSecret(*value) : std::nullopt;
        if (!id) {
            apply(response, noSession());
            response.end();
            return;
        }
        const std::optional<Session> local = localSession(*id);
        const Secret csrf = local ? local->csrf : panelCsrfToken(*id);
        if (change) {
            const auto token = parseSecret(request.get_header_value(std::string(CsrfHeader)));
            if (!token || !sameSecret(*token, csrf)) {
                apply(response,
                      error(403, "no CSRF token, or not the one of this session: see GET /api/session"));
                response.end();
                return;
            }
        }
        if (local) {
            onLoop(request, response, [work = std::move(work), csrf, id = *id](const Reply &reply) {
                work(SignedIn{.actor = std::string(LocalActor),
                              .account = std::nullopt,
                              .csrf = csrf,
                              .session = id},
                     reply);
            });
            return;
        }
        onLoop(request, response, [this, work = std::move(work), csrf, id = *id](const Reply &reply) {
            if (panelAccounts == nullptr) {
                reply(noSession());
                return;
            }
            panelAccounts->resume(id, [this, work, csrf, id, reply](const PanelResume &resumed) {
                if (const auto *account = std::get_if<PanelAccount>(&resumed)) {
                    remember(id, account->id);
                    work(SignedIn{.actor = std::to_string(account->id),
                                  .account = *account,
                                  .csrf = csrf,
                                  .session = id},
                         reply);
                    return;
                }
                if (std::get<PanelRefusal>(resumed) == PanelRefusal::Busy) {
                    reply(error(503, "the database cannot check the session now"));
                    return;
                }
                forgetSession(id);
                reply(noSession());
            });
        });
    }

    // --- Signing in ---

    void signInRoutes()
    {
        CROW_ROUTE(app, "/api/sign-in")
            .methods(crow::HTTPMethod::Get,
                     crow::HTTPMethod::Post)([this](const crow::request &request, crow::response &response) {
                if (request.method == crow::HTTPMethod::Get) {
                    // What the page of the panel offers before any session
                    onLoop(request, response, [this](const Reply &reply) {
                        crow::json::wvalue body;
                        body["password"] = panelAccounts != nullptr;
                        body["discord"] = panelAccounts != nullptr && panelAccounts->discordEnabled();
                        reply(jsonOf(200, body));
                    });
                    return;
                }
                signIn(request, response);
            });
        CROW_ROUTE(app, "/api/sign-out")
            .methods(crow::HTTPMethod::Post)([this](const crow::request &request, crow::response &response) {
                withSession(request, response, true, [this](const SignedIn &who, const Reply &reply) {
                    const auto removed = setCookie(SessionCookie, "", std::chrono::seconds(0), "Strict");
                    const Answer done{200, "application/json", "{}", {removed}};
                    if (!who.account) {
                        {
                            const std::lock_guard lock(sessionsMutex);
                            std::erase_if(sessions, [&](const Session &session) {
                                return sameSecret(session.id, who.session);
                            });
                        }
                        subscribers->close([&](const Accepted &accepted) {
                            return sameSecret(accepted.session, who.session);
                        });
                        reply(done);
                        return;
                    }
                    forgetSession(who.session);
                    logger().info("{} ({}) signed out of the panel", who.account->username, who.account->id);
                    panelAccounts->signOut(who.session, [reply, done](bool) { reply(done); });
                });
            });
    }

    /// `POST /api/sign-in`: a login and a password traded for a session of an account.
    void signIn(const crow::request &request, crow::response &response)
    {
        // A form of another site cannot send JSON without asking first, which this server never allows
        if (!request.get_header_value("Content-Type").starts_with("application/json")) {
            apply(response, error(415, "the body is JSON, with Content-Type: application/json"));
            response.end();
            return;
        }
        const auto read = body(request, false);
        auto login = read ? textField(*read, "login", MaxLoginLength) : std::nullopt;
        auto password = read ? textField(*read, "password", MaxPasswordBytes) : std::nullopt;
        if (!login || !password || login->empty() || password->empty()) {
            apply(response, error(400, "the body is a JSON object with a login and a password"));
            response.end();
            return;
        }
        const rtype::network::Address visitor = visitorOf(app, request);
        onLoop(request, response,
               [this, login = std::move(*login), password = std::move(*password),
                visitor](const Reply &reply) mutable {
                   if (panelAccounts == nullptr) {
                       reply(error(404, "this server has no accounts: open the address in admin-url.txt"));
                       return;
                   }
                   panelAccounts->signIn(login, std::move(password), visitor,
                                         [this, reply](const PanelSignIn &result) {
                                             if (const auto *refusal = std::get_if<PanelRefusal>(&result)) {
                                                 const Refused refused = refusedBecause(*refusal);
                                                 reply(error(refused.status, refused.text));
                                                 return;
                                             }
                                             const PanelSignedIn &signedIn = std::get<PanelSignedIn>(result);
                                             remember(signedIn.token, signedIn.account.id);
                                             crow::json::wvalue body;
                                             body["csrfToken"] = formatSecret(panelCsrfToken(signedIn.token));
                                             body["account"] = accountJson(signedIn.account);
                                             Answer answer = jsonOf(200, body);
                                             answer.headers.push_back(sessionCookie(signedIn.token));
                                             reply(std::move(answer));
                                         });
               });
    }

    /// `GET /auth/discord` sends the browser to Discord; `GET /auth/discord/callback` is where Discord sends
    /// it back, with the state the browser kept in its cookie.
    void discordRoutes()
    {
        CROW_ROUTE(app, "/auth/discord")
        ([this](const crow::request &request, crow::response &response) {
            const rtype::network::Address visitor = visitorOf(app, request);
            onLoop(request, response, [this, visitor](const Reply &reply) {
                if (panelAccounts == nullptr || !panelAccounts->discordEnabled()) {
                    reply(page(404, "Discord is not enabled",
                               "This server does not offer the sign-in with Discord to its panel."));
                    return;
                }
                const std::optional<PanelDiscordStart> started = panelAccounts->startDiscord(visitor);
                if (!started) {
                    reply(page(429, "Too many attempts",
                               "Too many sign-ins from your address: try again in a "
                               "few minutes."));
                    return;
                }
                reply(redirect(started->url,
                               {setCookie(DiscordCookie, started->state,
                                          std::chrono::duration_cast<std::chrono::seconds>(DiscordLifetime),
                                          "Lax", DiscordPath)}));
            });
        });
        CROW_ROUTE(app, "/auth/discord/callback")
        ([this](const crow::request &request, crow::response &response) {
            // Single use: the cookie goes whatever the answer
            const auto removed = setCookie(DiscordCookie, "", std::chrono::seconds(0), "Lax", DiscordPath);
            const std::string kept(cookie(request.get_header_value("Cookie"), DiscordCookie).value_or(""));
            const char *state = request.url_params.get("state");
            const char *code = request.url_params.get("code");
            const auto refuse = [&](Answer answer) {
                answer.headers.push_back(removed);
                apply(response, answer);
                response.end();
            };
            if (request.url_params.get("error") != nullptr) {
                refuse(page(403, "Not signed in",
                            "Discord did not sign you in. Go back to the panel to try again."));
                return;
            }
            if (state == nullptr || code == nullptr || std::string_view(code).empty() ||
                std::string_view(code).size() > MaxPasswordBytes) {
                refuse(page(400, "Not signed in",
                            "Discord sent back an address without its code or its state."));
                return;
            }
            onLoop(request, response,
                   [this, removed, state = std::string(state), kept,
                    code = std::string(code)](const Reply &reply) mutable {
                       if (panelAccounts == nullptr) {
                           Answer answer =
                               page(404, "Discord is not enabled",
                                    "This server does not offer the sign-in with Discord to its panel.");
                           answer.headers.push_back(removed);
                           reply(std::move(answer));
                           return;
                       }
                       panelAccounts->finishDiscord(
                           state, kept, std::move(code), [this, removed, reply](const PanelSignIn &result) {
                               if (const auto *refusal = std::get_if<PanelRefusal>(&result)) {
                                   const Refused refused = refusedBecause(*refusal);
                                   std::string text(refused.text);
                                   text[0] =
                                       static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
                                   Answer answer = page(refused.status, refused.title, text + ".");
                                   answer.headers.push_back(removed);
                                   reply(std::move(answer));
                                   return;
                               }
                               const PanelSignedIn &signedIn = std::get<PanelSignedIn>(result);
                               remember(signedIn.token, signedIn.account.id);
                               reply(redirect("/", {removed, sessionCookie(signedIn.token)}));
                           });
                   });
        });
    }

    // --- The administration ---

    /// The work of a route of the API, on the loop, with the administration and who asks.
    using AdminWork =
        std::function<void(Administration &administration, const SignedIn &who, const Reply &reply)>;

    /// Answers a route of the API: a session first, its CSRF token for a change, then the work on the loop.
    void api(const crow::request &request, crow::response &response, bool change, AdminWork work)
    {
        withSession(request, response, change,
                    [this, work = std::move(work)](const SignedIn &who, const Reply &reply) {
                        if (administration == nullptr) {
                            reply(error(503, "the administration is not ready"));
                            return;
                        }
                        work(*administration, who, reply);
                    });
    }

    void administrationRoutes()
    {
        CROW_ROUTE(app, "/api/server")
        ([this](const crow::request &request, crow::response &response) {
            api(request, response, false,
                [](Administration &administration, const SignedIn &, const Reply &reply) {
                    reply(jsonOf(200, toJson(administration.server())));
                });
        });
        CROW_ROUTE(app, "/api/games")
        ([this](const crow::request &request, crow::response &response) {
            api(request, response, false,
                [](Administration &administration, const SignedIn &, const Reply &reply) {
                    reply(jsonOf(200, toJsonList(administration.games())));
                });
        });
        CROW_ROUTE(app, "/api/games/<uint>")
        ([this](const crow::request &request, crow::response &response, std::uint64_t id) {
            api(request, response, false,
                [id](Administration &administration, const SignedIn &, const Reply &reply) {
                    const auto game = id <= std::numeric_limits<std::uint32_t>::max()
                                          ? administration.game(static_cast<std::uint32_t>(id))
                                          : std::nullopt;
                    reply(game ? jsonOf(200, toJson(*game)) : error(404, "no such game"));
                });
        });
        CROW_ROUTE(app, "/api/players")
        ([this](const crow::request &request, crow::response &response) {
            api(request, response, false,
                [](Administration &administration, const SignedIn &, const Reply &reply) {
                    reply(jsonOf(200, toJsonList(administration.players())));
                });
        });
        CROW_ROUTE(app, "/api/actions")
        ([this](const crow::request &request, crow::response &response) {
            const auto limit = queryNumber(request, "limit", DefaultActions);
            const auto before = queryNumber(request, "before", 0);
            const bool valid = limit && *limit != 0 && *limit <= MaxActions && before &&
                               *before <= static_cast<std::uint64_t>(std::numeric_limits<long long>::max());
            api(request, response, false,
                [valid, limit, before](Administration &administration, const SignedIn &, const Reply &reply) {
                    if (!valid) {
                        reply(error(400, "limit is 1 to 500, before an id of an action"));
                        return;
                    }
                    const std::optional<long long> page =
                        *before == 0 ? std::nullopt : std::optional(static_cast<long long>(*before));
                    administration.actions(
                        *limit, page, [reply](std::optional<std::vector<AdminAction>> actions) {
                            reply(actions ? jsonOf(200, toJsonList(*actions))
                                          : error(503, "the database cannot read the actions now"));
                        });
                });
        });
        CROW_ROUTE(app, "/api/bans")
            .methods(crow::HTTPMethod::Get,
                     crow::HTTPMethod::Post)([this](const crow::request &request, crow::response &response) {
                if (request.method == crow::HTTPMethod::Get) {
                    api(request, response, false,
                        [](Administration &administration, const SignedIn &, const Reply &reply) {
                            reply(jsonOf(200, toJsonList(administration.bans())));
                        });
                    return;
                }
                ban(request, response);
            });
        CROW_ROUTE(app, "/api/bans/lift")
            .methods(crow::HTTPMethod::Post)(
                [this](const crow::request &request, crow::response &response) { unban(request, response); });
        CROW_ROUTE(app, "/api/players/<uint>/kick")
            .methods(crow::HTTPMethod::Post)([this](const crow::request &request, crow::response &response,
                                                    std::uint64_t id) { kick(request, response, id); });
        CROW_ROUTE(app, "/api/games/<uint>/end")
            .methods(crow::HTTPMethod::Post)([this](const crow::request &request, crow::response &response,
                                                    std::uint64_t id) { endGame(request, response, id); });
        CROW_ROUTE(app, "/api/messages")
            .methods(crow::HTTPMethod::Post)([this](const crow::request &request, crow::response &response) {
                message(request, response);
            });
    }

    void kick(const crow::request &request, crow::response &response, std::uint64_t id)
    {
        const auto reason = reasonOf(request);
        const std::string visitor = visitorOf(app, request).ip();
        api(request, response, true,
            [id, reason, visitor](Administration &administration, const SignedIn &who, const Reply &reply) {
                if (!reason) {
                    reply(error(400, ReasonOnly));
                    return;
                }
                if (id > std::numeric_limits<std::uint32_t>::max() ||
                    !administration.kick(static_cast<std::uint32_t>(id), *reason, who.actor)) {
                    reply(error(404, "no such player"));
                    return;
                }
                logger().info("Player {} removed from the panel, from {}", id, visitor);
                crow::json::wvalue done;
                done["playerId"] = id;
                reply(jsonOf(200, done));
            });
    }

    void endGame(const crow::request &request, crow::response &response, std::uint64_t id)
    {
        const auto reason = reasonOf(request);
        api(request, response, true,
            [id, reason](Administration &administration, const SignedIn &who, const Reply &reply) {
                if (!reason) {
                    reply(error(400, ReasonOnly));
                    return;
                }
                if (id > std::numeric_limits<std::uint32_t>::max() ||
                    !administration.endGame(static_cast<std::uint32_t>(id), *reason, who.actor)) {
                    reply(error(404, "no such game"));
                    return;
                }
                crow::json::wvalue done;
                done["gameId"] = id;
                reply(jsonOf(200, done));
            });
    }

    void message(const crow::request &request, crow::response &response)
    {
        const auto read = body(request, false);
        const auto text =
            read ? textField(*read, "text", rtype::lobby::MaxServerMessageLength) : std::nullopt;
        std::optional<std::uint32_t> gameId;
        bool valid = text && rtype::lobby::isValidServerMessage(*text);
        if (valid && read->has("gameId") && read->operator[]("gameId").t() != crow::json::type::Null) {
            const auto id = integerField(*read, "gameId");
            valid = id && *id > 0 && *id <= std::numeric_limits<std::uint32_t>::max();
            if (valid) {
                gameId = static_cast<std::uint32_t>(*id);
            }
        }
        api(request, response, true,
            [valid, gameId, text](Administration &administration, const SignedIn &who, const Reply &reply) {
                if (!valid) {
                    reply(error(
                        400,
                        "the body is a JSON object with a text of 1 to 200 bytes, and an optional gameId"));
                    return;
                }
                const auto recipients = administration.sendMessage(gameId, *text, who.actor);
                if (!recipients) {
                    reply(error(404, "no such game"));
                    return;
                }
                crow::json::wvalue done;
                done["recipients"] = static_cast<std::uint64_t>(*recipients);
                reply(jsonOf(200, done));
            });
    }

    void ban(const crow::request &request, crow::response &response)
    {
        const auto read = body(request, false);
        const auto reason = read ? textField(*read, "reason", MaxAdminReasonLength) : std::nullopt;
        const auto addressText = read ? textField(*read, "address", 64) : std::nullopt;
        const auto address = addressText ? rtype::network::Address::parse(*addressText, 0) : std::nullopt;
        const auto duration = read ? integerField(*read, "duration") : std::nullopt;
        const auto longest = std::chrono::duration_cast<std::chrono::seconds>(MaxBanDuration).count();
        const bool valid = reason && address && duration && *duration >= 1 && *duration <= longest;
        api(request, response, true,
            [valid, address, duration, reason](Administration &administration, const SignedIn &who,
                                               const Reply &reply) {
                if (!valid) {
                    reply(error(400,
                                "the body is a JSON object with an IP address, a duration of 1 to 604800 "
                                "seconds, and an optional reason of at most 200 bytes"));
                    return;
                }
                const std::size_t removed =
                    administration.ban(*address, std::chrono::seconds(*duration), *reason, who.actor);
                const std::optional<AddressBan> ban = [&]() -> std::optional<AddressBan> {
                    for (const AddressBan &listed : administration.bans()) {
                        if (listed.address == banKey(*address)) {
                            return listed;
                        }
                    }
                    return std::nullopt;
                }();
                crow::json::wvalue done;
                done["ban"] = ban ? toJson(*ban) : crow::json::wvalue(nullptr);
                done["removed"] = static_cast<std::uint64_t>(removed);
                reply(jsonOf(200, done));
            });
    }

    void unban(const crow::request &request, crow::response &response)
    {
        const auto read = body(request, false);
        const auto reason = read ? textField(*read, "reason", MaxAdminReasonLength) : std::nullopt;
        const auto addressText = read ? textField(*read, "address", 64) : std::nullopt;
        const auto address = addressText ? rtype::network::Address::parse(*addressText, 0) : std::nullopt;
        api(request, response, true,
            [address, reason](Administration &administration, const SignedIn &who, const Reply &reply) {
                if (!reason || !address) {
                    reply(error(400,
                                "the body is a JSON object with an IP address, and an optional reason of at "
                                "most 200 bytes"));
                    return;
                }
                if (!administration.unban(*address, *reason, who.actor)) {
                    reply(error(404, "no ban of this address"));
                    return;
                }
                crow::json::wvalue done;
                done["address"] = banKey(*address).ip();
                reply(jsonOf(200, done));
            });
    }

    // --- The accounts ---

    void accountRoutes()
    {
        CROW_ROUTE(app, "/api/accounts")
        ([this](const crow::request &request, crow::response &response) {
            const char *searched = request.url_params.get("search");
            std::string search = searched != nullptr ? searched : "";
            const auto limit = queryNumber(request, "limit", DefaultAccounts);
            const auto before = queryNumber(request, "before", 0);
            const bool valid = search.size() <= MaxSearchLength && rtype::network::isValidText(search) &&
                               limit && *limit != 0 && *limit <= MaxAccounts && before &&
                               *before <= static_cast<std::uint64_t>(std::numeric_limits<long long>::max());
            api(request, response, false,
                [valid, search = std::move(search), limit, before](Administration &administration,
                                                                   const SignedIn &, const Reply &reply) {
                    if (!valid) {
                        reply(error(400,
                                    "search is at most 254 bytes of text, limit 1 to 200, before an id of an "
                                    "account"));
                        return;
                    }
                    const std::optional<long long> page =
                        *before == 0 ? std::nullopt : std::optional(static_cast<long long>(*before));
                    administration.accounts(
                        search, *limit, page, [reply](std::optional<std::vector<AdminAccount>> accounts) {
                            reply(accounts ? jsonOf(200, toJsonList(*accounts))
                                           : error(503, "the database cannot read the accounts now"));
                        });
                });
        });
        CROW_ROUTE(app, "/api/accounts/<uint>")
        ([this](const crow::request &request, crow::response &response, std::uint64_t id) {
            api(request, response, false,
                [id](Administration &administration, const SignedIn &, const Reply &reply) {
                    if (id > static_cast<std::uint64_t>(std::numeric_limits<long long>::max())) {
                        reply(error(404, "no such account"));
                        return;
                    }
                    administration.account(
                        static_cast<long long>(id),
                        [reply](std::optional<std::optional<AdminAccount>> account) {
                            if (!account) {
                                reply(error(503, "the database cannot read the account now"));
                            } else if (!*account) {
                                reply(error(404, "no such account"));
                            } else {
                                reply(jsonOf(200, toJson(**account)));
                            }
                        });
                });
        });
        CROW_ROUTE(app, "/api/accounts/<uint>/avatar")
        ([this](const crow::request &request, crow::response &response, std::uint64_t id) {
            api(request, response, false,
                [id](Administration &administration, const SignedIn &, const Reply &reply) {
                    if (id > static_cast<std::uint64_t>(std::numeric_limits<long long>::max())) {
                        reply(error(404, "no such picture"));
                        return;
                    }
                    administration.profile(
                        static_cast<long long>(id),
                        [reply](std::optional<std::optional<DiscordProfile>> profile) {
                            if (!profile) {
                                reply(error(503, "the database cannot read the picture now"));
                            } else if (!*profile || (*profile)->avatar.empty()) {
                                reply(error(404, "no such picture"));
                            } else {
                                // Named by its version: the browser keeps it until it changes
                                reply(Answer{200,
                                             "image/png",
                                             (*profile)->avatar,
                                             {{"Cache-Control", "private, max-age=86400"},
                                              {"ETag", "\"" + (*profile)->avatarHash + "\""}}});
                            }
                        });
                });
        });
        CROW_ROUTE(app, "/api/accounts/<uint>/ban")
            .methods(crow::HTTPMethod::Post)(
                [this](const crow::request &request, crow::response &response, std::uint64_t id) {
                    changeAccount(request, response, id, true,
                                  [reason = reasonOf(request)](Administration &administration,
                                                               long long account, const SignedIn &who,
                                                               Administration::ChangeCallback done) {
                                      if (!reason) {
                                          return false;
                                      }
                                      administration.banAccount(account, *reason, who.actor, std::move(done));
                                      return true;
                                  });
                });
        CROW_ROUTE(app, "/api/accounts/<uint>/unban")
            .methods(crow::HTTPMethod::Post)([this](const crow::request &request, crow::response &response,
                                                    std::uint64_t id) {
                changeAccount(request, response, id, false,
                              [reason = reasonOf(request)](Administration &administration, long long account,
                                                           const SignedIn &who,
                                                           Administration::ChangeCallback done) {
                                  if (!reason) {
                                      return false;
                                  }
                                  administration.unbanAccount(account, *reason, who.actor, std::move(done));
                                  return true;
                              });
            });
        CROW_ROUTE(app, "/api/accounts/<uint>/role")
            .methods(crow::HTTPMethod::Post)(
                [this](const crow::request &request, crow::response &response, std::uint64_t id) {
                    const auto read = body(request, false);
                    const auto name = read ? textField(*read, "role", 16) : std::nullopt;
                    const std::optional<AccountRole> role = name ? parseRole(*name) : std::nullopt;
                    changeAccount(request, response, id, role != AccountRole::Admin,
                                  [role](Administration &administration, long long account,
                                         const SignedIn &who, Administration::ChangeCallback done) {
                                      if (!role) {
                                          return false;
                                      }
                                      administration.setRole(account, *role, who.actor, std::move(done));
                                      return true;
                                  });
                });
        CROW_ROUTE(app, "/api/accounts/<uint>/delete")
            .methods(crow::HTTPMethod::Post)([this](const crow::request &request, crow::response &response,
                                                    std::uint64_t id) {
                changeAccount(request, response, id, true,
                              [reason = reasonOf(request)](Administration &administration, long long account,
                                                           const SignedIn &who,
                                                           Administration::ChangeCallback done) {
                                  if (!reason) {
                                      return false;
                                  }
                                  administration.deleteAccount(account, *reason, who.actor, std::move(done));
                                  return true;
                              });
            });
    }

    /// Starts an action on an account; false if its body is refused.
    using AccountWork = std::function<bool(Administration &administration, long long accountId,
                                           const SignedIn &who, Administration::ChangeCallback done)>;

    /// An action on an account: its answer once the database did it. When it takes the panel from the account
    /// (`revokes`: a ban, a deletion, a demotion), its sessions are forgotten and its WebSockets closed.
    void changeAccount(const crow::request &request, crow::response &response, std::uint64_t id, bool revokes,
                       AccountWork work)
    {
        api(request, response, true,
            [this, id, revokes, work = std::move(work)](Administration &administration, const SignedIn &who,
                                                        const Reply &reply) {
                if (id > static_cast<std::uint64_t>(std::numeric_limits<long long>::max())) {
                    reply(error(404, "no such account"));
                    return;
                }
                const auto accountId = static_cast<long long>(id);
                const bool started = work(administration, accountId, who,
                                          [this, accountId, revokes, reply](AccountChanged done) {
                                              if (revokes && done.outcome == AccountChanged::Outcome::Done) {
                                                  forgetAccount(accountId);
                                              }
                                              reply(changed(done));
                                          });
                if (!started) {
                    reply(error(400,
                                "the body is a JSON object, with an optional reason of at most 200 bytes, or "
                                "the role player or admin"));
                }
            });
    }

    // --- The live events ---

    /// What opens a WebSocket at its upgrade: the session of the local access token, or a session of an
    /// account found valid lately.
    [[nodiscard]] std::optional<Accepted> acceptable(const crow::request &request)
    {
        const auto value = cookie(request.get_header_value("Cookie"), SessionCookie);
        const std::optional<Secret> id = value ? parseSecret(*value) : std::nullopt;
        if (!id) {
            return std::nullopt;
        }
        if (localSession(*id)) {
            return Accepted{.session = *id, .accountId = 0};
        }
        if (const std::optional<long long> account = knownAccount(*id)) {
            return Accepted{.session = *id, .accountId = *account};
        }
        return std::nullopt;
    }

    /// The page that opened a WebSocket is the panel's: the origin the browser sends is the one of the
    /// address it asked, or the public address of the panel. A page of another site sends its own, which it
    /// cannot change.
    [[nodiscard]] bool fromPanel(const crow::request &request) const
    {
        const std::string origin = request.get_header_value("Origin");
        if (origin.empty()) {
            return false;
        }
        if (!settings.publicUrl.empty() && origin == originOf(settings.publicUrl)) {
            return true;
        }
        const std::string host = request.get_header_value("Host");
        return !host.empty() && (origin == "http://" + host || origin == "https://" + host);
    }

    /// `GET /api/events`: the WebSocket of the live events, a snapshot first.
    void eventsRoute()
    {
        CROW_WEBSOCKET_ROUTE(app, "/api/events")
            .onaccept([this](const crow::request &request, std::optional<crow::response> &response,
                             void **userdata) {
                // Answered before the upgrade, without the headers of the other responses: nothing is shown
                const std::optional<Accepted> accepted = acceptable(request);
                if (!accepted) {
                    response.emplace(
                        401, R"({"error":"no session: sign in, or open the address in admin-url.txt"})");
                } else if (!fromPanel(request)) {
                    logger().warning("A WebSocket was refused: it was opened by a page of {}",
                                     request.get_header_value("Origin").empty()
                                         ? std::string("no origin")
                                         : request.get_header_value("Origin"));
                    response.emplace(403, R"({"error":"the WebSocket is opened by the panel only"})");
                } else {
                    // Taken back by onopen, which Crow calls right after the upgrade
                    *userdata = new Accepted(*accepted);
                }
            })
            .onopen([this](crow::websocket::connection &connection) {
                const std::unique_ptr<Accepted> accepted(static_cast<Accepted *>(connection.userdata()));
                connection.userdata(nullptr);
                subscribe(connection, accepted ? *accepted : Accepted{});
            })
            .onclose([this](crow::websocket::connection &connection, const std::string &, std::uint16_t) {
                unsubscribe(connection);
            })
            .onmessage([](crow::websocket::connection &, const std::string &, bool) {
                // The events go one way: what the panel sends is ignored
            });
    }

    void subscribe(crow::websocket::connection &opened, const Accepted &accepted)
    {
        crow::websocket::connection *connection = &opened;
        std::uint64_t token = 0;
        {
            const std::lock_guard lock(subscribers->mutex);
            token = ++subscribers->lastToken;
            subscribers->connections[connection] = {token, 0, accepted};
        }
        loop.post([this, connection, token, accepted, pending = pending] {
            {
                const std::lock_guard lock(pending->mutex);
                if (pending->stopped) {
                    return;
                }
            }
            if (accepted.accountId == 0) {
                startEvents(connection, token);
                return;
            }
            if (panelAccounts == nullptr) {
                subscribers->close(
                    [&](const Accepted &open) { return sameSecret(open.session, accepted.session); });
                return;
            }
            // The upgrade trusted what it knew: the database tells whether the session still holds
            panelAccounts->resume(accepted.session,
                                  [this, connection, token, accepted, pending](const PanelResume &resumed) {
                                      {
                                          const std::lock_guard lock(pending->mutex);
                                          if (pending->stopped) {
                                              return;
                                          }
                                      }
                                      if (std::holds_alternative<PanelAccount>(resumed)) {
                                          startEvents(connection, token);
                                      } else {
                                          forgetSession(accepted.session);
                                      }
                                  });
        });
    }

    /// Subscribes an open WebSocket to the administration. On the loop.
    void startEvents(crow::websocket::connection *connection, std::uint64_t token)
    {
        if (administration == nullptr) {
            return;
        }
        const std::uint64_t subscription =
            administration->subscribe([shared = subscribers, connection, token](const AdminEvent &event) {
                shared->send(connection, token, toJson(event).dump());
            });
        const std::lock_guard lock(subscribers->mutex);
        const auto found = subscribers->connections.find(connection);
        if (found != subscribers->connections.end() && found->second.token == token) {
            found->second.subscription = subscription;
        } else {
            // Closed meanwhile
            administration->unsubscribe(subscription);
        }
    }

    void unsubscribe(crow::websocket::connection &closed)
    {
        std::uint64_t subscription = 0;
        {
            const std::lock_guard lock(subscribers->mutex);
            const auto found = subscribers->connections.find(&closed);
            if (found == subscribers->connections.end()) {
                return;
            }
            subscription = found->second.subscription;
            subscribers->connections.erase(found);
        }
        if (subscription != 0) {
            loop.post([this, subscription, pending = pending] {
                {
                    const std::lock_guard lock(pending->mutex);
                    if (pending->stopped) {
                        return;
                    }
                }
                if (administration != nullptr) {
                    administration->unsubscribe(subscription);
                }
            });
        }
    }

    // --- The panel ---

    Answer serve(const crow::request &request, crow::response &response)
    {
        const std::string &path = request.url;
        if (path == "/api" || path.starts_with("/api/")) {
            const auto value = cookie(request.get_header_value("Cookie"), SessionCookie);
            const bool session = value && parseSecret(*value);
            return session ? error(404, "no such route") : noSession();
        }
        if (request.method != crow::HTTPMethod::Get) {
            response.set_header("Allow", "GET");
            return page(405, "Method not allowed", "The panel only answers GET.");
        }
        if (path == "/" && request.url_params.get("token") != nullptr) {
            return exchangeToken(request);
        }
        return servePanel(path, response);
    }

    /// `GET /?token=…`: the local access token traded for a session cookie, then the address without it.
    Answer exchangeToken(const crow::request &request)
    {
        const std::string visitor = visitorOf(app, request).ip();
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
            sessions.push_back({id, newAccessToken(), Clock::now() + SessionLifetime});
        }
        logger().info("The panel was opened with the local access token, from {}", visitor);
        return redirect("/", {sessionCookie(id)});
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

void WebServer::setAdministration(Administration *administration)
{
    _impl->administration = administration;
}

void WebServer::setPanelAccounts(PanelAccounts *accounts)
{
    _impl->panelAccounts = accounts;
}

} // namespace rtype::server
