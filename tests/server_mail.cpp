// Tests of the mails of the master (docs/architecture/accounts.md#the-mails): what they say in English and in
// French, how they are written for SMTP, and CurlMailer against a fake mail server: in the clear, with
// STARTTLS or TLS, with an account, refused, slow or gone; its queue, its retries, its limit per address; the
// check at start.

#include "Accounts/CurlMailer.hpp"
#include "Accounts/MailMessages.hpp"
#include "Accounts/Mailer.hpp"
#include "Common/Base64.hpp"
#include "Common/Options.hpp"
#include "Common/Settings.hpp"
#include "Server.hpp"
#include "SmtpServer.hpp"
#include "rtype/log/Log.hpp"
#include "rtype/log/Logger.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace rtype::server;
using namespace std::chrono_literals;
using namespace std::string_view_literals;
using Catch::Matchers::ContainsSubstring;
using testing::SmtpServer;
using testing::SmtpSettings;
using testing::SmtpTls;

namespace
{

/// Captures the messages logged during a test, from every thread, at every level.
class Capture : public rtype::log::Sink
{
private:
    mutable std::mutex _mutex;
    std::string _text;

public:
    void write(const rtype::log::Record &record) override
    {
        const std::lock_guard lock(_mutex);
        _text.append(record.message).append("\n");
    }

    [[nodiscard]] std::string text() const
    {
        const std::lock_guard lock(_mutex);
        return _text;
    }
};

struct Captured {
    std::shared_ptr<Capture> sink = std::make_shared<Capture>();
    rtype::log::Level previous = rtype::log::level();

    Captured()
    {
        rtype::log::setSink(sink);
        rtype::log::setLevel(rtype::log::Level::Trace);
    }
    ~Captured()
    {
        rtype::log::setLevel(previous);
        rtype::log::setSink(nullptr);
    }
    Captured(const Captured &) = delete;
    Captured &operator=(const Captured &) = delete;
};

/// A mailer of the fake server: quick to retry, quick to give up.
CurlMailerSettings settingsFor(const SmtpServer &server, MailSecurity security = MailSecurity::None)
{
    CurlMailerSettings settings;
    settings.mail.host = "127.0.0.1";
    settings.mail.port = server.port();
    settings.mail.security = security;
    settings.mail.from = "R-Type <rtype@example.org>";
    settings.retryDelay = 50ms;
    settings.connectTimeout = 2s;
    settings.timeout = 5s;
    settings.caFile = server.certificate();
    return settings;
}

Mail codeMail(std::string to, MailLanguage language = MailLanguage::English)
{
    return {.to = std::move(to), .content = verificationMail(language, "pavel", "482915", 15min)};
}

/// Waits until the mailer has nothing left to send.
void drain(const CurlMailer &mailer, std::chrono::milliseconds timeout = 10s)
{
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (mailer.pending() > 0 && std::chrono::steady_clock::now() < end) {
        std::this_thread::sleep_for(10ms);
    }
    REQUIRE(mailer.pending() == 0);
}

/// A port nothing listens on: the one of a server stopped.
std::uint16_t closedPort()
{
    SmtpServer server;
    const std::uint16_t port = server.port();
    server.stop();
    return port;
}

/// A folder of its own for each test, removed after it.
struct TempDirectory {
    std::filesystem::path path;

    explicit TempDirectory(std::string_view name)
        : path(std::filesystem::temp_directory_path() / ("rtype_tests_mail_" + std::string(name)))
    {
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }
    ~TempDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    TempDirectory(const TempDirectory &) = delete;
    TempDirectory &operator=(const TempDirectory &) = delete;

    std::filesystem::path write(const std::string &name, std::string_view content) const
    {
        const std::filesystem::path file = path / name;
        std::ofstream(file, std::ios::binary) << content;
        return file;
    }
};

/// A master with accounts, checked against a mail server on `port`.
int checkMaster(const TempDirectory &directory, std::uint16_t port)
{
    const std::string file = directory
                                 .write("server.toml", std::format(R"(
[server]
port = 0
game_port = 0
key = "server.key"
database = "sqlite:rtype.db"
[web]
listen = "127.0.0.1:0"
[accounts]
[mail]
host = "127.0.0.1"
port = {}
security = "none"
from = "R-Type <rtype@example.org>"
)",
                                                                   port))
                                 .string();
    std::ostringstream out;
    std::istringstream in;
    const std::vector<std::string_view> arguments = {"--settings", file, "--check", "--config",
                                                     RTYPE_TEST_CONFIG};
    return rtype::server::run(arguments, out, in);
}

} // namespace

// --- What the mails say ---

TEST_CASE("The language of a mail follows the locale of the player", "[server][mail]")
{
    for (const std::string_view locale : {"fr", "FR", "fr-FR", "fr_CA"}) {
        CHECK(mailLanguage(locale) == MailLanguage::French);
    }
    for (const std::string_view locale : {"", "en", "en-US", "f", "fra", "frisian", "de"}) {
        CHECK(mailLanguage(locale) == MailLanguage::English);
    }
}

TEST_CASE("A code mail gives the code large, its expiry, and no link, in English and French",
          "[server][mail]")
{
    for (const MailLanguage language : {MailLanguage::English, MailLanguage::French}) {
        for (const MailContent &mail : {verificationMail(language, "pavel", "482915", 15min),
                                        resetMail(language, "pavel", "482915", 15min)}) {
            CHECK_THAT(mail.text, ContainsSubstring("pavel"));
            CHECK_THAT(mail.text, ContainsSubstring("\n    482915\n"));
            CHECK_THAT(mail.text, ContainsSubstring("15 minutes"));
            CHECK_THAT(mail.html, ContainsSubstring("font-size:40px"));
            CHECK_THAT(mail.html, ContainsSubstring(">482915</p>"));
            CHECK_THAT(mail.html, ContainsSubstring("15 minutes"));
            for (const std::string &body : {mail.text, mail.html}) {
                CHECK(body.find("http") == std::string::npos);
                CHECK(body.find("href") == std::string::npos);
            }
        }
    }
    CHECK(verificationMail(MailLanguage::English, "pavel", "1", 15min).subject ==
          "Your R-Type verification code");
    CHECK(verificationMail(MailLanguage::French, "pavel", "1", 15min).subject ==
          "Votre code de vérification R-Type");
    CHECK_THAT(verificationMail(MailLanguage::French, "pavel", "1", 15min).text,
               ContainsSubstring("Bonjour pavel,"));
    CHECK_THAT(verificationMail(MailLanguage::French, "pavel", "1", 15min).html,
               ContainsSubstring("lang=\"fr\""));
    CHECK_THAT(resetMail(MailLanguage::English, "pavel", "1", 1min).text, ContainsSubstring("in 1 minute."));
    CHECK(verificationMail(MailLanguage::English, "pavel", "1", 15min).subject !=
          resetMail(MailLanguage::English, "pavel", "1", 15min).subject);
}

TEST_CASE("The mails without a code say why they come, in both languages", "[server][mail]")
{
    for (const MailLanguage language : {MailLanguage::English, MailLanguage::French}) {
        const MailContent existing = existingAccountMail(language, "pavel");
        const MailContent limit = tooManyCodesMail(language, "pavel");
        for (const MailContent &mail : {existing, limit}) {
            CHECK_THAT(mail.text, ContainsSubstring("pavel"));
            CHECK(mail.html.find("font-size:40px") == std::string::npos);
        }
    }
    CHECK(existingAccountMail(MailLanguage::English, "pavel").subject ==
          "You already have an R-Type account");
    CHECK(existingAccountMail(MailLanguage::French, "pavel").subject == "Vous avez déjà un compte R-Type");
    CHECK_THAT(tooManyCodesMail(MailLanguage::English, "pavel").text, ContainsSubstring("midnight UTC"));
    CHECK_THAT(tooManyCodesMail(MailLanguage::French, "pavel").text, ContainsSubstring("minuit UTC"));
}

TEST_CASE("The HTML of a mail escapes what it is given", "[server][mail]")
{
    const MailContent mail = verificationMail(MailLanguage::English, "<b>&\"'", "1", 15min);
    CHECK_THAT(mail.html, ContainsSubstring("Hello &lt;b&gt;&amp;&quot;&#39;,"));
    CHECK(mail.html.find("<b>") == std::string::npos);
}

// --- How they are written ---

TEST_CASE("base64 follows RFC 4648", "[server][mail]")
{
    CHECK(base64("").empty());
    CHECK(base64("f") == "Zg==");
    CHECK(base64("fo") == "Zm8=");
    CHECK(base64("foo") == "Zm9v");
    CHECK(base64("foob") == "Zm9vYg==");
    CHECK(base64("fooba") == "Zm9vYmE=");
    CHECK(base64("foobar") == "Zm9vYmFy");
    CHECK(base64("\xff\xfe\x00"sv) == "//4A");
}

TEST_CASE("base64 decodes what it encodes, and refuses what is not base64", "[server][mail]")
{
    for (const std::string_view bytes :
         {""sv, "f"sv, "fo"sv, "foo"sv, "foob"sv, "foobar"sv, "\xff\xfe\x00"sv}) {
        CHECK(decodeBase64(base64(bytes)) == std::string(bytes));
    }
    CHECK_FALSE(decodeBase64("Zg="));
    CHECK_FALSE(decodeBase64("Z!=="));
    CHECK_FALSE(decodeBase64("Zg==Zg=="));
    CHECK_FALSE(decodeBase64("Z==="));
    CHECK_FALSE(decodeBase64("Zg=a"));
}

TEST_CASE("A mail is written for SMTP: its headers, then its text and its HTML in UTF-8", "[server][mail]")
{
    const Mail mail = codeMail("player@example.org", MailLanguage::French);
    // 2026-10-30 12:54:56 UTC
    const auto date = std::chrono::system_clock::time_point(std::chrono::seconds(1'793'364'896));
    const std::string data = formatMail(mail, "R-Type <rtype@example.org>", date, "<abc123@example.org>");

    const testing::ParsedMail parsed = testing::parseMail(data);
    CHECK(parsed.header("Date") == "Fri, 30 Oct 2026 12:54:56 +0000");
    CHECK(parsed.header("From") == "R-Type <rtype@example.org>");
    CHECK(parsed.header("To") == "player@example.org");
    CHECK(parsed.header("Message-ID") == "<abc123@example.org>");
    CHECK(parsed.header("MIME-Version") == "1.0");
    CHECK(parsed.header("Auto-Submitted") == "auto-generated");
    CHECK(parsed.header("Content-Type") == "multipart/alternative; boundary=\"=_rtype_abc123exampleorg\"");
    // Not ASCII: encoded, and read back as it was
    const std::string subject = parsed.header("Subject");
    CHECK(subject.starts_with("=?UTF-8?B?"));
    CHECK(testing::decodeBase64(subject.substr(10, subject.size() - 12)) == mail.content.subject);
    // Each part decoded back, its line ends made CRLF
    CHECK(parsed.text.find("Bonjour pavel,\r\n") == 0);
    CHECK_THAT(parsed.text, ContainsSubstring("482915"));
    CHECK_THAT(parsed.html, ContainsSubstring(">482915</p>"));
    CHECK_THAT(data, ContainsSubstring(
                         "Content-Type: text/plain; charset=utf-8\r\nContent-Transfer-Encoding: base64"));
    CHECK_THAT(data, ContainsSubstring(
                         "Content-Type: text/html; charset=utf-8\r\nContent-Transfer-Encoding: base64"));
    CHECK(data.ends_with("--=_rtype_abc123exampleorg--\r\n"));

    // Every line ends with CRLF, none is longer than SMTP allows
    std::size_t start = 0;
    while (start < data.size()) {
        const std::size_t end = data.find('\n', start);
        REQUIRE(end != std::string::npos);
        CHECK(end > start);
        CHECK(data[end - 1] == '\r');
        CHECK(end - start - 1 <= 78);
        start = end + 1;
    }
    // ASCII, every byte
    CHECK(std::ranges::all_of(data, [](char c) { return static_cast<unsigned char>(c) < 0x80; }));
}

TEST_CASE("A long subject is folded into encoded words of whole characters", "[server][mail]")
{
    Mail mail = codeMail("player@example.org");
    mail.content.subject.clear();
    for (int i = 0; i < 40; ++i) {
        mail.content.subject += "é";
    }
    const testing::ParsedMail parsed =
        testing::parseMail(formatMail(mail, "rtype@example.org", {}, "<id@example.org>"));
    std::string decoded;
    const std::string header = parsed.header("Subject");
    std::string_view subject = header;
    std::size_t words = 0;
    while (!subject.empty()) {
        const std::size_t end = subject.find("?=");
        REQUIRE(subject.starts_with("=?UTF-8?B?"));
        CHECK(end + 2 <= 75);
        const std::string word = testing::decodeBase64(subject.substr(10, end - 10));
        // Each word holds whole characters: é is 2 bytes
        CHECK(word.size() % 2 == 0);
        decoded += word;
        subject.remove_prefix(end + 2);
        if (subject.starts_with("\r\n ")) {
            subject.remove_prefix(3);
        }
        ++words;
    }
    CHECK(words > 1);
    CHECK(decoded == mail.content.subject);
}

TEST_CASE("The name of the sender is quoted or encoded as it needs", "[server][mail]")
{
    const Mail mail = codeMail("player@example.org");
    const auto from = [&](std::string_view sender) {
        return testing::parseMail(formatMail(mail, sender, {}, "<id@example.org>")).header("From");
    };
    CHECK(from("rtype@example.org") == "rtype@example.org");
    CHECK(from("<rtype@example.org>") == "rtype@example.org");
    CHECK(from("  R-Type   <rtype@example.org>") == "R-Type <rtype@example.org>");
    CHECK(from("R-Type, Official <rtype@example.org>") == "\"R-Type, Official\" <rtype@example.org>");
    CHECK(from("Say \"hi\". <rtype@example.org>") == "\"Say \\\"hi\\\".\" <rtype@example.org>");
    CHECK(from("Équipe R-Type <rtype@example.org>") ==
          "=?UTF-8?B?w4lxdWlwZSBSLVR5cGU=?= <rtype@example.org>");
}

// --- CurlMailer ---

TEST_CASE("A code mail reaches the mail server, in English and in French", "[server][mail]")
{
    SmtpServer server;
    CurlMailer mailer(settingsFor(server));

    CHECK(mailer.send(codeMail("player@example.org", MailLanguage::English)) == MailQueued::Yes);
    CHECK(mailer.send(codeMail("joueur@example.org", MailLanguage::French)) == MailQueued::Yes);
    const auto mails = server.waitForMails(2);
    REQUIRE(mails.size() == 2);
    drain(mailer);
    CHECK(mailer.counts().sent == 2);
    CHECK(mailer.counts().failed == 0);

    CHECK(mails[0].from == "rtype@example.org");
    CHECK(mails[0].to == std::vector<std::string>{"player@example.org"});
    const testing::ParsedMail english = testing::parseMail(mails[0].data);
    CHECK(english.header("From") == "R-Type <rtype@example.org>");
    CHECK(english.header("To") == "player@example.org");
    CHECK(english.header("Subject") == "Your R-Type verification code");
    CHECK(english.header("Message-ID").ends_with("@example.org>"));
    CHECK_THAT(english.text, ContainsSubstring("Hello pavel,"));
    CHECK_THAT(english.text, ContainsSubstring("482915"));
    CHECK_THAT(english.html, ContainsSubstring(">482915</p>"));

    CHECK(mails[1].to == std::vector<std::string>{"joueur@example.org"});
    const testing::ParsedMail french = testing::parseMail(mails[1].data);
    CHECK_THAT(french.text, ContainsSubstring("Bonjour pavel,"));
    CHECK_THAT(french.text, ContainsSubstring("482915"));
    CHECK_THAT(french.text, ContainsSubstring("Il expire dans 15 minutes."));
    // Each mail its own id
    CHECK(english.header("Message-ID") != french.header("Message-ID"));
}

TEST_CASE("The mailer signs in to the mail server, and logs neither the password nor the code",
          "[server][mail]")
{
    const Captured captured;
    SmtpServer server(SmtpSettings{.tls = SmtpTls::None, .username = "rtype", .password = "s3cret-password"});
    CurlMailerSettings settings = settingsFor(server);
    settings.mail.username = "rtype";

    SECTION("with its account")
    {
        settings.mail.password = "s3cret-password";
        CHECK_FALSE(checkMailServer(settings));
        CurlMailer mailer(settings);
        CHECK(mailer.send(codeMail("player@example.org")) == MailQueued::Yes);
        CHECK(server.waitForMails(1).size() == 1);
        drain(mailer);
        CHECK(mailer.counts().sent == 1);
    }
    SECTION("refused with a wrong password")
    {
        settings.mail.password = "wrong-password";
        const auto problem = checkMailServer(settings);
        REQUIRE(problem);
        CHECK_THAT(*problem, ContainsSubstring("ogin denied"));
        settings.attempts = 2;
        CurlMailer mailer(settings);
        CHECK(mailer.send(codeMail("player@example.org")) == MailQueued::Yes);
        drain(mailer);
        CHECK(mailer.counts().failed == 1);
        CHECK(server.mails().empty());
        CHECK_THAT(captured.sink->text(), ContainsSubstring("given up after 2 tries"));
        CHECK(captured.sink->text().find("wrong-password") == std::string::npos);
    }
    const std::string logs = captured.sink->text();
    CHECK(logs.find("s3cret-password") == std::string::npos);
    CHECK(logs.find("482915") == std::string::npos);
    CHECK(logs.find("player@example.org") == std::string::npos);
}

TEST_CASE("The check of the mail server sends nothing", "[server][mail]")
{
    SmtpServer server;
    CHECK_FALSE(checkMailServer(settingsFor(server)));
    CHECK(server.connections() == 1);
    CHECK(server.mails().empty());
}

TEST_CASE("A mail server gone fails the check, and its mails after their tries", "[server][mail]")
{
    const Captured captured;
    SmtpServer server;
    CurlMailerSettings settings = settingsFor(server);
    settings.mail.port = closedPort();
    settings.attempts = 3;

    const auto problem = checkMailServer(settings);
    REQUIRE(problem);
    CHECK_THAT(*problem, ContainsSubstring("onnect"));

    CurlMailer mailer(settings);
    CHECK(mailer.send(codeMail("player@example.org")) == MailQueued::Yes);
    drain(mailer);
    CHECK(mailer.counts().failed == 1);
    CHECK(mailer.counts().sent == 0);
    const std::string logs = captured.sink->text();
    CHECK_THAT(logs, ContainsSubstring("try 1 of 3"));
    CHECK_THAT(logs, ContainsSubstring("try 2 of 3"));
    CHECK_THAT(logs, ContainsSubstring("given up after 3 tries"));
}

TEST_CASE("A mail the server refuses for now is tried again, later", "[server][mail]")
{
    SmtpServer server;
    server.refuseNext(2);
    CurlMailerSettings settings = settingsFor(server);
    settings.attempts = 3;
    CurlMailer mailer(settings);

    const auto start = std::chrono::steady_clock::now();
    CHECK(mailer.send(codeMail("player@example.org")) == MailQueued::Yes);
    REQUIRE(server.waitForMails(1).size() == 1);
    // 50 ms, then 100 ms
    CHECK(std::chrono::steady_clock::now() - start >= 150ms);
    drain(mailer);
    CHECK(mailer.counts().sent == 1);
    CHECK(server.connections() == 3);
}

TEST_CASE("A mail waiting to be tried again holds back none of the others", "[server][mail]")
{
    SmtpServer server;
    server.refuseNext(1);
    CurlMailerSettings settings = settingsFor(server);
    settings.retryDelay = 2s;
    CurlMailer mailer(settings);

    CHECK(mailer.send(codeMail("first@example.org")) == MailQueued::Yes);
    REQUIRE(server.waitForConnections(1));
    CHECK(mailer.send(codeMail("second@example.org")) == MailQueued::Yes);
    const auto mails = server.waitForMails(1, 1500ms);
    REQUIRE(mails.size() == 1);
    CHECK(mails[0].to == std::vector<std::string>{"second@example.org"});
    // The first one still waits for its second try
    const auto end = std::chrono::steady_clock::now() + 1s;
    while (mailer.counts().sent == 0 && std::chrono::steady_clock::now() < end) {
        std::this_thread::sleep_for(10ms);
    }
    CHECK(mailer.counts().sent == 1);
    CHECK(mailer.pending() == 1);
}

TEST_CASE("A full queue refuses a mail at once", "[server][mail]")
{
    SmtpServer server;
    server.hold();
    CurlMailerSettings settings = settingsFor(server);
    settings.maxPending = 2;
    CurlMailer mailer(settings);

    CHECK(mailer.send(codeMail("a@example.org")) == MailQueued::Yes);
    // The first one is being sent, waiting for its greeting
    REQUIRE(server.waitForConnections(1));
    CHECK(mailer.send(codeMail("b@example.org")) == MailQueued::Yes);
    CHECK(mailer.pending() == 2);
    const auto start = std::chrono::steady_clock::now();
    CHECK(mailer.send(codeMail("c@example.org")) == MailQueued::Busy);
    CHECK(std::chrono::steady_clock::now() - start < 100ms);

    server.release();
    CHECK(server.waitForMails(2).size() == 2);
    drain(mailer);
    CHECK(mailer.send(codeMail("c@example.org")) == MailQueued::Yes);
    CHECK(server.waitForMails(3).size() == 3);
}

TEST_CASE("An address gets a few mails an hour, ignoring case", "[server][mail]")
{
    SmtpServer server;
    CurlMailerSettings settings = settingsFor(server);
    settings.perAddress = 2;
    settings.addressWindow = 500ms;
    CurlMailer mailer(settings);

    CHECK(mailer.send(codeMail("player@example.org")) == MailQueued::Yes);
    CHECK(mailer.send(codeMail("Player@Example.org")) == MailQueued::Yes);
    CHECK(mailer.send(codeMail("PLAYER@example.org")) == MailQueued::AddressLimit);
    // Another address is not held back
    CHECK(mailer.send(codeMail("other@example.org")) == MailQueued::Yes);
    CHECK(server.waitForMails(3).size() == 3);

    // Once the window has passed
    std::this_thread::sleep_for(600ms);
    CHECK(mailer.send(codeMail("player@example.org")) == MailQueued::Yes);
    CHECK(server.waitForMails(4).size() == 4);
}

TEST_CASE("A mail to something that is not one address is never sent", "[server][mail]")
{
    SmtpServer server;
    CurlMailer mailer(settingsFor(server));
    for (const std::string_view to :
         {"", "player", "player@example.org\r\nBcc: other@example.org", "Player <player@example.org>",
          "a@b.c, d@e.f", "player@example.org\n"}) {
        CHECK(mailer.send(codeMail(std::string(to))) == MailQueued::InvalidAddress);
    }
    CHECK(mailer.pending() == 0);
    CHECK(server.connections() == 0);
}

TEST_CASE("Destroying the mailer cuts the mail being sent", "[server][mail]")
{
    SmtpServer server;
    server.hold();
    const auto start = std::chrono::steady_clock::now();
    {
        CurlMailerSettings settings = settingsFor(server);
        settings.timeout = 30s;
        CurlMailer mailer(settings);
        CHECK(mailer.send(codeMail("a@example.org")) == MailQueued::Yes);
        CHECK(mailer.send(codeMail("b@example.org")) == MailQueued::Yes);
        REQUIRE(server.waitForConnections(1));
    }
    CHECK(std::chrono::steady_clock::now() - start < 5s);
    server.release();
    CHECK(server.mails().empty());
}

TEST_CASE("The mailer sends over STARTTLS, and only over it when asked", "[server][mail]")
{
    if (!testing::SmtpTlsAvailable) {
        SKIP("the fake mail server has no TLS in this build");
    }
    SmtpServer server(
        SmtpSettings{.tls = SmtpTls::StartTls, .username = "rtype", .password = "s3cret-password"});
    CurlMailerSettings settings = settingsFor(server, MailSecurity::StartTls);
    settings.mail.username = "rtype";
    settings.mail.password = "s3cret-password";

    CHECK_FALSE(checkMailServer(settings));
    {
        CurlMailer mailer(settings);
        CHECK(mailer.send(codeMail("player@example.org")) == MailQueued::Yes);
        REQUIRE(server.waitForMails(1).size() == 1);
        CHECK_THAT(testing::parseMail(server.mails()[0].data).text, ContainsSubstring("482915"));
    }

    // A certificate the system does not trust
    CurlMailerSettings untrusted = settings;
    untrusted.caFile.clear();
    CHECK(checkMailServer(untrusted));

    // A server that offers no STARTTLS is never used in the clear
    SmtpServer plain;
    CurlMailerSettings clear = settingsFor(plain, MailSecurity::StartTls);
    const auto problem = checkMailServer(clear);
    REQUIRE(problem);
    CHECK(plain.mails().empty());
}

TEST_CASE("The mailer sends over TLS from the start", "[server][mail]")
{
    if (!testing::SmtpTlsAvailable) {
        SKIP("the fake mail server has no TLS in this build");
    }
    SmtpServer server(
        SmtpSettings{.tls = SmtpTls::Implicit, .username = "rtype", .password = "s3cret-password"});
    CurlMailerSettings settings = settingsFor(server, MailSecurity::Tls);
    settings.mail.username = "rtype";
    settings.mail.password = "s3cret-password";

    CHECK_FALSE(checkMailServer(settings));
    CurlMailer mailer(settings);
    CHECK(mailer.send(codeMail("player@example.org", MailLanguage::French)) == MailQueued::Yes);
    REQUIRE(server.waitForMails(1).size() == 1);
    CHECK_THAT(testing::parseMail(server.mails()[0].data).text, ContainsSubstring("Bonjour pavel,"));
}

// --- The master ---

TEST_CASE("A master with accounts checks its mail server at start", "[server][mail]")
{
    const Captured captured;
    const TempDirectory directory("master");

    SmtpServer server;
    CHECK(checkMaster(directory, server.port()) == Success);
    CHECK(server.connections() == 1);
    CHECK(server.mails().empty());
    CHECK_THAT(captured.sink->text(), ContainsSubstring("Mail server 127.0.0.1:"));
    CHECK_THAT(captured.sink->text(), ContainsSubstring("in the clear"));

    CHECK(checkMaster(directory, closedPort()) == Failure);
    CHECK_THAT(captured.sink->text(), ContainsSubstring("Cannot use the mail server 127.0.0.1:"));
}

// --- By hand ---

// Sends a code mail in each language through a real mail server, the one of a settings file:
//   RTYPE_TEST_MAIL_SETTINGS=server.toml RTYPE_TEST_MAIL_TO=you@example.org rtype_tests "[.mail-server]"
TEST_CASE("A real mail server receives a code mail in each language", "[.mail-server]")
{
    const char *settingsFile = std::getenv("RTYPE_TEST_MAIL_SETTINGS");
    const char *to = std::getenv("RTYPE_TEST_MAIL_TO");
    if (settingsFile == nullptr || to == nullptr) {
        SKIP("RTYPE_TEST_MAIL_SETTINGS and RTYPE_TEST_MAIL_TO name the settings and the recipient");
    }
    const ServerSettings settings = loadSettings(settingsFile, rtype::log::Logger("tests"));
    REQUIRE(settings.mail);
    CurlMailerSettings mailer;
    mailer.mail = *settings.mail;
    REQUIRE_FALSE(checkMailServer(mailer));

    CurlMailer real(mailer);
    CHECK(real.send(codeMail(to, MailLanguage::English)) == MailQueued::Yes);
    CHECK(real.send(codeMail(to, MailLanguage::French)) == MailQueued::Yes);
    drain(real, 60s);
    CHECK(real.counts().sent == 2);
}
