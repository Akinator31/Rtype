#pragma once

#include "Accounts/AccountStore.hpp"
#include "Common/Settings.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <variant>

namespace rtype::server
{

/// What the client of a player got from Discord once the player accepted (DISCORD_CONFIRM): the authorization
/// code, the code verifier of PKCE whose challenge the client gave Discord, and the address Discord sent the
/// code back to. Secret: never logged.
struct DiscordAuthorization {
    std::string code;
    std::string codeVerifier;
    std::string redirectUri;
};

/// The tokens of a Discord user, for the Social SDK of its client (DISCORD_TOKEN): kept by the master only
/// until they are sent. Secret: never logged.
struct DiscordTokens {
    std::string accessToken;
    std::string refreshToken;
    /// The access token expires in this many seconds.
    std::uint32_t expiresIn = 0;

    friend bool operator==(const DiscordTokens &, const DiscordTokens &) = default;
};

/// A Discord user, as `/users/@me` tells it, with its picture.
struct DiscordUser {
    /// Its id, a snowflake in decimal digits: what the master keeps to sign it in.
    std::string id;
    /// Its name on Discord (`@username`), for the logs and as the username the client proposes.
    std::string username;
    /// The name it shows on Discord (`global_name`), empty if it has none.
    std::string displayName{};
    /// The version of its picture on Discord (`avatar`), empty if it has none.
    std::string avatarHash{};
    /// Its picture, a PNG of AvatarSize pixels from the CDN of Discord; empty if it has none, or if the CDN
    /// failed: the user signs in all the same.
    std::string avatar{};
    /// Its tokens, with `[discord] social` only.
    std::optional<DiscordTokens> tokens{};

    friend bool operator==(const DiscordUser &, const DiscordUser &) = default;
};

/// What the master keeps of a Discord user (saveDiscordProfile()).
[[nodiscard]] DiscordProfile profileOf(const DiscordUser &user);

/// Why Discord gave no user.
enum class DiscordFailure {
    /// Discord refused the code: wrong, used already, expired, or not made for this verifier or address.
    Refused,
    /// Discord could not be reached, or answered something else than a user: the player tries again later.
    Unavailable,
};

using DiscordAnswer = std::variant<DiscordUser, DiscordFailure>;

/// The size of the pictures asked of Discord, in pixels.
inline constexpr int AvatarSize = 128;
/// Most bytes of a picture kept: Discord sends far less at AvatarSize.
inline constexpr std::size_t MaxAvatarBytes = 256 * 1024;

/// Whether the bytes start as a PNG image does.
[[nodiscard]] bool isPng(std::string_view bytes);

/// How the master reaches Discord.
struct DiscordClientSettings {
    /// [discord] of the settings: the id of the application and its secret.
    DiscordSettings discord;
    /// The address of the API of Discord, without a trailing slash. Changed by the tests only, for their fake
    /// Discord: neither the settings file nor the command line reach it.
    std::string apiUrl = "https://discord.com/api/v10";
    /// The address of the CDN of Discord, which serves the pictures. Changed by the tests only, as apiUrl.
    std::string cdnUrl = "https://cdn.discordapp.com";
    /// Most requests not answered yet, the one running included.
    std::size_t maxPending = 16;
    /// Longest wait for the connection, then for each whole exchange with Discord.
    std::chrono::milliseconds connectTimeout = std::chrono::seconds(10);
    std::chrono::milliseconds timeout = std::chrono::seconds(15);
};

/// Checks that Discord accepts the id and the secret of the application, without any player: a token of the
/// application itself (`client_credentials`), asked then dropped.
/// @return Nothing if it does, or why not, without the secret.
[[nodiscard]] std::optional<std::string> checkDiscord(const DiscordClientSettings &settings);

/// Asks Discord who a player is, with libcurl, on a thread of its own
/// (docs/architecture/accounts.md#discord): the authorization code of DISCORD_CONFIRM exchanged for a token
/// (`/oauth2/token`, with the secret of the application and the code verifier of PKCE), then `/users/@me`
/// with that token, then its picture from the CDN, if it has one. The master keeps the id of the user, its
/// names and its picture. The tokens are dropped at once, unless `[discord] social` gives them to the player
/// (DiscordUser::tokens), and refresh() renews them from the refresh token of the player.
///
/// - **The results come back through `post`**, given to the constructor: the post() of the event loop of the
///   master.
/// - **The queue is bounded**: beyond `maxPending` requests, identify() refuses at once (`BUSY` for the
///   lobby). A request holds its place until its result is delivered.
/// - **Destroying it** drops the requests not started, cuts the one running, and never calls their `done`.
///
/// Nothing it logs holds the secret, a code or a token.
class DiscordClient
{
public:
    /// Hands a function to the thread that waits for the results. It must stay valid until the DiscordClient
    /// is destroyed.
    using Post = std::function<void(std::function<void()>)>;
    using Done = std::function<void(DiscordAnswer)>;

private:
    struct Job {
        DiscordAuthorization authorization;
        /// A refresh of the tokens of a player, instead of an authorization.
        std::optional<std::string> refreshToken;
        Done done;
    };
    /// What outlives the client: the count of the requests whose results are still to be delivered.
    struct Shared {
        std::mutex mutex;
        std::size_t pending = 0;
    };

    DiscordClientSettings _settings;
    Post _post;
    std::shared_ptr<Shared> _shared = std::make_shared<Shared>();
    std::mutex _mutex;
    std::condition_variable _wake;
    std::deque<Job> _jobs;
    /// Read by the transfer running, which it cuts.
    std::atomic<bool> _stopping = false;
    std::thread _thread;

    void loop();
    /// The requests to Discord, without holding the lock.
    [[nodiscard]] DiscordAnswer ask(const DiscordAuthorization &authorization) const;
    [[nodiscard]] DiscordAnswer renew(const std::string &refreshToken) const;
    /// The user of a token, `/users/@me`; nothing if Discord does not tell it.
    [[nodiscard]] std::optional<DiscordUser> userOf(const std::string &accessToken) const;
    /// Takes a job, or refuses it beyond `maxPending`.
    bool queue(Job job);

public:
    /// @throws std::invalid_argument if the id of the application is not a number.
    DiscordClient(DiscordClientSettings settings, Post post);
    ~DiscordClient();

    DiscordClient(const DiscordClient &) = delete;
    DiscordClient &operator=(const DiscordClient &) = delete;
    DiscordClient(DiscordClient &&) = delete;
    DiscordClient &operator=(DiscordClient &&) = delete;

    /// The id of the application, which DISCORD_AUTHORIZE gives the clients.
    [[nodiscard]] std::uint64_t applicationId() const;

    /// `[discord] social`: the players get their tokens.
    [[nodiscard]] bool social() const { return _settings.discord.social; }

    /// Asks Discord who accepted, then calls `done` through post with the user or why there is none.
    /// @return False if the queue is full: nothing is done, and `done` is never called.
    bool identify(DiscordAuthorization authorization, Done done);
    /// Renews the tokens of a player from its refresh token (`grant_type=refresh_token`), then calls `done`
    /// through post with the user they belong to, its names but no picture, and the new tokens: Refused if
    /// Discord refuses the refresh token. Needs `[discord] social`.
    /// @return False if the queue is full: nothing is done, and `done` is never called.
    bool refresh(std::string refreshToken, Done done);

    /// The requests whose result is not delivered yet.
    [[nodiscard]] std::size_t pending() const;
};

} // namespace rtype::server
