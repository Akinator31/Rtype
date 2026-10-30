#pragma once

// A small SMTP server for the tests of the mails of the master, on a thread of its own: one connection at a
// time, in the clear, with STARTTLS, or TLS from the start, with or without an account. It keeps the mails it
// receives, and can be told to refuse them, to wait before it greets, or to go away. TLS needs OpenSSL
// (RTYPE_TEST_OPENSSL, Linux): its certificate is made at start, for 127.0.0.1, and written for the client to
// trust.

#include <asio.hpp>

#ifndef _WIN32
#include <csignal>
#endif
#ifdef RTYPE_TEST_OPENSSL
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#endif

#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace testing
{

/// Tells whether the fake mail server can speak TLS in this build.
inline constexpr bool SmtpTlsAvailable =
#ifdef RTYPE_TEST_OPENSSL
    true;
#else
    false;
#endif

enum class SmtpTls {
    None,
    StartTls,
    /// TLS from the start, as on port 465.
    Implicit,
};

struct SmtpSettings {
    SmtpTls tls = SmtpTls::None;
    /// The account the client must sign in with; empty username: none.
    std::string username;
    std::string password;
};

/// A mail received: its envelope, then its content as sent, dots unstuffed.
struct ReceivedMail {
    std::string from;
    std::vector<std::string> to;
    std::string data;
};

namespace detail
{

inline std::string decodeBase64(std::string_view text)
{
    std::string result;
    std::uint32_t group = 0;
    int bits = 0;
    for (const char c : text) {
        int value = -1;
        if (c >= 'A' && c <= 'Z') {
            value = c - 'A';
        } else if (c >= 'a' && c <= 'z') {
            value = c - 'a' + 26;
        } else if (c >= '0' && c <= '9') {
            value = c - '0' + 52;
        } else if (c == '+') {
            value = 62;
        } else if (c == '/') {
            value = 63;
        }
        if (value < 0) {
            continue;
        }
        group = (group << 6U) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            result += static_cast<char>((group >> static_cast<unsigned>(bits)) & 0xFFU);
        }
    }
    return result;
}

} // namespace detail

/// Decodes the base64 of a part of a mail, its lines joined.
inline std::string decodeBase64(std::string_view text)
{
    return detail::decodeBase64(text);
}

class SmtpServer
{
private:
    SmtpSettings _settings;
    asio::io_context _context;
    asio::ip::tcp::acceptor _acceptor{_context};
    std::uint16_t _port = 0;
    std::filesystem::path _certificateFile;

    mutable std::mutex _mutex;
    std::condition_variable _changed;
    std::vector<ReceivedMail> _mails;
    std::size_t _connections = 0;
    std::size_t _refuseNext = 0;
    bool _held = false;
    std::atomic<bool> _stopping = false;
    /// The socket of the connection being served, shut down to stop it.
    std::atomic<asio::ip::tcp::socket::native_handle_type> _active{
        asio::ip::tcp::socket::native_handle_type(-1)};
    std::thread _thread;

#ifdef RTYPE_TEST_OPENSSL
    struct SslContextDeleter {
        void operator()(SSL_CTX *context) const { SSL_CTX_free(context); }
    };
    std::unique_ptr<SSL_CTX, SslContextDeleter> _ssl;

    /// A key and a certificate of its own, for 127.0.0.1 and localhost, written as PEM for the client.
    void makeCertificate()
    {
        EVP_PKEY *key = EVP_EC_gen("P-256");
        X509 *certificate = X509_new();
        X509_set_version(certificate, 2);
        ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1);
        X509_gmtime_adj(X509_getm_notBefore(certificate), -60);
        X509_gmtime_adj(X509_getm_notAfter(certificate), 24L * 60 * 60);
        X509_set_pubkey(certificate, key);
        X509_NAME *name = X509_get_subject_name(certificate);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   reinterpret_cast<const unsigned char *>("localhost"), -1, -1, 0);
        X509_set_issuer_name(certificate, name);
        X509V3_CTX context;
        X509V3_set_ctx_nodb(&context);
        X509V3_set_ctx(&context, certificate, certificate, nullptr, nullptr, 0);
        for (const auto &[nid, value] : {std::pair{NID_subject_alt_name, "IP:127.0.0.1,DNS:localhost"},
                                         std::pair{NID_basic_constraints, "critical,CA:TRUE"}}) {
            X509_EXTENSION *extension = X509V3_EXT_conf_nid(nullptr, &context, nid, value);
            X509_add_ext(certificate, extension, -1);
            X509_EXTENSION_free(extension);
        }
        X509_sign(certificate, key, EVP_sha256());

        _ssl.reset(SSL_CTX_new(TLS_server_method()));
        SSL_CTX_use_certificate(_ssl.get(), certificate);
        SSL_CTX_use_PrivateKey(_ssl.get(), key);

        _certificateFile =
            std::filesystem::temp_directory_path() / ("rtype_tests_smtp_" + std::to_string(_port) + ".pem");
        FILE *file = std::fopen(_certificateFile.string().c_str(), "wb");
        PEM_write_X509(file, certificate);
        std::fclose(file);
        X509_free(certificate);
        EVP_PKEY_free(key);
    }
#endif

    /// One connection, in the clear or over TLS once it is started.
    class Stream
    {
    private:
        asio::ip::tcp::socket &_socket;
        std::string _buffer;
#ifdef RTYPE_TEST_OPENSSL
        SSL *_tls = nullptr;
#endif

    public:
        explicit Stream(asio::ip::tcp::socket &socket) : _socket(socket) {}
        ~Stream()
        {
#ifdef RTYPE_TEST_OPENSSL
            if (_tls != nullptr) {
                SSL_free(_tls);
            }
#endif
        }
        Stream(const Stream &) = delete;
        Stream &operator=(const Stream &) = delete;

        [[nodiscard]] bool secured() const
        {
#ifdef RTYPE_TEST_OPENSSL
            return _tls != nullptr;
#else
            return false;
#endif
        }

#ifdef RTYPE_TEST_OPENSSL
        bool startTls(SSL_CTX *context)
        {
            _buffer.clear();
            _tls = SSL_new(context);
            SSL_set_fd(_tls, static_cast<int>(_socket.native_handle()));
            return SSL_accept(_tls) == 1;
        }
#endif

        void write(std::string_view text)
        {
#ifdef RTYPE_TEST_OPENSSL
            if (_tls != nullptr) {
                SSL_write(_tls, text.data(), static_cast<int>(text.size()));
                return;
            }
#endif
            asio::error_code error;
            asio::write(_socket, asio::buffer(text.data(), text.size()), error);
        }

        void reply(std::string_view line) { write(std::string(line) + "\r\n"); }

        /// A line without its CRLF; nothing once the connection ends.
        std::optional<std::string> readLine()
        {
            while (true) {
                const std::size_t end = _buffer.find("\r\n");
                if (end != std::string::npos) {
                    std::string line = _buffer.substr(0, end);
                    _buffer.erase(0, end + 2);
                    return line;
                }
                char chunk[4096];
                std::size_t received = 0;
#ifdef RTYPE_TEST_OPENSSL
                if (_tls != nullptr) {
                    const int count = SSL_read(_tls, chunk, sizeof(chunk));
                    if (count <= 0) {
                        return std::nullopt;
                    }
                    received = static_cast<std::size_t>(count);
                } else
#endif
                {
                    asio::error_code error;
                    received = _socket.read_some(asio::buffer(chunk), error);
                    if (error) {
                        return std::nullopt;
                    }
                }
                _buffer.append(chunk, received);
            }
        }
    };

    static bool startsWith(std::string_view line, std::string_view command)
    {
        if (line.size() < command.size()) {
            return false;
        }
        for (std::size_t i = 0; i < command.size(); ++i) {
            if (std::toupper(static_cast<unsigned char>(line[i])) != command[i]) {
                return false;
            }
        }
        return true;
    }

    /// What is between the angle brackets of `MAIL FROM:<a@b.c>`.
    static std::string pathOf(std::string_view line)
    {
        const std::size_t open = line.find('<');
        const std::size_t close = line.find('>', open);
        return open == std::string_view::npos || close == std::string_view::npos
                   ? std::string{}
                   : std::string(line.substr(open + 1, close - open - 1));
    }

    bool authenticate(Stream &stream, std::string_view line)
    {
        const std::string mechanism = line.size() > 5 ? std::string(line.substr(5)) : std::string{};
        std::string username;
        std::string password;
        if (startsWith(mechanism, "PLAIN")) {
            std::string response = mechanism.size() > 6 ? mechanism.substr(6) : std::string{};
            if (response.empty()) {
                stream.reply("334 ");
                response = stream.readLine().value_or("");
            }
            const std::string decoded = detail::decodeBase64(response);
            const std::size_t first = decoded.find('\0');
            const std::size_t second = decoded.find('\0', first + 1);
            if (first == std::string::npos || second == std::string::npos) {
                return false;
            }
            username = decoded.substr(first + 1, second - first - 1);
            password = decoded.substr(second + 1);
        } else if (startsWith(mechanism, "LOGIN")) {
            stream.reply("334 VXNlcm5hbWU6");
            username = detail::decodeBase64(stream.readLine().value_or(""));
            stream.reply("334 UGFzc3dvcmQ6");
            password = detail::decodeBase64(stream.readLine().value_or(""));
        } else {
            return false;
        }
        return username == _settings.username && password == _settings.password;
    }

    void serve(asio::ip::tcp::socket &socket)
    {
        Stream stream(socket);
#ifdef RTYPE_TEST_OPENSSL
        if (_settings.tls == SmtpTls::Implicit && !stream.startTls(_ssl.get())) {
            return;
        }
#endif
        stream.reply("220 fake.test ESMTP ready");
        bool signedIn = _settings.username.empty();
        std::optional<ReceivedMail> mail;
        while (const auto line = stream.readLine()) {
            if (startsWith(*line, "EHLO") || startsWith(*line, "HELO")) {
                std::vector<std::string> features = {"fake.test", "8BITMIME"};
                if (_settings.tls == SmtpTls::StartTls && !stream.secured()) {
                    features.emplace_back("STARTTLS");
                }
                if (!_settings.username.empty() && (_settings.tls == SmtpTls::None || stream.secured())) {
                    features.emplace_back("AUTH PLAIN LOGIN");
                }
                for (std::size_t i = 0; i < features.size(); ++i) {
                    stream.reply((i + 1 == features.size() ? "250 " : "250-") + features[i]);
                }
            } else if (startsWith(*line, "STARTTLS")) {
#ifdef RTYPE_TEST_OPENSSL
                if (_settings.tls == SmtpTls::StartTls && !stream.secured()) {
                    stream.reply("220 Go ahead");
                    if (!stream.startTls(_ssl.get())) {
                        return;
                    }
                    continue;
                }
#endif
                stream.reply("502 No TLS here");
            } else if (startsWith(*line, "AUTH")) {
                if (_settings.tls == SmtpTls::StartTls && !stream.secured()) {
                    stream.reply("538 Encryption required");
                } else if (authenticate(stream, *line)) {
                    signedIn = true;
                    stream.reply("235 Authenticated");
                } else {
                    stream.reply("535 Authentication credentials invalid");
                }
            } else if (startsWith(*line, "NOOP")) {
                stream.reply("250 OK");
            } else if (startsWith(*line, "RSET")) {
                mail.reset();
                stream.reply("250 OK");
            } else if (startsWith(*line, "MAIL FROM:")) {
                bool refused = false;
                {
                    const std::lock_guard lock(_mutex);
                    if (_refuseNext > 0) {
                        --_refuseNext;
                        refused = true;
                    }
                }
                if (!signedIn) {
                    stream.reply("530 Authentication required");
                } else if (refused) {
                    stream.reply("451 Try again later");
                } else {
                    mail = ReceivedMail{.from = pathOf(*line), .to = {}, .data = {}};
                    stream.reply("250 OK");
                }
            } else if (startsWith(*line, "RCPT TO:")) {
                if (!mail) {
                    stream.reply("503 MAIL first");
                } else {
                    mail->to.push_back(pathOf(*line));
                    stream.reply("250 OK");
                }
            } else if (startsWith(*line, "DATA")) {
                if (!mail || mail->to.empty()) {
                    stream.reply("503 RCPT first");
                    continue;
                }
                stream.reply("354 End data with <CR><LF>.<CR><LF>");
                while (const auto data = stream.readLine()) {
                    if (*data == ".") {
                        break;
                    }
                    mail->data += (data->starts_with('.') ? data->substr(1) : *data) + "\r\n";
                }
                {
                    const std::lock_guard lock(_mutex);
                    _mails.push_back(std::move(*mail));
                }
                _changed.notify_all();
                mail.reset();
                stream.reply("250 OK queued");
            } else if (startsWith(*line, "QUIT")) {
                stream.reply("221 Bye");
                return;
            } else {
                stream.reply("502 Command not implemented");
            }
        }
    }

    void run()
    {
        while (!_stopping) {
            asio::ip::tcp::socket socket(_context);
            asio::error_code error;
            _acceptor.accept(socket, error);
            if (_stopping || error) {
                return;
            }
            // Set before _stopping is read again: stop() sees it, or this sees _stopping
            _active = socket.native_handle();
            {
                std::unique_lock lock(_mutex);
                ++_connections;
                _changed.notify_all();
                _changed.wait(lock, [this] { return !_held || _stopping; });
            }
            if (_stopping) {
                return;
            }
            serve(socket);
            _active = asio::ip::tcp::socket::native_handle_type(-1);
        }
    }

public:
    explicit SmtpServer(SmtpSettings settings = {}) : _settings(std::move(settings))
    {
#ifndef _WIN32
        // OpenSSL writes to the socket with write(): a client gone would stop the tests with SIGPIPE
        std::signal(SIGPIPE, SIG_IGN);
#endif
        const asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 0);
        _acceptor.open(endpoint.protocol());
        _acceptor.bind(endpoint);
        _acceptor.listen();
        _port = _acceptor.local_endpoint().port();
        if (_settings.tls != SmtpTls::None) {
#ifdef RTYPE_TEST_OPENSSL
            makeCertificate();
#else
            throw std::logic_error("the fake mail server has no TLS in this build");
#endif
        }
        _thread = std::thread([this] { run(); });
    }

    ~SmtpServer() { stop(); }

    SmtpServer(const SmtpServer &) = delete;
    SmtpServer &operator=(const SmtpServer &) = delete;

    /// Closes the server and the connection it serves: the mail server is gone.
    void stop()
    {
        if (_stopping.exchange(true)) {
            return;
        }
        {
            const std::lock_guard lock(_mutex);
            _held = false;
        }
        _changed.notify_all();
        // Unblocks the read of the connection served, then the accept
        const auto active = _active.load();
        if (active != asio::ip::tcp::socket::native_handle_type(-1)) {
            ::shutdown(active, 2);
        }
        {
            asio::io_context context;
            asio::ip::tcp::socket wake(context);
            asio::error_code error;
            wake.connect({asio::ip::make_address("127.0.0.1"), _port}, error);
        }
        _thread.join();
        if (!_certificateFile.empty()) {
            std::error_code error;
            std::filesystem::remove(_certificateFile, error);
        }
    }

    [[nodiscard]] std::uint16_t port() const { return _port; }
    /// The certificate the client must trust, with TLS.
    [[nodiscard]] const std::filesystem::path &certificate() const { return _certificateFile; }

    /// Answers the next `count` MAIL FROM with 451, a temporary failure.
    void refuseNext(std::size_t count)
    {
        const std::lock_guard lock(_mutex);
        _refuseNext = count;
    }

    /// Keeps the next connections waiting before their greeting, until release().
    void hold()
    {
        const std::lock_guard lock(_mutex);
        _held = true;
    }
    void release()
    {
        {
            const std::lock_guard lock(_mutex);
            _held = false;
        }
        _changed.notify_all();
    }

    /// The connections accepted so far, held ones included.
    [[nodiscard]] std::size_t connections() const
    {
        const std::lock_guard lock(_mutex);
        return _connections;
    }

    [[nodiscard]] std::vector<ReceivedMail> mails() const
    {
        const std::lock_guard lock(_mutex);
        return _mails;
    }

    /// Waits until `count` mails are received, or `timeout`.
    [[nodiscard]] std::vector<ReceivedMail>
    waitForMails(std::size_t count, std::chrono::milliseconds timeout = std::chrono::seconds(10))
    {
        std::unique_lock lock(_mutex);
        _changed.wait_for(lock, timeout, [&] { return _mails.size() >= count; });
        return _mails;
    }

    /// Waits until `count` connections are accepted, or `timeout`.
    bool waitForConnections(std::size_t count, std::chrono::milliseconds timeout = std::chrono::seconds(10))
    {
        std::unique_lock lock(_mutex);
        return _changed.wait_for(lock, timeout, [&] { return _connections >= count; });
    }
};

/// The parts of a mail: its headers, and the decoded body of each of its MIME parts by type.
struct ParsedMail {
    std::string headers;
    std::string text;
    std::string html;

    [[nodiscard]] std::string header(std::string_view name) const
    {
        const std::string prefix = std::string(name) + ": ";
        const std::size_t start = headers.starts_with(prefix) ? 0 : headers.find("\r\n" + prefix);
        if (start == std::string::npos) {
            return {};
        }
        const std::size_t from = start == 0 ? prefix.size() : start + 2 + prefix.size();
        std::size_t end = headers.find("\r\n", from);
        // Folded lines go on with a space
        while (end != std::string::npos && end + 2 < headers.size() && headers[end + 2] == ' ') {
            end = headers.find("\r\n", end + 2);
        }
        return headers.substr(from, end - from);
    }
};

/// Splits a mail of formatMail(): its headers, then its two parts, decoded.
inline ParsedMail parseMail(std::string_view data)
{
    ParsedMail parsed;
    const std::size_t end = data.find("\r\n\r\n");
    parsed.headers = std::string(data.substr(0, end));
    std::size_t at = data.find("Content-Type: text/", end);
    while (at != std::string_view::npos) {
        const bool html = data.substr(at, 23) == "Content-Type: text/html";
        const std::size_t body = data.find("\r\n\r\n", at) + 4;
        const std::size_t next = data.find("\r\n--", body);
        (html ? parsed.html : parsed.text) = decodeBase64(data.substr(body, next - body));
        at = data.find("Content-Type: text/", next);
    }
    return parsed;
}

} // namespace testing
