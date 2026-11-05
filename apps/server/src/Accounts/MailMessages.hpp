#pragma once

#include <chrono>
#include <string>
#include <string_view>

/// The emails the master sends to the players of its accounts (docs/architecture/accounts.md#the-mails), in
/// English and French. Pure functions: the lobby writes a mail with them, then gives it to the Mailer.
namespace rtype::server
{

/// The languages the mails are written in.
enum class MailLanguage {
    English,
    French,
};

/// The language of the mails of a player, from the locale its client sends: `fr`, `fr-FR` or `fr_CA` are
/// French, anything else English.
[[nodiscard]] MailLanguage mailLanguage(std::string_view locale);

/// A mail, written: its subject, then its body twice, as text and as HTML, which say the same.
struct MailContent {
    std::string subject;
    std::string text;
    std::string html;
};

/// The code that verifies the email of a new account. The code large, the time it is valid stated, no link:
/// the player types it in the game.
[[nodiscard]] MailContent verificationMail(MailLanguage language, std::string_view username,
                                           std::string_view code, std::chrono::minutes validFor);

/// The code that sets a new password, asked from the sign-in of the game.
[[nodiscard]] MailContent resetMail(MailLanguage language, std::string_view username, std::string_view code,
                                    std::chrono::minutes validFor);

/// Sent in place of a code when someone registers with an email that has an account already: its holder
/// learns it, and the one who registered learns nothing from the answer of the lobby.
[[nodiscard]] MailContent existingAccountMail(MailLanguage language, std::string_view username);

/// Sent when an account reaches the daily limit of wrong codes: no code is checked until the next day (UTC).
[[nodiscard]] MailContent tooManyCodesMail(MailLanguage language, std::string_view username);

} // namespace rtype::server
