#pragma once

// A Discord of the tests: the two routes of its API the master uses (/oauth2/token and /users/@me) and the
// pictures of its CDN (/cdn/avatars/<id>/<hash>.png), on the loopback, over HTTP. A user given to accept() or
// authorize() with an avatarHash has its `avatar` served as its picture, whatever its bytes. The tests play
// the player and the Discord Social SDK with accept(), which gives what the client would get once the user
// accepted, and the administrator in its browser with authorize(). A small Crow server, in FakeDiscord.cpp so
// that its headers are compiled once. Built with CROW_ENABLE_SSL, as in rtype_fake_services
// (tests/helpers/fake_services.cpp), it also speaks HTTPS, as discord.com.

#include "Accounts/Discord.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace testing
{

/// Where a fake Discord listens, and with what certificate.
struct FakeDiscordEndpoint {
    std::string address = "127.0.0.1";
    /// 0: a free port.
    std::uint16_t port = 0;
    /// PEM files of the certificate and its key; empty: HTTP. Needs CROW_ENABLE_SSL.
    std::string certificateFile;
    std::string keyFile;
};

class FakeDiscord
{
    struct Impl;
    std::shared_ptr<Impl> _impl;

public:
    /// The application the master is set up with.
    static constexpr const char *ClientId = "1234567890123456789";
    static constexpr const char *ClientSecret = "fake-discord-secret";
    /// Where the Social SDK says Discord sent the code back.
    static constexpr const char *RedirectUri = "http://127.0.0.1/callback";

    /// Listens on a free port of the loopback, over HTTP.
    FakeDiscord();
    /// @throws std::runtime_error if it cannot listen there, or has no TLS in this build when asked for it.
    explicit FakeDiscord(const FakeDiscordEndpoint &endpoint);
    ~FakeDiscord();

    FakeDiscord(const FakeDiscord &) = delete;
    FakeDiscord &operator=(const FakeDiscord &) = delete;

    /// What the master needs to reach this Discord with the application above.
    [[nodiscard]] rtype::server::DiscordClientSettings settings() const;

    /// The user accepts the application: a new code, single use, bound to a new code verifier and to
    /// RedirectUri, as the Social SDK gives them to the client.
    [[nodiscard]] rtype::server::DiscordAuthorization accept(const rtype::server::DiscordUser &user);

    /// The user accepts the application in the browser, as the panel asks: a new code, single use, bound to
    /// the challenge of PKCE (S256) and the redirect address of the authorization page.
    [[nodiscard]] std::string authorize(const rtype::server::DiscordUser &user, const std::string &challenge,
                                        const std::string &redirectUri);

    /// Discord down: every request gets 503.
    void setDown(bool down);
    /// The codes the master tried to exchange, right or wrong.
    [[nodiscard]] std::size_t exchanges() const;
};

} // namespace testing
