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
#include "rtype/lobby/Messages.hpp"

#include <crow/json.h>
#include <curl/curl.h>

#include <algorithm>
#include <charconv>
#include <format>
#include <limits>
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

/// Most bytes of an answer of the API of Discord kept: a token or a user is far smaller.
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
    /// Most bytes of the body kept: beyond, the transfer is cut.
    std::size_t limit = MaxAnswerSize;
};

/// One request to Discord, HTTPS only.
/// @param url The whole address: the API (apiUrl) or the CDN (cdnUrl) of the settings, and a path.
/// @param form A form for a POST, empty for a GET.
/// @param basic Whether the application authenticates with its id and its secret (HTTP Basic).
/// @param bearer A token of a user, for a GET.
/// @param limit Most bytes of the answer kept.
Answer request(const DiscordClientSettings &settings, const std::string &url, const std::string &form,
               bool basic, const std::string &bearer, const std::atomic<bool> *stopping,
               std::size_t limit = MaxAnswerSize)
{
    initializeCurl();
    Answer answer;
    answer.limit = limit;
    const CurlHandle curl(curl_easy_init());
    if (!curl) {
        answer.problem = "libcurl cannot start";
        return answer;
    }
    CURL *handle = curl.get();
    char errors[CURL_ERROR_SIZE] = {};
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
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &answer);
    curl_easy_setopt(
        handle, CURLOPT_WRITEFUNCTION, +[](char *data, std::size_t size, std::size_t count, void *user) {
            auto &answer = *static_cast<Answer *>(user);
            const std::size_t length = size * count;
            if (answer.body.size() + length > answer.limit) {
                // Cut: no answer of Discord is that long
                return std::size_t{0};
            }
            answer.body.append(data, length);
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

/// The tokens of an answer of /oauth2/token, if it has them as the Social SDK needs them.
std::optional<DiscordTokens> tokensOf(const crow::json::rvalue &json)
{
    const std::optional<std::string> access = textField(json, "access_token");
    const std::optional<std::string> refresh = textField(json, "refresh_token");
    if (!access || !refresh || !json.has("expires_in") ||
        json["expires_in"].t() != crow::json::type::Number || !rtype::lobby::isValidDiscordToken(*access) ||
        !rtype::lobby::isValidDiscordToken(*refresh)) {
        return std::nullopt;
    }
    const std::int64_t expiresIn = json["expires_in"].i();
    if (expiresIn <= 0 || expiresIn > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return DiscordTokens{
        .accessToken = *access, .refreshToken = *refresh, .expiresIn = static_cast<std::uint32_t>(expiresIn)};
}

bool isSnowflake(std::string_view id)
{
    return !id.empty() && id.size() <= 20 &&
           std::ranges::all_of(id, [](char c) { return c >= '0' && c <= '9'; });
}

/// The version of a picture as Discord names it: letters, digits and "_" (an animated one starts with "a_").
bool isAvatarHash(std::string_view hash)
{
    return !hash.empty() && hash.size() <= 64 && std::ranges::all_of(hash, [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    });
}

} // namespace

DiscordProfile profileOf(const DiscordUser &user)
{
    return DiscordProfile{.username = user.username,
                          .displayName = user.displayName,
                          .avatarHash = user.avatarHash,
                          .avatar = user.avatar};
}

bool isPng(std::string_view bytes)
{
    return bytes.starts_with("\x89PNG\r\n\x1a\n");
}

std::optional<std::string> checkDiscord(const DiscordClientSettings &settings)
{
    const Answer answer = request(settings, settings.apiUrl + "/oauth2/token",
                                  "grant_type=client_credentials&scope=identify", true, "", nullptr);
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
    return queue(Job{
        .authorization = std::move(authorization), .refreshToken = std::nullopt, .done = std::move(done)});
}

bool DiscordClient::refresh(std::string refreshToken, Done done)
{
    return queue(Job{.authorization = {}, .refreshToken = std::move(refreshToken), .done = std::move(done)});
}

bool DiscordClient::queue(Job job)
{
    {
        const std::lock_guard lock(_shared->mutex);
        if (_shared->pending >= _settings.maxPending) {
            logger().warning("{} requests waiting for Discord: one more refused", _shared->pending);
            return false;
        }
        ++_shared->pending;
    }
    {
        const std::lock_guard lock(_mutex);
        _jobs.push_back(std::move(job));
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
        DiscordAnswer answer = job.refreshToken ? renew(*job.refreshToken) : ask(job.authorization);
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
    const Answer token = request(_settings, _settings.apiUrl + "/oauth2/token", form, true, "", &_stopping);
    if (token.status == 400 && textField(crow::json::load(token.body), "error") == "invalid_grant") {
        logger().info("Discord refused a code: {}", failure(token));
        return DiscordFailure::Refused;
    }
    const crow::json::rvalue tokenJson =
        token.status == 200 ? crow::json::load(token.body) : crow::json::rvalue();
    const std::optional<std::string> accessToken = textField(tokenJson, "access_token");
    if (!accessToken) {
        logger().error("Discord gave no token for a code: {}", failure(token));
        return DiscordFailure::Unavailable;
    }

    std::optional<DiscordUser> me = userOf(*accessToken);
    if (!me) {
        return DiscordFailure::Unavailable;
    }
    DiscordUser &found = *me;
    if (_settings.discord.social) {
        found.tokens = tokensOf(tokenJson);
        if (!found.tokens) {
            // Signed in all the same: its client will not reach the social features of Discord
            logger().warning("Discord gave no refresh token for {}", found.id);
        }
    }
    if (!isAvatarHash(found.avatarHash)) {
        found.avatarHash.clear();
        return found;
    }
    // The picture, as a PNG even when it is animated: without it, the user signs in all the same
    const Answer picture = request(
        _settings,
        std::format("{}/avatars/{}/{}.png?size={}", _settings.cdnUrl, found.id, found.avatarHash, AvatarSize),
        "", false, "", &_stopping, MaxAvatarBytes);
    if (picture.status == 200 && isPng(picture.body)) {
        found.avatar = picture.body;
    } else {
        logger().warning("Discord gave no picture for {}: {}", found.id,
                         picture.status == 200 ? std::string("not a PNG") : failure(picture));
    }
    return found;
}

std::optional<DiscordUser> DiscordClient::userOf(const std::string &accessToken) const
{
    const Answer me = request(_settings, _settings.apiUrl + "/users/@me", "", false, accessToken, &_stopping);
    const crow::json::rvalue user = me.status == 200 ? crow::json::load(me.body) : crow::json::rvalue();
    const std::optional<std::string> id = textField(user, "id");
    if (!id || !isSnowflake(*id)) {
        logger().error("Discord did not tell who a token is: {}", failure(me));
        return std::nullopt;
    }
    return DiscordUser{.id = *id,
                       .username = textField(user, "username").value_or(""),
                       .displayName = textField(user, "global_name").value_or(""),
                       .avatarHash = textField(user, "avatar").value_or(""),
                       .avatar = {}};
}

DiscordAnswer DiscordClient::renew(const std::string &refreshToken) const
{
    const Answer token =
        request(_settings, _settings.apiUrl + "/oauth2/token",
                "grant_type=refresh_token&refresh_token=" + escape(refreshToken), true, "", &_stopping);
    const crow::json::rvalue json = token.status == 200 ? crow::json::load(token.body) : crow::json::rvalue();
    if (token.status == 400 && textField(crow::json::load(token.body), "error") == "invalid_grant") {
        logger().info("Discord refused a refresh token: {}", failure(token));
        return DiscordFailure::Refused;
    }
    std::optional<DiscordTokens> tokens = tokensOf(json);
    if (!tokens) {
        logger().error("Discord gave no token for a refresh: {}", failure(token));
        return DiscordFailure::Unavailable;
    }
    std::optional<DiscordUser> user = userOf(tokens->accessToken);
    if (!user) {
        return DiscordFailure::Unavailable;
    }
    user->avatarHash.clear();
    user->tokens = std::move(tokens);
    return std::move(*user);
}

} // namespace rtype::server
