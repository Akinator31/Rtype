#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

/// What a username, an email and a password must be (docs/architecture/accounts.md#the-rules). Pure
/// functions: the lobby checks a request with them before it reaches the database or the hasher.
namespace rtype::server
{

inline constexpr std::size_t MinUsernameLength = 3;
inline constexpr std::size_t MaxUsernameLength = 16;
/// In characters, not bytes.
inline constexpr std::size_t MinPasswordLength = 10;
inline constexpr std::size_t MaxPasswordLength = 128;
/// The longest address SMTP carries.
inline constexpr std::size_t MaxEmailLength = 254;

/// Tells whether a username is allowed: 3 to 16 of `A-Z a-z 0-9 _ -`.
[[nodiscard]] bool isValidUsername(std::string_view username);

/// What two usernames that differ only by case share: the username in lower case. Unique in the database.
[[nodiscard]] std::string usernameKey(std::string_view username);

/// The email as it is kept: without the spaces around it.
/// @return Nothing if it is not an address: one `@` with something before it, a domain with a dot after it,
/// at most 254 characters, no space nor control character. Whether it receives mail, only a code sent to it
/// tells.
[[nodiscard]] std::optional<std::string> normalizeEmail(std::string_view email);

/// What two emails that differ only by case share: the normalized email in lower case. Unique in the
/// database.
[[nodiscard]] std::string emailKey(std::string_view email);

/// Why a password is refused.
enum class PasswordProblem {
    None,
    TooShort,
    TooLong,
    /// Not valid UTF-8, or with a control character.
    InvalidText,
    /// The username, ignoring case.
    SameAsUsername,
    /// The email, ignoring case.
    SameAsEmail,
};

/// Checks a new password: 10 to 128 characters, neither the username nor the email of its account.
/// @param email Empty for an account without one.
[[nodiscard]] PasswordProblem checkPassword(std::string_view password, std::string_view username,
                                            std::string_view email);

} // namespace rtype::server
