#pragma once

#include "Accounts/MailMessages.hpp"

#include <chrono>
#include <string>
#include <string_view>

namespace rtype::server
{

/// A mail to send: where to, and what it says.
struct Mail {
    /// The address it goes to, as normalizeEmail() keeps it.
    std::string to;
    MailContent content;
};

/// What became of a mail given to a Mailer, at once.
enum class MailQueued {
    /// It will be sent, retried if the mail server fails.
    Yes,
    /// Too many mails wait already: none is added (`MAIL_UNAVAILABLE` for the lobby).
    Busy,
    /// The address got its most mails of the hour already: none is added.
    AddressLimit,
    /// The address is not an email, or holds a line break: never sent.
    InvalidAddress,
};

/// Sends the mails of the accounts, never on the event loop of the master
/// (docs/architecture/accounts.md#the-mails). CurlMailer sends them through the mail server of the settings;
/// the tests give a fake that keeps them.
class Mailer
{
public:
    virtual ~Mailer() = default;

    /// Queues a mail and returns at once: whether it reached the mail server only the logs tell.
    virtual MailQueued send(Mail mail) = 0;
};

/// Writes a mail as SMTP carries it (RFC 5322 and MIME): its headers, the subject and the name of the sender
/// encoded when they are not ASCII, then its text and its HTML as the two parts of a multipart/alternative,
/// in UTF-8 and base64. Every line ends with CRLF.
/// @param from The sender, `R-Type <rtype@example.org>`.
/// @param messageId Unique to the mail, `<...@example.org>`; the boundary of its parts is made from it.
[[nodiscard]] std::string formatMail(const Mail &mail, std::string_view from,
                                     std::chrono::system_clock::time_point date, std::string_view messageId);

} // namespace rtype::server
