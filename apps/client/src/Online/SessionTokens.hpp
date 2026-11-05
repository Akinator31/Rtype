#pragma once

#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/lobby/Messages.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace rtype::client
{

/// The session token of the account signed in on each server (docs/protocol/lobby.md, section 6.2), kept in
/// the settings under `servers.session.<address>` (serverAddress()), so that the player comes back signed in.
///
/// @code
/// SessionTokens sessions(engine.getSettings());
/// LobbyClient lobby(loop, server, trusted, name, handlers, locale, sessions.token(address));
/// // SIGNED_IN: sessions.keep(address, signedIn.token)
/// // SIGN_OUT, DELETE_ACCOUNT, SESSION_EXPIRED: sessions.forget(address)
/// @endcode
///
/// A token is a secret: it is never logged, and the settings file is readable by the player only.
class SessionTokens
{
    engine::Settings &_settings;

public:
    /// Where the tokens are kept in the settings.
    static constexpr std::string_view SettingsKeyPrefix = "servers.session.";

    explicit SessionTokens(engine::Settings &settings);

    /// The token kept for this server, if any. A value that is not 64 hexadecimal digits is none.
    [[nodiscard]] std::optional<lobby::SessionToken> token(const std::string &address) const;

    /// Keeps the token for this server, and writes the settings.
    /// @return False if the settings could not be written: the token is then only kept until the client
    /// closes.
    bool keep(const std::string &address, const lobby::SessionToken &token);
    /// Forgets the token of this server, and writes the settings.
    /// @return A token was forgotten.
    bool forget(const std::string &address);
};

} // namespace rtype::client
