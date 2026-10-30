#include "Accounts/Discord.hpp"

#include "rtype/log/Logger.hpp"

// libcurl includes windows.h: without its min and max macros, which break std::min
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#include <crow/json.h>
#include <curl/curl.h>

#include <algorithm>
#include <charconv>
#include <format>
#include <stdexcept>
#include <utility>

namespace rtype::server
{

namespace
{

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("discord");
    return instance;
}

/// libcurl wants its global state made once, before its first handle.
void initializeCurl()
{
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

/// Most bytes of an answer of Discord kept: a token or a user is far smaller.
constexpr std::size_t MaxAnswerSize = 64 * 1024;

struct CurlDeleter {
    void operator()(CURL *curl) const { curl_easy_cleanup(curl); }
};
struct ListDeleter {
    void operator()(curl_slist *list) const { curl_slist_free_all(list); }
};
using CurlHandle = std::unique_ptr<CURL, CurlDeleter>;
using CurlList = std::unique_ptr<curl_slist, ListDeleter>;

/// What Discord answered a request.
struct Answer {
    /// 0 if no answer came: the reason is in `problem`.
    long status = 0;
    std::string body;
    std::string problem;
};

/// One request to the API of Discord, HTTPS only.
/// @param form A form for a POST, empty for a GET.
/// @param basic Whether the application authenticates with its id and its secret (HTTP Basic).
/// @param bearer A token of a user, for a GET.
Answer request(const DiscordClientSettings &settings, const std::string &path, const std::string &form,
               bool basic, const std::string &bearer, const std::atomic<bool> *stopping)
{
    initializeCurl();
    Answer answer;
    const CurlHandle curl(curl_easy_init());
    if (!curl) {
        answer.problem = "libcurl cannot start";
        return answer;
    }
    CURL *handle = curl.get();
    char errors[CURL_ERROR_SIZE] = {};
    const std::string url = settings.apiUrl + path;
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    // HTTP for the fake Discord of the tests, on the loopback: the address is never one of the settings
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "https,http");
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(settings.connectTimeout.count()));
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, static_cast<long>(settings.timeout.count()));
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errors);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "r-type_server (https://github.com/Akinator31/Rtype)");
    if (!form.empty()) {
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, form.c_str());
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(form.size()));
    }
    if (basic) {
        curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        curl_easy_setopt(handle, CURLOPT_USERNAME, settings.discord.clientId.c_str());
        curl_easy_setopt(handle, CURLOPT_PASSWORD, settings.discord.clientSecret.c_str());
    }
    CurlList headers(curl_slist_append(nullptr, "Accept: application/json"));
    std::string authorization;
    if (!bearer.empty()) {
        authorization = "Authorization: Bearer " + bearer;
        headers.reset(curl_slist_append(headers.release(), authorization.c_str()));
    }
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &answer.body);
    curl_easy_setopt(
        handle, CURLOPT_WRITEFUNCTION, +[](char *data, std::size_t size, std::size_t count, void *user) {
            auto &body = *static_cast<std::string *>(user);
            const std::size_t length = size * count;
            if (body.size() + length > MaxAnswerSize) {
                // Cut: no answer of Discord is that long
                return std::size_t{0};
            }
            body.append(data, length);
            return length;
        });
    if (stopping != nullptr) {
        // Cut when the client is destroyed, rather than wait for the timeout
        curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(handle, CURLOPT_XFERINFODATA, stopping);
        curl_easy_setopt(
            handle, CURLOPT_XFERINFOFUNCTION,
            +[](void *user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
                return static_cast<const std::atomic<bool> *>(user)->load() ? 1 : 0;
            });
    }
    const CURLcode result = curl_easy_perform(handle);
    if (result != CURLE_OK) {
        const std::string_view detail(errors);
        answer.problem = detail.empty() ? curl_easy_strerror(result)
                                        : std::format("{} ({})", curl_easy_strerror(result), detail);
        return answer;
    }
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &answer.status);
    return answer;
}

/// Encodes a value of a form (application/x-www-form-urlencoded).
std::string escape(const std::string &value)
{
    std::string escaped;
    for (const char c : value) {
        const auto byte = static_cast<unsigned char>(c);
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '.' || c == '_' || c == '~') {
            escaped += c;
        } else {
            escaped += std::format("%{:02X}", byte);
        }
    }
    return escaped;
}

/// The text of a field of a JSON object, if it is one.
std::optional<std::string> textField(const crow::json::rvalue &json, const char *name)
{
    if (!json || json.t() != crow::json::type::Object || !json.has(name) ||
        json[name].t() != crow::json::type::String) {
        return std::nullopt;
    }
    return std::string(json[name].s());
}

/// Why Discord answered with an error: its `error` field, or its status.
std::string failure(const Answer &answer)
{
    if (!answer.problem.empty()) {
        return answer.problem;
    }
    const crow::json::rvalue json = crow::json::load(answer.body);
    const std::optional<std::string> error = textField(json, "error");
    return error ? std::format("HTTP {}, {}", answer.status, *error) : std::format("HTTP {}", answer.status);
}

bool isSnowflake(std::string_view id)
{
    return !id.empty() && id.size() <= 20 &&
           std::ranges::all_of(id, [](char c) { return c >= '0' && c <= '9'; });
}

} // namespace

std::optional<std::string> checkDiscord(const DiscordClientSettings &settings)
{
    const Answer answer =
        request(settings, "/oauth2/token", "grant_type=client_credentials&scope=identify", true, "", nullptr);
    if (answer.status == 200 && textField(crow::json::load(answer.body), "access_token")) {
        return std::nullopt;
    }
    if (answer.status == 401) {
        return std::format("Discord refuses the id {} or its secret ({})", settings.discord.clientId,
                           failure(answer));
    }
    return failure(answer);
}

DiscordClient::DiscordClient(DiscordClientSettings settings, Post post)
    : _settings(std::move(settings)), _post(std::move(post))
{
    (void)applicationId();
    initializeCurl();
    _thread = std::thread([this] { loop(); });
}

DiscordClient::~DiscordClient()
{
    {
        const std::lock_guard lock(_mutex);
        _stopping = true;
        _jobs.clear();
    }
    _wake.notify_all();
    _thread.join();
}

std::uint64_t DiscordClient::applicationId() const
{
    const std::string &id = _settings.discord.clientId;
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(id.data(), id.data() + id.size(), value);
    if (error != std::errc{} || end != id.data() + id.size() || value == 0) {
        throw std::invalid_argument(std::format("The Discord client id {} is not a number", id));
    }
    return value;
}

bool DiscordClient::identify(DiscordAuthorization authorization, Done done)
{
    {
        const std::lock_guard lock(_shared->mutex);
        if (_shared->pending >= _settings.maxPending) {
            logger().warning("{} sign-ins waiting for Discord: one more refused", _shared->pending);
            return false;
        }
        ++_shared->pending;
    }
    {
        const std::lock_guard lock(_mutex);
        _jobs.push_back(Job{.authorization = std::move(authorization), .done = std::move(done)});
    }
    _wake.notify_one();
    return true;
}

std::size_t DiscordClient::pending() const
{
    const std::lock_guard lock(_shared->mutex);
    return _shared->pending;
}

void DiscordClient::loop()
{
    while (true) {
        Job job;
        {
            std::unique_lock lock(_mutex);
            _wake.wait(lock, [this] { return _stopping || !_jobs.empty(); });
            if (_stopping) {
                return;
            }
            job = std::move(_jobs.front());
            _jobs.pop_front();
        }
        DiscordAnswer answer = ask(job.authorization);
        if (_stopping) {
            return;
        }
        // The place is freed when the result is delivered: the queue bounds what the loop has to handle too
        _post([shared = _shared, done = std::move(job.done), answer = std::move(answer)] {
            {
                const std::lock_guard lock(shared->mutex);
                --shared->pending;
            }
            done(answer);
        });
    }
}

DiscordAnswer DiscordClient::ask(const DiscordAuthorization &authorization) const
{
    const std::string form = std::format(
        "grant_type=authorization_code&code={}&redirect_uri={}&code_verifier={}", escape(authorization.code),
        escape(authorization.redirectUri), escape(authorization.codeVerifier));
    const Answer token = request(_settings, "/oauth2/token", form, true, "", &_stopping);
    if (token.status == 400 && textField(crow::json::load(token.body), "error") == "invalid_grant") {
        logger().info("Discord refused a code: {}", failure(token));
        return DiscordFailure::Refused;
    }
    const std::optional<std::string> accessToken =
        token.status == 200 ? textField(crow::json::load(token.body), "access_token") : std::nullopt;
    if (!accessToken) {
        logger().error("Discord gave no token for a code: {}", failure(token));
        return DiscordFailure::Unavailable;
    }

    const Answer me = request(_settings, "/users/@me", "", false, *accessToken, &_stopping);
    const crow::json::rvalue user = me.status == 200 ? crow::json::load(me.body) : crow::json::rvalue();
    const std::optional<std::string> id = textField(user, "id");
    if (!id || !isSnowflake(*id)) {
        logger().error("Discord did not tell who signed in: {}", failure(me));
        return DiscordFailure::Unavailable;
    }
    return DiscordUser{.id = *id, .username = textField(user, "username").value_or("")};
}

} // namespace rtype::server
