#include "Accounts/CurlMailer.hpp"

#include "Accounts/AccountRules.hpp"
#include "Common/Secret.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/Crypto.hpp"

// libcurl includes windows.h: without its min and max macros, which break std::min
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#include <curl/curl.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <memory>
#include <utility>

namespace rtype::server
{

namespace
{

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("mail");
    return instance;
}

/// libcurl wants its global state made once, before its first handle.
void initializeCurl()
{
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

/// `p…@example.org`: enough to tell mails apart in the logs, not enough to read the addresses of the players.
std::string maskedAddress(std::string_view address)
{
    const std::size_t at = address.find('@');
    if (at == std::string_view::npos || at == 0) {
        return "…";
    }
    return std::format("{}…{}", address.front(), address.substr(at));
}

struct CurlDeleter {
    void operator()(CURL *curl) const { curl_easy_cleanup(curl); }
};
struct ListDeleter {
    void operator()(curl_slist *list) const { curl_slist_free_all(list); }
};
using CurlHandle = std::unique_ptr<CURL, CurlDeleter>;
using CurlList = std::unique_ptr<curl_slist, ListDeleter>;

/// A handle with the options every exchange with the mail server shares: its address, its security, its
/// account, the time it is given.
CurlHandle connection(const CurlMailerSettings &settings, std::string_view sender, char *errors)
{
    initializeCurl();
    CurlHandle curl(curl_easy_init());
    if (!curl) {
        return curl;
    }
    const MailSettings &mail = settings.mail;
    const bool ipv6 = mail.host.find(':') != std::string::npos;
    // The path of the address is the name the master gives in EHLO: the domain of its sender
    const std::string domain(sender.substr(sender.find('@') + 1));
    const std::string url =
        std::format("{}://{}{}{}:{}/{}", mail.security == MailSecurity::Tls ? "smtps" : "smtp",
                    ipv6 ? "[" : "", mail.host, ipv6 ? "]" : "", mail.port, domain);
    CURL *handle = curl.get();
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "smtp,smtps");
    // STARTTLS required: a server that does not offer it is refused, never used in the clear unless the
    // settings say so
    curl_easy_setopt(
        handle, CURLOPT_USE_SSL,
        static_cast<long>(mail.security == MailSecurity::None ? CURLUSESSL_NONE : CURLUSESSL_ALL));
    if (!mail.username.empty()) {
        curl_easy_setopt(handle, CURLOPT_USERNAME, mail.username.c_str());
        curl_easy_setopt(handle, CURLOPT_PASSWORD, mail.password.c_str());
    }
    if (!settings.caFile.empty()) {
        const std::string caFile = settings.caFile.string();
        curl_easy_setopt(handle, CURLOPT_CAINFO, caFile.c_str());
    }
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(settings.connectTimeout.count()));
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, static_cast<long>(settings.timeout.count()));
    // No signal on a timeout: the master has other threads
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errors);
    // What the server answers a command goes nowhere, not to the standard output
    curl_easy_setopt(
        handle, CURLOPT_WRITEFUNCTION,
        +[](char *, std::size_t size, std::size_t count, void *) { return size * count; });
    return curl;
}

/// Why an exchange failed: what libcurl says, and the detail of its buffer if it has one.
std::string failure(CURLcode code, const char *errors)
{
    const std::string_view detail(errors);
    return detail.empty() ? curl_easy_strerror(code)
                          : std::format("{} ({})", curl_easy_strerror(code), detail);
}

/// What the upload of a mail reads.
struct Upload {
    std::string_view data;
    std::size_t offset = 0;
};

} // namespace

std::optional<std::string> checkMailServer(const CurlMailerSettings &settings)
{
    const std::optional<std::string> sender = mailboxAddress(settings.mail.from);
    if (!sender) {
        return std::format("the sender {} holds no address", settings.mail.from);
    }
    char errors[CURL_ERROR_SIZE] = {};
    const CurlHandle curl = connection(settings, *sender, errors);
    if (!curl) {
        return "libcurl cannot start";
    }
    // No recipient and nothing to upload: after its greeting, TLS and authentication, libcurl sends this
    // command alone, then QUIT
    curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "NOOP");
    const CURLcode result = curl_easy_perform(curl.get());
    if (result != CURLE_OK) {
        return failure(result, errors);
    }
    return std::nullopt;
}

CurlMailer::CurlMailer(CurlMailerSettings settings)
    : _settings(std::move(settings)), _sender(mailboxAddress(_settings.mail.from).value_or(""))
{
    initializeCurl();
    _thread = std::thread([this] { loop(); });
}

CurlMailer::~CurlMailer()
{
    {
        const std::lock_guard lock(_mutex);
        _stopping = true;
        if (!_jobs.empty()) {
            logger().warning("{} mails not sent: the master stops", _jobs.size());
        }
        _jobs.clear();
    }
    _wake.notify_all();
    _thread.join();
}

MailQueued CurlMailer::send(Mail mail)
{
    const std::optional<std::string> address = mailboxAddress(mail.to);
    if (!address || *address != mail.to || _sender.empty()) {
        // Its address is not logged: it could hold anything, line breaks included
        logger().error("A mail \"{}\" not sent: its address is not one", mail.content.subject);
        return MailQueued::InvalidAddress;
    }
    const auto now = std::chrono::steady_clock::now();
    const std::string key = emailKey(mail.to);
    {
        const std::lock_guard lock(_mutex);
        if (_jobs.size() + (_sending ? 1 : 0) >= _settings.maxPending) {
            logger().warning("{} mails waiting to be sent: one more refused", _settings.maxPending);
            return MailQueued::Busy;
        }
        // The times older than the window are forgotten, and with them the addresses that have none left
        for (auto it = _recent.begin(); it != _recent.end();) {
            auto &times = it->second;
            while (!times.empty() && now - times.front() >= _settings.addressWindow) {
                times.pop_front();
            }
            it = times.empty() ? _recent.erase(it) : std::next(it);
        }
        auto &times = _recent[key];
        if (times.size() >= _settings.perAddress) {
            logger().warning("A mail to {} refused: {} sent to it already within the hour",
                             maskedAddress(mail.to), times.size());
            return MailQueued::AddressLimit;
        }
        times.push_back(now);
        _jobs.push_back(Job{.mail = std::move(mail), .tries = 0, .notBefore = now});
    }
    _wake.notify_one();
    return MailQueued::Yes;
}

std::size_t CurlMailer::pending() const
{
    const std::lock_guard lock(_mutex);
    return _jobs.size() + (_sending ? 1 : 0);
}

CurlMailer::Counts CurlMailer::counts() const
{
    const std::lock_guard lock(_mutex);
    return _counts;
}

void CurlMailer::loop()
{
    std::unique_lock lock(_mutex);
    while (true) {
        // The first mail whose time has come; else, wait for the next time or a new mail
        const auto now = std::chrono::steady_clock::now();
        auto ready = std::ranges::find_if(_jobs, [now](const Job &job) { return job.notBefore <= now; });
        if (_stopping) {
            return;
        }
        if (ready == _jobs.end()) {
            if (_jobs.empty()) {
                _wake.wait(lock);
            } else {
                const auto next = std::ranges::min_element(_jobs, {}, &Job::notBefore)->notBefore;
                _wake.wait_until(lock, next);
            }
            continue;
        }
        Job job = std::move(*ready);
        _jobs.erase(ready);
        _sending = true;

        lock.unlock();
        ++job.tries;
        const std::optional<std::string> problem = deliver(job.mail);
        lock.lock();

        _sending = false;
        if (!problem) {
            ++_counts.sent;
            logger().info("Mail \"{}\" sent to {}", job.mail.content.subject, maskedAddress(job.mail.to));
        } else if (_stopping) {
            return;
        } else if (job.tries < _settings.attempts) {
            const auto delay = _settings.retryDelay * (1U << std::min(job.tries - 1U, 16U));
            logger().warning(
                "Mail to {} not sent, try {} of {}: {}; again in {}", maskedAddress(job.mail.to), job.tries,
                _settings.attempts, *problem,
                delay >= std::chrono::seconds(1)
                    ? std::format("{} s", std::chrono::duration_cast<std::chrono::seconds>(delay).count())
                    : std::format("{} ms", delay.count()));
            job.notBefore = std::chrono::steady_clock::now() + delay;
            _jobs.push_back(std::move(job));
        } else {
            ++_counts.failed;
            logger().error("Mail to {} given up after {} tries: {}", maskedAddress(job.mail.to), job.tries,
                           *problem);
        }
    }
}

std::optional<std::string> CurlMailer::deliver(const Mail &mail)
{
    const auto messageId =
        std::format("<{}@{}>", formatSecret(rtype::network::randomBytes<SecretSize>()).substr(0, 32),
                    _sender.substr(_sender.find('@') + 1));
    const std::string message =
        formatMail(mail, _settings.mail.from, std::chrono::system_clock::now(), messageId);

    char errors[CURL_ERROR_SIZE] = {};
    const CurlHandle curl = connection(_settings, _sender, errors);
    if (!curl) {
        return "libcurl cannot start";
    }
    CURL *handle = curl.get();
    const std::string from = std::format("<{}>", _sender);
    const std::string to = std::format("<{}>", mail.to);
    const CurlList recipients(curl_slist_append(nullptr, to.c_str()));
    curl_easy_setopt(handle, CURLOPT_MAIL_FROM, from.c_str());
    curl_easy_setopt(handle, CURLOPT_MAIL_RCPT, recipients.get());

    Upload upload{.data = message, .offset = 0};
    curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(handle, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(message.size()));
    curl_easy_setopt(handle, CURLOPT_READDATA, &upload);
    curl_easy_setopt(
        handle, CURLOPT_READFUNCTION,
        +[](char *buffer, std::size_t size, std::size_t count, void *user) -> std::size_t {
            auto &source = *static_cast<Upload *>(user);
            const std::size_t length = std::min(size * count, source.data.size() - source.offset);
            std::memcpy(buffer, source.data.data() + source.offset, length);
            source.offset += length;
            return length;
        });
    // Cut when the mailer is destroyed, rather than wait for the timeout
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, &_stopping);
    curl_easy_setopt(
        handle, CURLOPT_XFERINFOFUNCTION,
        +[](void *user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
            return static_cast<std::atomic<bool> *>(user)->load() ? 1 : 0;
        });

    const CURLcode result = curl_easy_perform(handle);
    if (result != CURLE_OK) {
        return failure(result, errors);
    }
    return std::nullopt;
}

} // namespace rtype::server
