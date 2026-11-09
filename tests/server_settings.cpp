// Tests of the settings file of the master, --settings (docs/guides/configure-a-server.md).

#include "Common/Options.hpp"
#include "Common/Settings.hpp"
#include "Server.hpp"
#include "SmtpServer.hpp"
#include "rtype/log/Log.hpp"
#include "rtype/log/Logger.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace rtype::server;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::Equals;
using rtype::network::Address;

namespace
{

const rtype::log::Logger Log("tests");

/// A folder of its own for each test, removed after it.
struct TempDirectory {
    std::filesystem::path path;

    explicit TempDirectory(std::string_view name)
        : path(std::filesystem::temp_directory_path() / ("rtype_tests_settings_" + std::string(name)))
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
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary) << content;
#ifndef _WIN32
        std::filesystem::permissions(file, std::filesystem::perms::owner_read |
                                               std::filesystem::perms::owner_write);
#endif
        return file;
    }
};

/// Captures the messages logged during a test.
class Capture : public rtype::log::Sink
{
public:
    std::string text;

    void write(const rtype::log::Record &record) override { text.append(record.message).append("\n"); }
};

struct Captured {
    std::shared_ptr<Capture> sink = std::make_shared<Capture>();

    Captured() { rtype::log::setSink(sink); }
    ~Captured() { rtype::log::setSink(nullptr); }
    Captured(const Captured &) = delete;
    Captured &operator=(const Captured &) = delete;
};

/// Settings with no secret file, in a folder that does not exist.
ServerSettings parse(std::string_view text)
{
    return parseSettings(text, "/srv/rtype/server.toml", Log);
}

/// The message of the error the settings are refused with.
std::string refusal(std::string_view text)
{
    try {
        (void)parse(text);
    } catch (const SettingsError &error) {
        return error.what();
    }
    FAIL("the settings were accepted: " << text);
    return {};
}

int run(std::vector<std::string_view> arguments)
{
    std::ostringstream out;
    std::istringstream in;
    return rtype::server::run(arguments, out, in);
}

/// [accounts] and the [mail] it needs, without authentication.
constexpr std::string_view Accounts = R"(
[accounts]
[mail]
host = "mail.example.org"
from = "R-Type <rtype@example.org>"
)";

} // namespace

// --- Reading ---

TEST_CASE("The example of the guide is read, every key of it", "[server][settings]")
{
    const TempDirectory directory("example");
    directory.write("secrets/database_url", "postgresql://rtype:secret@db/rtype\n");
    directory.write("secrets/mail_password", "mail password  \r\n");
    directory.write("secrets/discord_secret", "discord-secret\n");
    const std::filesystem::path file = directory.write("server.toml", R"(# server.toml
[server]
port = 4242
game_port = 4242
max_games = 20
key = "/data/server.key"
database_file = "secrets/database_url"

[web]
listen = "0.0.0.0:8080"
public_url = "https://admin.rtype.example.org/"
trusted_proxies = ["172.16.0.0/12", "::1"]
panel_dir = "admin"

[accounts]
enabled = true
guests = false

[mail]
host = "mail.example.org"
port = 587
security = "starttls"
username = "rtype@example.org"
password_file = "secrets/mail_password"
from = "R-Type <rtype@example.org>"

[discord]
client_id = "123456789012345678"
client_secret_file = "secrets/discord_secret"

[admin]
admins = ["pavel@example.org", "discord:123456789012345678"]
)");

    const ServerSettings settings = loadSettings(file, Log);

    CHECK(settings.port == 4242);
    CHECK(settings.gamePort == 4242);
    CHECK(settings.maxGames == 20);
    CHECK(settings.key == std::filesystem::path("/data/server.key"));
    CHECK_FALSE(settings.database);
    CHECK(settings.databaseFile == directory.path / "secrets/database_url");

    CHECK(settings.web.listen == Address::anyIpv4(8080));
    CHECK(settings.web.publicUrl == "https://admin.rtype.example.org");
    REQUIRE(settings.web.trustedProxies.size() == 2);
    CHECK(settings.web.trustedProxies[0] == Subnet{*Address::parse("172.16.0.0", 0), 12});
    CHECK(settings.web.trustedProxies[1] == Subnet{*Address::parse("::1", 0), 128});
    CHECK(settings.web.panelDir == directory.path / "admin");

    REQUIRE(settings.accounts);
    CHECK(settings.accountsEnabled());
    CHECK_FALSE(settings.accounts->guests);

    REQUIRE(settings.mail);
    CHECK(settings.mail->host == "mail.example.org");
    CHECK(settings.mail->port == 587);
    CHECK(settings.mail->security == MailSecurity::StartTls);
    CHECK(settings.mail->username == "rtype@example.org");
    CHECK(settings.mail->password == "mail password");
    CHECK(settings.mail->from == "R-Type <rtype@example.org>");

    REQUIRE(settings.discord);
    CHECK(settings.discord->clientId == "123456789012345678");
    CHECK(settings.discord->clientSecret == "discord-secret");

    CHECK(settings.admin.admins ==
          std::vector<std::string>{"pavel@example.org", "discord:123456789012345678"});
}

TEST_CASE("An empty settings file leaves every default", "[server][settings]")
{
    const ServerSettings settings = parse("");

    CHECK_FALSE(settings.port);
    CHECK_FALSE(settings.gamePort);
    CHECK_FALSE(settings.maxGames);
    CHECK_FALSE(settings.key);
    CHECK_FALSE(settings.database);
    CHECK_FALSE(settings.databaseFile);
    CHECK(settings.web.listen == Address::loopbackIpv4(8080));
    CHECK(settings.web.publicUrl.empty());
    CHECK(settings.web.trustedProxies.empty());
    CHECK(settings.web.panelDir.empty());
    CHECK_FALSE(settings.accounts);
    CHECK_FALSE(settings.accountsEnabled());
    CHECK_FALSE(settings.mail);
    CHECK_FALSE(settings.discord);
    CHECK(settings.admin.admins.empty());

    // The same as without --settings
    const Options without = parseOptions(std::vector<std::string_view>{});
    const Options with = parseOptions(std::vector<std::string_view>{}, settings);
    CHECK(with.master.listen == without.master.listen);
    CHECK(with.master.key == without.master.key);
    CHECK(with.master.database == without.master.database);
}

TEST_CASE("The sections have their defaults", "[server][settings]")
{
    const ServerSettings settings = parse(std::string(Accounts) + "[web]\nlisten = \"[::1]:0\"\n");

    CHECK(settings.web.listen == *Address::parse("::1", 0));
    CHECK(settings.accounts->enabled);
    CHECK(settings.accounts->guests);
    CHECK(settings.mail->port == 587);
    CHECK(settings.mail->security == MailSecurity::StartTls);
    CHECK(settings.mail->username.empty());
    CHECK(settings.mail->password.empty());

    CHECK(parse(std::string(Accounts) + "security = \"tls\"\n").mail->port == 465);
    CHECK(parse(std::string(Accounts) + "security = \"tls\"\nport = 2465\n").mail->port == 2465);
    CHECK(parse(std::string(Accounts) + "security = \"none\"\n").mail->security == MailSecurity::None);
    CHECK(parse(std::string(Accounts) + "security = \"none\"\n").mail->port == 25);
    CHECK(parse("[server]\ndatabase = \"postgresql://rtype@db/rtype\"\n").database ==
          rtype::database::Address{rtype::database::Engine::Postgresql, "postgresql://rtype@db/rtype"});
}

// --- Refused ---

TEST_CASE("A settings file is refused with the line and the column of the problem", "[server][settings]")
{
    CHECK_THAT(refusal("[server]\nport = 4242\nprot = 4243\n"),
               Equals("/srv/rtype/server.toml:3:1: unknown key 'prot' in [server]"));
    CHECK_THAT(refusal("[server]\n[srever]\n"), ContainsSubstring("server.toml:2:"));
    CHECK_THAT(refusal("[server]\n[srever]\n"), ContainsSubstring("unknown section [srever]"));
    CHECK_THAT(refusal("port = 4242\n"), ContainsSubstring(":1:1: unknown key 'port' outside of a section"));
    CHECK_THAT(refusal("web = 3\n"), ContainsSubstring("web must be a section"));
    CHECK_THAT(refusal("[web]\npublic_url = \"https://x\"\nlistn = \"127.0.0.1:80\"\n"),
               ContainsSubstring(":3:1: unknown key 'listn' in [web]"));
    // Not TOML
    CHECK_THAT(refusal("[server]\nport = \n"), ContainsSubstring("server.toml:2:"));
    CHECK_THAT(refusal("[server]\nport = 1\nport = 2\n"), ContainsSubstring("server.toml:3:"));
}

TEST_CASE("A value of the wrong type is refused where it is", "[server][settings]")
{
    CHECK_THAT(refusal("[server]\nport = \"4242\"\n"),
               Equals("/srv/rtype/server.toml:2:8: [server] port must be a number"));
    CHECK_THAT(refusal("[accounts]\nguests = \"yes\"\n"),
               ContainsSubstring(":2:10: [accounts] guests must be"));
    CHECK_THAT(refusal("[server]\nkey = 3\n"), ContainsSubstring("[server] key must be a string"));
    CHECK_THAT(refusal("[admin]\nadmins = \"pavel@example.org\"\n"),
               ContainsSubstring("[admin] admins must be an array of strings"));
    CHECK_THAT(refusal("[admin]\nadmins = [\"pavel@example.org\", 3]\n"),
               ContainsSubstring(":2:32: [admin] admins must be an array of strings"));
}

TEST_CASE("Invalid values are refused", "[server][settings]")
{
    CHECK_THAT(refusal("[server]\nport = 70000\n"), ContainsSubstring("[server] port must be a port"));
    CHECK_THAT(refusal("[server]\nport = -1\n"), ContainsSubstring("[server] port must be a port"));
    CHECK_THAT(refusal("[server]\ngame_port = 70000\n"),
               ContainsSubstring(":2:13: [server] game_port must be a port"));
    CHECK_THAT(refusal("[server]\ngame_port = \"4242\"\n"),
               ContainsSubstring("[server] game_port must be a number"));
    for (const std::string_view games : {"0", "-1", "1001"}) {
        CHECK_THAT(refusal(std::format("[server]\nmax_games = {}\n", games)),
                   ContainsSubstring(":2:13: [server] max_games must be from 1 to 1000"));
    }
    // The range of before the relay: refused at its line, with the keys that replace it
    CHECK_THAT(refusal("[server]\nport = 4242\ngame_ports = \"4243-4299\"\n"),
               ContainsSubstring(":3:14: [server] game_ports is gone") &&
                   ContainsSubstring("game_port (default 4242)") && ContainsSubstring("max_games"));
    // 0 lets the system choose, as --game-port 0
    CHECK(parse("[server]\ngame_port = 0\n").gamePort == 0);
    CHECK_THAT(refusal("[server]\nkey = \"\"\n"), ContainsSubstring("[server] key must not be empty"));
    CHECK_THAT(refusal("[server]\ndatabase = \"rtype.db\"\n"), ContainsSubstring("[server] database:"));
    CHECK_THAT(refusal("[server]\ndatabase = \"sqlite:a.db\"\ndatabase_file = \"url\"\n"),
               ContainsSubstring(":3:17: [server] database and database_file both give the database"));
    for (const std::string_view listen : {"127.0.0.1", "localhost:8080", "::1:8080", "127.0.0.1:99999"}) {
        CHECK_THAT(refusal(std::format("[web]\nlisten = \"{}\"\n", listen)),
                   ContainsSubstring("[web] listen must be"));
    }
    CHECK_THAT(refusal("[web]\npublic_url = \"admin.example.org\"\n"),
               ContainsSubstring("[web] public_url must start with https://"));
    for (const std::string_view proxy : {"172.16.0.0/33", "proxy", "10.0.0.0/", "::/129"}) {
        CHECK_THAT(refusal(std::format("[web]\ntrusted_proxies = [\"{}\"]\n", proxy)),
                   ContainsSubstring("[web] trusted_proxies:"));
    }
    CHECK_THAT(refusal(std::string(Accounts) + "security = \"ssl\"\n"),
               ContainsSubstring("[mail] security must be \"starttls\", \"tls\" or \"none\""));
    CHECK_THAT(refusal("[accounts]\n[mail]\nfrom = \"a@b.c\"\n"), ContainsSubstring("[mail] needs host"));
    CHECK_THAT(refusal("[accounts]\n[mail]\nhost = \"mail\"\n"), ContainsSubstring("[mail] needs from"));
    CHECK_THAT(refusal(std::string(Accounts) + "username = \"rtype\"\n"),
               ContainsSubstring("[mail] username and password_file go together"));
}

TEST_CASE("The sender of the mails holds an address", "[server][settings]")
{
    const auto from = [](std::string_view sender) {
        return std::format("[accounts]\n[mail]\nhost = \"mail\"\nfrom = \"{}\"\n", sender);
    };
    CHECK(parse(from("rtype@example.org")).mail->from == "rtype@example.org");
    CHECK(parse(from("R-Type <rtype@example.org>")).mail->from == "R-Type <rtype@example.org>");
    for (const std::string_view sender : {"R-Type", "R-Type <rtype>", "R-Type <rtype@example.org", "<>",
                                          "R-Type rtype@example.org", "a <b> <rtype@example.org>x"}) {
        CHECK_THAT(refusal(from(sender)),
                   ContainsSubstring(":4:8: [mail] from must hold the address of the sender"));
    }

    CHECK(mailboxAddress("R-Type <rtype@example.org>") == "rtype@example.org");
    CHECK(mailboxAddress("  rtype@example.org ") == "rtype@example.org");
    CHECK(mailboxAddress("R-Type < rtype@example.org >") == "rtype@example.org");
    CHECK_FALSE(mailboxAddress("rtype@example.org\r\nBcc: other@example.org"));
    CHECK_FALSE(mailboxAddress("R-Type\n<rtype@example.org>"));
    CHECK_FALSE(mailboxAddress("<rtype@example.org> <other@example.org>"));
}

TEST_CASE("Mail in the clear takes no password", "[server][settings]")
{
    const TempDirectory directory("clear");
    directory.write("mail_password", "password\n");
    const std::string clear = std::string(Accounts) + "security = \"none\"\n";
    CHECK(parse(clear).mail->username.empty());
    CHECK_THAT(refusal(clear + "username = \"rtype\"\npassword_file = \"/nowhere\"\n"),
               ContainsSubstring(":7:12: [mail] security = \"none\" would send the password in the clear"));
}

TEST_CASE("An administrator is an email or a Discord id, never a username", "[server][settings]")
{
    CHECK(parse("[admin]\nadmins = [\"pavel@example.org\", \"discord:42\"]\n").admin.admins.size() == 2);

    CHECK_THAT(refusal("[admin]\nadmins = [\"pavel@example.org\", \"pavel\"]\n"),
               Equals("/srv/rtype/server.toml:2:32: [admin] admins: \"pavel\" is neither an email nor "
                      "discord:<id>; an administrator is never named by username"));
    for (const std::string_view admin : {"discord:", "discord:abc", "discord:pavel", "@example.org", "pavel@",
                                         "pavel@localhost", "a b@c.d", "a@b@c.d", "pavel@example."}) {
        CHECK_THAT(refusal(std::format("[admin]\nadmins = [\"{}\"]\n", admin)),
                   ContainsSubstring("is neither an email nor discord:<id>"));
    }
}

// --- Rules between the sections ---

TEST_CASE("Accounts need the mail, and the mail the accounts", "[server][settings]")
{
    CHECK(parse(Accounts).accountsEnabled());
    CHECK_THAT(refusal("\n[accounts]\nguests = true\n"),
               Equals("/srv/rtype/server.toml:2:1: [accounts] needs a [mail] section, to send the codes"));
    CHECK_THAT(refusal("[mail]\nhost = \"mail\"\nfrom = \"a@b.c\"\n"),
               ContainsSubstring(":1:1: [mail] only sends the codes of the accounts"));

    // Accounts turned off keep their mail settings, and need none
    CHECK_FALSE(parse("[accounts]\nenabled = false\n").accountsEnabled());
    CHECK(parse("[accounts]\nenabled = false\n[mail]\nhost = \"mail\"\nfrom = \"a@b.c\"\n").mail);
}

TEST_CASE("Discord needs the accounts, not the public address of the panel", "[server][settings]")
{
    const TempDirectory directory("discord");
    directory.write("discord_secret", "secret\n");
    const std::string discord = "[discord]\nclient_id = \"123\"\nclient_secret_file = \"discord_secret\"\n";
    const std::string publicUrl = "[web]\npublic_url = \"https://admin.example.org\"\n";
    const auto load = [&](const std::string &text) {
        return loadSettings(directory.write("server.toml", text), Log);
    };

    CHECK(load(std::string(Accounts) + publicUrl + discord).discord->clientId == "123");
    // The players get the code of Discord on their own machine (ADR 0026): the master need not be reachable
    CHECK(load(std::string(Accounts) + discord).discord->clientId == "123");
    CHECK_THROWS_WITH(load(publicUrl + discord), ContainsSubstring("[discord] signs in to accounts"));
    CHECK_THROWS_WITH(
        load(std::string(Accounts) + publicUrl + "[discord]\nclient_secret_file = \"discord_secret\"\n"),
        ContainsSubstring("[discord] needs client_id"));
    CHECK_THROWS_WITH(load(std::string(Accounts) + publicUrl + "[discord]\nclient_id = \"123\"\n"),
                      ContainsSubstring("[discord] needs client_secret_file"));
    CHECK_THROWS_WITH(load(std::string(Accounts) + publicUrl +
                           "[discord]\nclient_id = \"rtype\"\nclient_secret_file = \"discord_secret\"\n"),
                      ContainsSubstring("[discord] client_id must be the digits of a Discord id"));

    // The social features: off unless asked
    CHECK_FALSE(load(std::string(Accounts) + discord).discord->social);
    CHECK(load(std::string(Accounts) + discord + "social = true\n").discord->social);
    CHECK_THROWS(load(std::string(Accounts) + discord + "social = \"yes\"\n"));
}

TEST_CASE("Without accounts, the panel listens on the loopback only", "[server][settings]")
{
    CHECK(parse("[web]\nlisten = \"127.0.0.1:9000\"\n").web.listen == Address::loopbackIpv4(9000));
    CHECK(parse("[web]\nlisten = \"127.0.0.2:9000\"\n").web.listen.port() == 9000);
    CHECK(parse("[web]\nlisten = \"[::1]:9000\"\n").web.listen == *Address::parse("::1", 9000));
    for (const std::string_view listen : {"0.0.0.0:8080", "192.168.1.10:8080", "[::]:8080"}) {
        CHECK_THAT(refusal(std::format("[web]\nlisten = \"{}\"\n", listen)),
                   ContainsSubstring(":2:10: [web] listen must be a loopback address"));
        CHECK_THAT(refusal(std::format("[accounts]\nenabled = false\n[web]\nlisten = \"{}\"\n", listen)),
                   ContainsSubstring("[web] listen must be a loopback address"));
    }
    CHECK(parse(std::string(Accounts) + "[web]\nlisten = \"0.0.0.0:8080\"\n").web.listen ==
          Address::anyIpv4(8080));
}

// --- Paths and secrets ---

TEST_CASE("The paths of the settings file are relative to its folder", "[server][settings]")
{
    const TempDirectory directory("paths");
    directory.write("secrets/mail", "password\n");
    const std::filesystem::path file = directory.write("conf/server.toml", R"(
[server]
key = "keys/server.key"
database = "sqlite:data/rtype.db"
[accounts]
[mail]
host = "mail"
from = "a@b.c"
username = "rtype"
password_file = "../secrets/mail"
)");

    const ServerSettings settings = loadSettings(file, Log);

    CHECK(settings.key == directory.path / "conf" / "keys/server.key");
    CHECK(settings.database->target == (directory.path / "conf" / "data/rtype.db").string());
    CHECK(settings.mail->password == "password");

    const ServerSettings absolute = parse("[server]\nkey = \"/data/server.key\"\ndatabase_file = \"url\"\n");
    CHECK(absolute.key == std::filesystem::path("/data/server.key"));
    CHECK(absolute.databaseFile == std::filesystem::path("/srv/rtype/url"));
    CHECK(parseSettings("[server]\nkey = \"k\"\n", "server.toml", Log).key == std::filesystem::path("k"));
}

TEST_CASE("A secret file is read, its line end and spaces cut, and never logged", "[server][settings]")
{
    const TempDirectory directory("secret");
    const Captured captured;

    CHECK(readSecretFile(directory.write("a", "s3cret\n"), Log) == "s3cret");
    CHECK(readSecretFile(directory.write("b", "s3cret \t\r\nsecond line\n"), Log) == "s3cret");
    CHECK(readSecretFile(directory.write("c", "s3cret"), Log) == "s3cret");
    CHECK_THROWS_WITH(readSecretFile(directory.write("empty", ""), Log),
                      ContainsSubstring("holds no secret"));
    CHECK_THROWS_WITH(readSecretFile(directory.write("blank", "  \n"), Log),
                      ContainsSubstring("holds no secret"));
    CHECK_THROWS_WITH(readSecretFile(directory.path / "missing", Log), ContainsSubstring("cannot read"));
    CHECK(captured.sink->text.find("s3cret") == std::string::npos);
}

TEST_CASE("A secret file that is missing refuses the settings, without its content", "[server][settings]")
{
    const TempDirectory directory("missing_secret");
    const std::string text =
        std::string(Accounts) + "username = \"rtype\"\npassword_file = \"mail_password\"\n";

    CHECK_THROWS_WITH(loadSettings(directory.write("server.toml", text), Log),
                      ContainsSubstring(":7:17: [mail] password_file: cannot read"));

    directory.write("mail_password", "\n");
    CHECK_THROWS_WITH(loadSettings(directory.path / "server.toml", Log),
                      ContainsSubstring("holds no secret"));
}

#ifndef _WIN32
TEST_CASE("A secret file every user can read is warned about", "[server][settings]")
{
    const TempDirectory directory("readable_secret");
    const std::filesystem::path file = directory.write("secret", "s3cret\n");
    const Captured captured;

    CHECK(readSecretFile(file, Log) == "s3cret");
    CHECK(captured.sink->text.empty());

    std::filesystem::permissions(file, std::filesystem::perms::others_read,
                                 std::filesystem::perm_options::add);
    CHECK(readSecretFile(file, Log) == "s3cret");
    CHECK_THAT(captured.sink->text, ContainsSubstring("can be read by every user of this machine"));
    CHECK(captured.sink->text.find("s3cret") == std::string::npos);
}
#endif

// --- With the command line ---

TEST_CASE("The command line wins over the settings file", "[server][settings]")
{
    const ServerSettings settings = parse(R"(
[server]
port = 5000
game_port = 5000
max_games = 10
key = "/data/server.key"
database_file = "/run/secrets/database_url"
)");

    const Options fromFile = parseOptions(std::vector<std::string_view>{}, settings);
    CHECK(fromFile.master.listen == Address::anyIpv4(5000));
    CHECK(fromFile.master.gamePort == 5000);
    CHECK(fromFile.master.maxGames == 10);
    CHECK(fromFile.master.key == "/data/server.key");
    CHECK(fromFile.master.databaseFile == "/run/secrets/database_url");

    const Options overridden = parseOptions(
        std::vector<std::string_view>{"--port", "6000", "--game-port", "6001", "--max-games", "2", "--key",
                                      "other.key", "--database", "sqlite:other.db"},
        settings);
    CHECK(overridden.master.listen == Address::anyIpv4(6000));
    CHECK(overridden.master.gamePort == 6001);
    CHECK(overridden.master.maxGames == 2);
    CHECK(overridden.master.key == "other.key");
    CHECK(overridden.master.database ==
          rtype::database::Address{rtype::database::Engine::Sqlite, "other.db"});
    CHECK(overridden.master.databaseFile.empty());

    // --bind keeps the port of the file
    CHECK(parseOptions(std::vector<std::string_view>{"--bind", "127.0.0.1"}, settings).master.listen ==
          Address::loopbackIpv4(5000));
    CHECK(
        parseOptions(std::vector<std::string_view>{"--database-file", "url"}, settings).master.databaseFile ==
        "url");
}

TEST_CASE("The option --settings is one of the master, which reads the file at start", "[server][settings]")
{
    CHECK(parseOptions(std::vector<std::string_view>{"--settings", "server.toml"}).master.settingsFile ==
          "server.toml");
    CHECK_THROWS_AS(parseOptions(std::vector<std::string_view>{"--settings"}), std::invalid_argument);
    CHECK_THROWS_AS(
        parseOptions(std::vector<std::string_view>{"--instance", "--id", "7", "--udp-port", "4251",
                                                   "--master-port", "5000", "--settings", "s.toml"}),
        std::invalid_argument);

    const TempDirectory directory("run");
    const Captured captured;
    const std::string missing = (directory.path / "missing.toml").string();
    CHECK(run({"--settings", missing, "--check"}) == Failure);
    CHECK_THAT(captured.sink->text, ContainsSubstring("Cannot read the settings file"));

    const std::string broken = directory.write("broken.toml", "[server]\nprot = 1\n").string();
    CHECK(run({"--settings", broken, "--check"}) == Failure);
    CHECK_THAT(captured.sink->text, ContainsSubstring("broken.toml:2:1: unknown key 'prot' in [server]"));
}

TEST_CASE("A master checks its settings file with --check", "[server][settings]")
{
    const TempDirectory directory("check");
    // The mail server of the accounts is checked too (tests/server_mail.cpp)
    const testing::SmtpServer mail;
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
                                                                   mail.port()))
                                 .string();

    CHECK(run({"--settings", file, "--check", "--config", RTYPE_TEST_CONFIG}) == Success);
    // Made next to the settings file, not in the current directory
    CHECK(std::filesystem::exists(directory.path / "server.key"));
    CHECK(std::filesystem::exists(directory.path / "rtype.db"));
}
