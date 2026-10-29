#include "Accounts/AccountRules.hpp"

#include "rtype/network/Text.hpp"

#include <algorithm>

namespace rtype::server
{

namespace
{

/// ASCII only: the other characters are kept as they are, and a username has none.
std::string lowerCase(std::string_view text)
{
    std::string lower(text);
    std::ranges::transform(lower, lower.begin(),
                           [](char c) { return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c; });
    return lower;
}

/// The characters of valid UTF-8: the bytes that do not continue a character.
std::size_t characterCount(std::string_view text)
{
    return static_cast<std::size_t>(
        std::ranges::count_if(text, [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

} // namespace

bool isValidUsername(std::string_view username)
{
    return username.size() >= MinUsernameLength && username.size() <= MaxUsernameLength &&
           std::ranges::all_of(username, [](char c) {
               return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                      c == '_' || c == '-';
           });
}

std::string usernameKey(std::string_view username)
{
    return lowerCase(username);
}

std::optional<std::string> normalizeEmail(std::string_view email)
{
    while (!email.empty() && isSpace(email.front())) {
        email.remove_prefix(1);
    }
    while (!email.empty() && isSpace(email.back())) {
        email.remove_suffix(1);
    }
    if (email.size() > MaxEmailLength || !rtype::network::isValidText(email) ||
        std::ranges::any_of(email, isSpace)) {
        return std::nullopt;
    }
    const std::size_t at = email.rfind('@');
    if (at == std::string_view::npos || at == 0) {
        return std::nullopt;
    }
    const std::string_view domain = email.substr(at + 1);
    const std::size_t dot = domain.find('.');
    if (dot == std::string_view::npos || dot == 0 || domain.back() == '.') {
        return std::nullopt;
    }
    return std::string(email);
}

std::string emailKey(std::string_view email)
{
    return lowerCase(email);
}

PasswordProblem checkPassword(std::string_view password, std::string_view username, std::string_view email)
{
    if (!rtype::network::isValidText(password)) {
        return PasswordProblem::InvalidText;
    }
    const std::size_t length = characterCount(password);
    if (length < MinPasswordLength) {
        return PasswordProblem::TooShort;
    }
    if (length > MaxPasswordLength) {
        return PasswordProblem::TooLong;
    }
    const std::string lower = lowerCase(password);
    if (lower == lowerCase(username)) {
        return PasswordProblem::SameAsUsername;
    }
    if (!email.empty() && lower == lowerCase(email)) {
        return PasswordProblem::SameAsEmail;
    }
    return PasswordProblem::None;
}

} // namespace rtype::server
