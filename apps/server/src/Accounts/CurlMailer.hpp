#pragma once

#include "Accounts/Mailer.hpp"
#include "Common/Settings.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace rtype::server
{

/// How CurlMailer reaches the mail server, and what it allows.
struct CurlMailerSettings {
    /// [mail] of the settings: the server, its security, its account, the sender.
    MailSettings mail;
    /// Most mails not sent yet, the one being sent and those waiting to be tried again included.
    std::size_t maxPending = 64;
    /// Tries of a mail, the first one included, before it is given up.
    unsigned attempts = 4;
    /// The wait before the second try, doubled before each next one: 30 s, 1 min, 2 min.
    std::chrono::milliseconds retryDelay = std::chrono::seconds(30);
    /// Most mails one address receives in `addressWindow`.
    unsigned perAddress = 5;
    std::chrono::milliseconds addressWindow = std::chrono::hours(1);
    /// Longest wait for the connection, then for the whole exchange with the mail server.
    std::chrono::milliseconds connectTimeout = std::chrono::seconds(10);
    std::chrono::milliseconds timeout = std::chrono::seconds(30);
    /// The certificates the mail server is checked against; empty: those of the system. For the tests, whose
    /// mail server signs its own.
    std::filesystem::path caFile;
};

/// Checks that the mail server of the settings can be used, without sending anything: the connection, TLS
/// (STARTTLS or from the start), the authentication, then QUIT.
/// @return Nothing if it can, or why not, without the password.
[[nodiscard]] std::optional<std::string> checkMailServer(const CurlMailerSettings &settings);

/// Sends the mails through the mail server of the settings, with libcurl, on a thread of its own
/// (docs/architecture/accounts.md#the-mails).
///
/// - **A bounded queue**: beyond `maxPending` mails not sent yet, send() answers MailQueued::Busy at once.
/// - **Retried**: a mail the server refuses or does not take, its connection lost, is tried again after
///   `retryDelay`, doubled each time, `attempts` times in all; then given up, and logged.
/// - **A limit per address**: `perAddress` mails per `addressWindow` to one address (ignoring case), counted
///   when they are queued; beyond, MailQueued::AddressLimit.
/// - **Destroying it** drops the mails not sent yet, and cuts the one being sent.
///
/// Every function can be called from any thread. Nothing it logs holds the password, nor the content of a
/// mail, where a code may be.
class CurlMailer final : public Mailer
{
public:
    /// What became of the mails, for the tests and the logs.
    struct Counts {
        std::size_t sent = 0;
        std::size_t failed = 0;
    };

private:
    struct Job {
        Mail mail;
        /// The tries done already.
        unsigned tries = 0;
        std::chrono::steady_clock::time_point notBefore;
    };

    CurlMailerSettings _settings;
    std::string _sender;
    mutable std::mutex _mutex;
    std::condition_variable _wake;
    std::deque<Job> _jobs;
    /// The one being sent: it counts as pending.
    bool _sending = false;
    /// Read by the transfer running, which it cuts.
    std::atomic<bool> _stopping = false;
    Counts _counts;
    /// When each address got its last mails, oldest first.
    std::map<std::string, std::deque<std::chrono::steady_clock::time_point>> _recent;
    std::thread _thread;

    void loop();
    /// Sends one mail, without holding the lock.
    /// @return Nothing if it went, or why not.
    std::optional<std::string> deliver(const Mail &mail);

public:
    explicit CurlMailer(CurlMailerSettings settings);
    ~CurlMailer() override;

    CurlMailer(const CurlMailer &) = delete;
    CurlMailer &operator=(const CurlMailer &) = delete;
    CurlMailer(CurlMailer &&) = delete;
    CurlMailer &operator=(CurlMailer &&) = delete;

    /// Queues a mail, refused at once if the queue or its address is full, or if the address is not one.
    MailQueued send(Mail mail) override;

    /// The mails not sent yet nor given up.
    [[nodiscard]] std::size_t pending() const;
    [[nodiscard]] Counts counts() const;
};

} // namespace rtype::server
