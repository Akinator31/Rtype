#pragma once

#include "rtype/database/Address.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/Address.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rtype::server
{

/// A settings file refused. The message says where, `server.toml:12:5: unknown key 'prot' in [server]`, and
/// never holds a secret.
class SettingsError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/// A range of addresses, `172.16.0.0/12`: the first `prefix` bits of `address` (its port is 0).
struct Subnet {
    rtype::network::Address address;
    unsigned prefix = 0;

    friend bool operator==(const Subnet &, const Subnet &) = default;
};

/// [web]: the administration panel, which runs without the section on 127.0.0.1:8080.
struct WebSettings {
    /// Where the panel listens. Without accounts, a loopback address only. Port 0 takes a free port.
    rtype::network::Address listen = rtype::network::Address::loopbackIpv4(8080);
    /// The address the panel is reached at from outside, `https://admin.example.org`, without a trailing
    /// slash; empty: none. Needed by Discord.
    std::string publicUrl;
    /// The proxies whose X-Forwarded-For is believed.
    std::vector<Subnet> trustedProxies;
    /// The folder of the built panel; empty: admin/ next to the executable.
    std::filesystem::path panelDir;
};

/// [accounts]: accounts with an email and a password. Needs [mail].
struct AccountsSettings {
    bool enabled = true;
    /// Whether players can still play without an account.
    bool guests = true;
};

enum class MailSecurity {
    /// Plain connection upgraded with STARTTLS, port 587 by default.
    StartTls,
    /// TLS from the start, port 465 by default.
    Tls,
    /// In the clear, port 25 by default: for a relay on this machine or a test server (Mailpit). The codes
    /// travel unencrypted to it, so it takes no password.
    None,
};

/// [mail]: the server the codes are sent through. Needs [accounts].
struct MailSettings {
    std::string host;
    std::uint16_t port = 587;
    MailSecurity security = MailSecurity::StartTls;
    /// Empty: no authentication. Never with MailSecurity::None.
    std::string username;
    /// Read from `password_file`. Never logged.
    std::string password;
    /// The sender, `R-Type <rtype@example.org>`.
    std::string from;
};

/// [discord]: signing in with Discord. Needs [accounts] and `web.public_url`.
struct DiscordSettings {
    std::string clientId;
    /// Read from `client_secret_file`. Never logged.
    std::string clientSecret;
};

/// [admin]: the administrators the panel can neither demote nor ban.
struct AdminSettings {
    /// Each one an email, or `discord:<id>`; never a username, which anyone can take.
    std::vector<std::string> admins;
};

/// The settings file of the master, given with --settings (docs/guides/configure-a-server.md). Its [server]
/// section holds what the command line also gives, and the command line wins over it; the other sections are
/// in the file only.
struct ServerSettings {
    // [server]: nothing when the file leaves the key out
    std::optional<std::uint16_t> port;
    std::optional<std::uint16_t> firstGamePort;
    std::optional<std::uint16_t> lastGamePort;
    std::optional<std::filesystem::path> key;
    std::optional<rtype::database::Address> database;
    std::optional<std::filesystem::path> databaseFile;

    WebSettings web;
    std::optional<AccountsSettings> accounts;
    std::optional<MailSettings> mail;
    std::optional<DiscordSettings> discord;
    AdminSettings admin;

    /// Whether the players can have accounts: [accounts] there and enabled.
    [[nodiscard]] bool accountsEnabled() const { return accounts && accounts->enabled; }
};

/// Tells whether `address` is a loopback address, 127.0.0.0/8 or ::1: only this machine reaches it.
[[nodiscard]] bool isLoopback(const rtype::network::Address &address);

/// The address of a mailbox written `R-Type <rtype@example.org>` or `rtype@example.org`: what is between its
/// angle brackets, or all of it, without the spaces around it.
/// @return Nothing if that is not an email, or if the mailbox holds a line break.
[[nodiscard]] std::optional<std::string> mailboxAddress(std::string_view mailbox);

/// Reads a secret kept in a file of its own: its first line, the spaces at its end cut. On Unix, warns when
/// every user of the machine can read the file. The secret is never logged.
/// @throws std::runtime_error if the file cannot be read or holds no secret; the message names the file only.
[[nodiscard]] std::string readSecretFile(const std::filesystem::path &path, const rtype::log::Logger &logger);

/// Reads settings written in TOML. Unknown keys and sections are refused; relative paths are relative to the
/// folder of `file`, which also names the file in the errors. The secret files are read.
/// @throws SettingsError with the line and the column of the problem.
[[nodiscard]] ServerSettings parseSettings(std::string_view text, const std::filesystem::path &file,
                                           const rtype::log::Logger &logger);

/// Reads the settings file `file`.
/// @throws SettingsError if it cannot be read, or as parseSettings().
[[nodiscard]] ServerSettings loadSettings(const std::filesystem::path &file,
                                          const rtype::log::Logger &logger);

} // namespace rtype::server
