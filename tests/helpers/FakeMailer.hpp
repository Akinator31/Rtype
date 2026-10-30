#pragma once

// A mailer that sends nothing and keeps the mails, for the tests of the accounts: the code a player would
// read in its mailbox is read from here.

#include "Accounts/Mailer.hpp"

#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace testing
{

struct FakeMailer final : rtype::server::Mailer {
    std::vector<rtype::server::Mail> mails;
    /// What send() answers: anything but Yes keeps nothing.
    rtype::server::MailQueued answer = rtype::server::MailQueued::Yes;

    rtype::server::MailQueued send(rtype::server::Mail mail) override
    {
        if (answer == rtype::server::MailQueued::Yes) {
            mails.push_back(std::move(mail));
        }
        return answer;
    }

    /// The mails sent to an address.
    [[nodiscard]] std::size_t count(const std::string &to) const
    {
        std::size_t total = 0;
        for (const rtype::server::Mail &mail : mails) {
            total += mail.to == to ? 1 : 0;
        }
        return total;
    }

    /// The last mail sent to an address, if any.
    [[nodiscard]] std::optional<rtype::server::Mail> last(const std::string &to) const
    {
        for (auto mail = mails.rbegin(); mail != mails.rend(); ++mail) {
            if (mail->to == to) {
                return *mail;
            }
        }
        return std::nullopt;
    }

    /// The code of the last mail sent to an address: its 6 digits.
    [[nodiscard]] std::optional<std::string> code(const std::string &to) const
    {
        const std::optional<rtype::server::Mail> mail = last(to);
        if (!mail) {
            return std::nullopt;
        }
        static const std::regex digits(R"(\b[0-9]{6}\b)");
        std::smatch match;
        if (!std::regex_search(mail->content.text, match, digits)) {
            return std::nullopt;
        }
        return match.str();
    }
};

} // namespace testing
