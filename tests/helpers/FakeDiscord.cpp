#include "FakeDiscord.hpp"

#include "Accounts/Mailer.hpp"
#include "rtype/network/Crypto.hpp"

#include <crow.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <future>
#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace testing
{

namespace
{

/// 48 random hexadecimal digits: unreserved characters, as a code verifier of PKCE wants.
std::string randomText()
{
    std::string text;
    for (const std::byte byte : rtype::network::randomBytes<24>()) {
        text += std::format("{:02x}", std::to_integer<unsigned>(byte));
    }
    return text;
}

crow::response json(int status, crow::json::wvalue body)
{
    crow::response response(status, body.dump());
    response.set_header("Content-Type", "application/json");
    return response;
}

} // namespace

struct FakeDiscord::Impl {
    /// What a code was given for.
    struct Grant {
        rtype::server::DiscordUser user;
        /// The verifier itself, as the Social SDK keeps it, or the challenge of a verifier (S256) the panel
        /// gave.
        std::string verifier;
        bool challenge = false;
        std::string redirectUri = RedirectUri;
    };

    crow::SimpleApp app;
    std::future<void> running;
    std::mutex mutex;
    std::map<std::string, Grant> codes;
    std::map<std::string, rtype::server::DiscordUser> tokens;
    bool down = false;
    std::size_t exchanges = 0;

    /// The answer of /oauth2/token.
    crow::response token(const crow::request &request)
    {
        const std::lock_guard lock(mutex);
        if (down) {
            return json(503, {{"message", "service unavailable"}});
        }
        const std::string expected =
            "Basic " + rtype::server::base64(std::format("{}:{}", ClientId, ClientSecret));
        if (request.get_header_value("Authorization") != expected) {
            return json(401, {{"error", "invalid_client"}});
        }
        const crow::query_string form("?" + request.body);
        const std::string grant = form.get("grant_type") != nullptr ? form.get("grant_type") : "";
        if (grant == "client_credentials") {
            return json(200, {{"access_token", "application-token"}, {"token_type", "Bearer"}});
        }
        ++exchanges;
        const char *code = form.get("code");
        const auto found = code != nullptr ? codes.find(code) : codes.end();
        if (grant != "authorization_code" || found == codes.end()) {
            return json(400, {{"error", "invalid_grant"}});
        }
        // Single use, right or wrong, as Discord
        const Grant granted = found->second;
        codes.erase(found);
        const char *verifier = form.get("code_verifier");
        const char *redirect = form.get("redirect_uri");
        const auto verified = [&] {
            if (verifier == nullptr) {
                return false;
            }
            if (!granted.challenge) {
                return granted.verifier == verifier;
            }
            const rtype::network::Sha256 digest = rtype::network::sha256(std::string_view(verifier));
            std::string encoded = rtype::server::base64(
                std::string_view(reinterpret_cast<const char *>(digest.data()), digest.size()));
            std::ranges::replace(encoded, '+', '-');
            std::ranges::replace(encoded, '/', '_');
            std::erase(encoded, '=');
            return encoded == granted.verifier;
        };
        if (!verified() || redirect == nullptr || std::string(redirect) != granted.redirectUri) {
            return json(400, {{"error", "invalid_grant"}});
        }
        const std::string access = randomText();
        tokens[access] = granted.user;
        return json(200, {{"access_token", access}, {"token_type", "Bearer"}, {"scope", "identify"}});
    }

    /// The answer of /users/@me.
    crow::response me(const crow::request &request)
    {
        const std::lock_guard lock(mutex);
        if (down) {
            return json(503, {{"message", "service unavailable"}});
        }
        const std::string authorization = request.get_header_value("Authorization");
        const auto found =
            authorization.starts_with("Bearer ") ? tokens.find(authorization.substr(7)) : tokens.end();
        if (found == tokens.end()) {
            return json(401, {{"message", "401: Unauthorized"}});
        }
        return json(200, {{"id", found->second.id}, {"username", found->second.username}});
    }
};

FakeDiscord::FakeDiscord() : FakeDiscord(FakeDiscordEndpoint{}) {}

FakeDiscord::FakeDiscord(const FakeDiscordEndpoint &endpoint) : _impl(std::make_shared<Impl>())
{
    Impl &impl = *_impl;
    CROW_ROUTE(impl.app, "/api/v10/oauth2/token")
        .methods(crow::HTTPMethod::Post)(
            [&impl](const crow::request &request) { return impl.token(request); });
    CROW_ROUTE(impl.app,
               "/api/v10/users/@me")([&impl](const crow::request &request) { return impl.me(request); });
    impl.app.signal_clear();
    impl.app.bindaddr(endpoint.address).port(endpoint.port).concurrency(1);
    if (!endpoint.certificateFile.empty()) {
#ifdef CROW_ENABLE_SSL
        impl.app.ssl_file(endpoint.certificateFile, endpoint.keyFile);
#else
        throw std::runtime_error("The fake Discord has no TLS in this build");
#endif
    }
    impl.running = impl.app.run_async();
    impl.app.wait_for_server_start(std::chrono::seconds(10));
    if (!impl.app.is_bound()) {
        throw std::runtime_error("The fake Discord cannot listen");
    }
}

FakeDiscord::~FakeDiscord()
{
    _impl->app.stop();
    if (_impl->running.valid()) {
        _impl->running.wait();
    }
}

rtype::server::DiscordClientSettings FakeDiscord::settings() const
{
    rtype::server::DiscordClientSettings settings;
    settings.discord.clientId = ClientId;
    settings.discord.clientSecret = ClientSecret;
    settings.apiUrl = std::format("http://127.0.0.1:{}/api/v10", _impl->app.port());
    settings.connectTimeout = std::chrono::seconds(2);
    settings.timeout = std::chrono::seconds(5);
    return settings;
}

rtype::server::DiscordAuthorization FakeDiscord::accept(const rtype::server::DiscordUser &user)
{
    rtype::server::DiscordAuthorization authorization{
        .code = randomText(), .codeVerifier = randomText(), .redirectUri = RedirectUri};
    const std::lock_guard lock(_impl->mutex);
    _impl->codes[authorization.code] = Impl::Grant{
        .user = user, .verifier = authorization.codeVerifier, .challenge = false, .redirectUri = RedirectUri};
    return authorization;
}

std::string FakeDiscord::authorize(const rtype::server::DiscordUser &user, const std::string &challenge,
                                   const std::string &redirectUri)
{
    std::string code = randomText();
    const std::lock_guard lock(_impl->mutex);
    _impl->codes[code] =
        Impl::Grant{.user = user, .verifier = challenge, .challenge = true, .redirectUri = redirectUri};
    return code;
}

void FakeDiscord::setDown(bool down)
{
    const std::lock_guard lock(_impl->mutex);
    _impl->down = down;
}

std::size_t FakeDiscord::exchanges() const
{
    const std::lock_guard lock(_impl->mutex);
    return _impl->exchanges;
}

} // namespace testing
