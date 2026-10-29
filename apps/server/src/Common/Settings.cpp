#include "Common/Settings.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <fstream>
#include <initializer_list>
#include <iterator>

namespace rtype::server
{

namespace
{

/// A number written in decimal, from `min` to `max`.
std::optional<std::uint64_t> parseNumber(std::string_view text, std::uint64_t min, std::uint64_t max)
{
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size() || value < min || value > max) {
        return std::nullopt;
    }
    return value;
}

/// `127.0.0.1:8080` or `[::1]:8080`.
std::optional<rtype::network::Address> parseEndpoint(std::string_view text)
{
    std::string_view ip;
    std::string_view port;
    if (text.starts_with('[')) {
        const std::size_t close = text.find(']');
        if (close == std::string_view::npos || close + 1 >= text.size() || text[close + 1] != ':') {
            return std::nullopt;
        }
        ip = text.substr(1, close - 1);
        port = text.substr(close + 2);
    } else {
        const std::size_t colon = text.rfind(':');
        if (colon == std::string_view::npos) {
            return std::nullopt;
        }
        ip = text.substr(0, colon);
        port = text.substr(colon + 1);
        // An IPv6 address needs its brackets, or its port could not be told from it
        if (ip.find(':') != std::string_view::npos) {
            return std::nullopt;
        }
    }
    const auto number = parseNumber(port, 0, 65535);
    if (!number) {
        return std::nullopt;
    }
    return rtype::network::Address::parse(ip, static_cast<std::uint16_t>(*number));
}

/// `172.16.0.0/12`, or one address alone.
std::optional<Subnet> parseSubnet(std::string_view text)
{
    const std::size_t slash = text.find('/');
    const auto address = rtype::network::Address::parse(text.substr(0, slash), 0);
    if (!address) {
        return std::nullopt;
    }
    const unsigned bits = address->isIpv4() ? 32 : 128;
    if (slash == std::string_view::npos) {
        return Subnet{*address, bits};
    }
    const auto prefix = parseNumber(text.substr(slash + 1), 0, bits);
    if (!prefix) {
        return std::nullopt;
    }
    return Subnet{*address, static_cast<unsigned>(*prefix)};
}

bool isDigits(std::string_view text, std::size_t maxLength)
{
    return !text.empty() && text.size() <= maxLength &&
           std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; });
}

/// Loose on purpose: the code sent there is the real check. One `@`, a domain with a dot, no spaces.
bool isEmail(std::string_view text)
{
    const std::size_t at = text.find('@');
    if (at == std::string_view::npos || at == 0 || text.find('@', at + 1) != std::string_view::npos) {
        return false;
    }
    const std::string_view domain = text.substr(at + 1);
    if (domain.empty() || domain.find('.') == std::string_view::npos || domain.front() == '.' ||
        domain.back() == '.') {
        return false;
    }
    return std::ranges::none_of(text, [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return byte <= ' ' || byte == 0x7F;
    });
}

/// Reads the values of the file, and refuses it with the place of the problem.
class Reader
{
private:
    const std::filesystem::path &_file;
    std::filesystem::path _folder;
    const rtype::log::Logger &_logger;

public:
    Reader(const std::filesystem::path &file, const rtype::log::Logger &logger)
        : _file(file), _folder(file.parent_path()), _logger(logger)
    {
    }

    [[noreturn]] void fail(const toml::source_region &where, std::string_view message) const
    {
        throw SettingsError(
            std::format("{}:{}:{}: {}", _file.string(), where.begin.line, where.begin.column, message));
    }

    /// Where the value of `key` is: the table holds it.
    static const toml::source_region &at(const toml::table &table, std::string_view key)
    {
        return table.get(key)->source();
    }

    /// A path of the file, relative to its folder. One with a root is kept as written: on Windows, `/data/x`
    /// has no drive, so is_relative() is true, yet it is not relative to the folder.
    [[nodiscard]] std::filesystem::path resolve(const std::string &text) const
    {
        const std::filesystem::path path(text);
        return path.has_root_path() ? path : _folder / path;
    }

    /// The section `name`, with its keys checked; nullptr if the file does not have it.
    const toml::table *section(const toml::table &root, std::string_view name,
                               std::initializer_list<std::string_view> keys) const
    {
        const toml::table *table = root.get_as<toml::table>(name);
        if (table == nullptr) {
            return nullptr;
        }
        for (auto &&[key, node] : *table) {
            if (std::ranges::find(keys, key.str()) == keys.end()) {
                fail(key.source(), std::format("unknown key '{}' in [{}]", key.str(), name));
            }
        }
        return table;
    }

    template <typename Value>
    std::optional<Value> get(const toml::table &table, std::string_view section, std::string_view key,
                             std::string_view expected) const
    {
        const toml::node *node = table.get(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        const auto *value = node->as<Value>();
        if (value == nullptr) {
            fail(node->source(), std::format("[{}] {} must be {}", section, key, expected));
        }
        return value->get();
    }

    /// A string, never empty.
    std::optional<std::string> string(const toml::table &table, std::string_view section,
                                      std::string_view key) const
    {
        std::optional<std::string> value = get<std::string>(table, section, key, "a string");
        if (value && value->empty()) {
            fail(at(table, key), std::format("[{}] {} must not be empty", section, key));
        }
        return value;
    }

    std::optional<bool> boolean(const toml::table &table, std::string_view section,
                                std::string_view key) const
    {
        return get<bool>(table, section, key, "true or false");
    }

    std::optional<std::uint16_t> port(const toml::table &table, std::string_view section,
                                      std::string_view key, std::uint16_t min) const
    {
        const std::optional<std::int64_t> value = get<std::int64_t>(table, section, key, "a number");
        if (!value) {
            return std::nullopt;
        }
        if (*value < min || *value > 65535) {
            fail(at(table, key), std::format("[{}] {} must be a port, from {} to 65535", section, key, min));
        }
        return static_cast<std::uint16_t>(*value);
    }

    /// The strings of an array; nothing if the key is not there.
    std::optional<std::vector<std::pair<std::string, const toml::node *>>>
    strings(const toml::table &table, std::string_view section, std::string_view key) const
    {
        const toml::node *node = table.get(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        const toml::array *array = node->as_array();
        if (array == nullptr) {
            fail(node->source(), std::format("[{}] {} must be an array of strings", section, key));
        }
        std::vector<std::pair<std::string, const toml::node *>> values;
        for (const toml::node &element : *array) {
            const auto *text = element.as_string();
            if (text == nullptr) {
                fail(element.source(), std::format("[{}] {} must be an array of strings", section, key));
            }
            values.emplace_back(text->get(), &element);
        }
        return values;
    }

    /// The secret of the file a `*_file` key names.
    std::optional<std::string> secret(const toml::table &table, std::string_view section,
                                      std::string_view key) const
    {
        const std::optional<std::string> name = string(table, section, key);
        if (!name) {
            return std::nullopt;
        }
        try {
            return readSecretFile(resolve(*name), _logger);
        } catch (const std::runtime_error &error) {
            fail(at(table, key), std::format("[{}] {}: {}", section, key, error.what()));
        }
    }
};

void readServer(const Reader &reader, const toml::table &root, ServerSettings &settings)
{
    const toml::table *server =
        reader.section(root, "server", {"port", "game_ports", "key", "database", "database_file"});
    if (server == nullptr) {
        return;
    }
    // 0 lets the system choose, as --port 0
    settings.port = reader.port(*server, "server", "port", 0);
    if (const auto range = reader.string(*server, "server", "game_ports")) {
        const std::size_t dash = range->find('-');
        const auto first = parseNumber(std::string_view(*range).substr(0, dash), 1, 65535);
        const auto last = dash == std::string::npos
                              ? std::nullopt
                              : parseNumber(std::string_view(*range).substr(dash + 1), 1, 65535);
        if (!first || !last || *first > *last) {
            reader.fail(
                Reader::at(*server, "game_ports"),
                std::format("[server] game_ports must be \"<first>-<last>\", two ports from 1 to 65535 "
                            "in order, not \"{}\"",
                            *range));
        }
        settings.firstGamePort = static_cast<std::uint16_t>(*first);
        settings.lastGamePort = static_cast<std::uint16_t>(*last);
    }
    if (const auto key = reader.string(*server, "server", "key")) {
        settings.key = reader.resolve(*key);
    }
    if (const auto database = reader.string(*server, "server", "database")) {
        try {
            settings.database = rtype::database::Address::parse(*database);
        } catch (const std::invalid_argument &error) {
            reader.fail(Reader::at(*server, "database"), std::format("[server] database: {}", error.what()));
        }
        if (settings.database->engine == rtype::database::Engine::Sqlite) {
            settings.database->target = reader.resolve(settings.database->target).string();
        }
    }
    if (const auto file = reader.string(*server, "server", "database_file")) {
        if (settings.database) {
            reader.fail(Reader::at(*server, "database_file"),
                        "[server] database and database_file both give the database: choose one");
        }
        settings.databaseFile = reader.resolve(*file);
    }
}

void readWeb(const Reader &reader, const toml::table &root, ServerSettings &settings)
{
    const toml::table *web =
        reader.section(root, "web", {"listen", "public_url", "trusted_proxies", "panel_dir"});
    if (web == nullptr) {
        return;
    }
    if (const auto listen = reader.string(*web, "web", "listen")) {
        const auto address = parseEndpoint(*listen);
        if (!address) {
            reader.fail(
                Reader::at(*web, "listen"),
                std::format("[web] listen must be \"<ip>:<port>\", \"[<ipv6>]:<port>\" for IPv6, not \"{}\"",
                            *listen));
        }
        settings.web.listen = *address;
    }
    if (auto url = reader.string(*web, "web", "public_url")) {
        if (!url->starts_with("https://") && !url->starts_with("http://")) {
            reader.fail(
                Reader::at(*web, "public_url"),
                std::format("[web] public_url must start with https:// (or http://), not \"{}\"", *url));
        }
        while (url->ends_with('/')) {
            url->pop_back();
        }
        settings.web.publicUrl = *url;
    }
    if (const auto proxies = reader.strings(*web, "web", "trusted_proxies")) {
        for (const auto &[text, node] : *proxies) {
            const auto subnet = parseSubnet(text);
            if (!subnet) {
                reader.fail(
                    node->source(),
                    std::format("[web] trusted_proxies: \"{}\" is not an address, or <ip>/<prefix>", text));
            }
            settings.web.trustedProxies.push_back(*subnet);
        }
    }
    if (const auto folder = reader.string(*web, "web", "panel_dir")) {
        settings.web.panelDir = reader.resolve(*folder);
    }
}

void readAccounts(const Reader &reader, const toml::table &root, ServerSettings &settings)
{
    const toml::table *accounts = reader.section(root, "accounts", {"enabled", "guests"});
    if (accounts == nullptr) {
        return;
    }
    AccountsSettings &result = settings.accounts.emplace();
    result.enabled = reader.boolean(*accounts, "accounts", "enabled").value_or(true);
    result.guests = reader.boolean(*accounts, "accounts", "guests").value_or(true);
}

void readMail(const Reader &reader, const toml::table &root, ServerSettings &settings)
{
    const toml::table *mail =
        reader.section(root, "mail", {"host", "port", "security", "username", "password_file", "from"});
    if (mail == nullptr) {
        return;
    }
    MailSettings &result = settings.mail.emplace();
    const auto host = reader.string(*mail, "mail", "host");
    if (!host) {
        reader.fail(mail->source(), "[mail] needs host, the mail server");
    }
    result.host = *host;
    if (const auto security = reader.string(*mail, "mail", "security")) {
        if (*security == "starttls") {
            result.security = MailSecurity::StartTls;
        } else if (*security == "tls") {
            result.security = MailSecurity::Tls;
        } else {
            reader.fail(
                Reader::at(*mail, "security"),
                std::format("[mail] security must be \"starttls\" or \"tls\", not \"{}\"", *security));
        }
    }
    result.port =
        reader.port(*mail, "mail", "port", 1).value_or(result.security == MailSecurity::Tls ? 465 : 587);
    const auto username = reader.string(*mail, "mail", "username");
    const bool passwordGiven = mail->contains("password_file");
    if (username.has_value() != passwordGiven) {
        reader.fail(mail->source(), "[mail] username and password_file go together: give both, or neither");
    }
    result.username = username.value_or("");
    result.password = reader.secret(*mail, "mail", "password_file").value_or("");
    const auto from = reader.string(*mail, "mail", "from");
    if (!from) {
        reader.fail(mail->source(),
                    "[mail] needs from, the sender of the mails: \"R-Type <rtype@example.org>\"");
    }
    result.from = *from;
}

void readDiscord(const Reader &reader, const toml::table &root, ServerSettings &settings)
{
    const toml::table *discord = reader.section(root, "discord", {"client_id", "client_secret_file"});
    if (discord == nullptr) {
        return;
    }
    DiscordSettings &result = settings.discord.emplace();
    const auto clientId = reader.string(*discord, "discord", "client_id");
    if (!clientId) {
        reader.fail(discord->source(), "[discord] needs client_id, the id of the Discord application");
    }
    if (!isDigits(*clientId, 20)) {
        reader.fail(
            Reader::at(*discord, "client_id"),
            std::format("[discord] client_id must be the digits of a Discord id, not \"{}\"", *clientId));
    }
    result.clientId = *clientId;
    const auto secret = reader.secret(*discord, "discord", "client_secret_file");
    if (!secret) {
        reader.fail(discord->source(), "[discord] needs client_secret_file, the file of its secret");
    }
    result.clientSecret = *secret;
}

void readAdmin(const Reader &reader, const toml::table &root, ServerSettings &settings)
{
    const toml::table *admin = reader.section(root, "admin", {"admins"});
    if (admin == nullptr) {
        return;
    }
    for (const auto &[text, node] :
         reader.strings(*admin, "admin", "admins")
             .value_or(std::vector<std::pair<std::string, const toml::node *>>{})) {
        const bool discord = text.starts_with("discord:") && isDigits(std::string_view(text).substr(8), 20);
        if (!discord && !isEmail(text)) {
            // A username can be taken by anyone who registers it first
            reader.fail(node->source(),
                        std::format("[admin] admins: \"{}\" is neither an email nor discord:<id>; an "
                                    "administrator is never named by username",
                                    text));
        }
        settings.admin.admins.push_back(text);
    }
}

/// The rules between the sections.
void checkSections(const Reader &reader, const toml::table &root, const ServerSettings &settings)
{
    if (settings.accountsEnabled() && !settings.mail) {
        reader.fail(root.get("accounts")->source(), "[accounts] needs a [mail] section, to send the codes");
    }
    if (settings.mail && !settings.accounts) {
        reader.fail(root.get("mail")->source(),
                    "[mail] only sends the codes of the accounts: add [accounts]");
    }
    if (settings.discord && !settings.accountsEnabled()) {
        reader.fail(root.get("discord")->source(), "[discord] signs in to accounts: add [accounts]");
    }
    if (settings.discord && settings.web.publicUrl.empty()) {
        reader.fail(root.get("discord")->source(),
                    "[discord] needs public_url in [web], where Discord sends the players back");
    }
    if (!settings.accountsEnabled() && !isLoopback(settings.web.listen)) {
        // Without accounts, nobody signs in to the panel: only this machine may reach it
        reader.fail(
            Reader::at(*root.get_as<toml::table>("web"), "listen"),
            std::format("[web] listen must be a loopback address (127.0.0.1, [::1]) without accounts, "
                        "not {}",
                        settings.web.listen));
    }
}

} // namespace

bool isLoopback(const rtype::network::Address &address)
{
    const std::vector<std::uint8_t> bytes = address.bytes();
    if (address.isIpv4()) {
        return bytes[0] == 127;
    }
    return std::all_of(bytes.begin(), bytes.end() - 1, [](std::uint8_t byte) { return byte == 0; }) &&
           bytes.back() == 1;
}

std::string readSecretFile(const std::filesystem::path &path, const rtype::log::Logger &logger)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error(std::format("cannot read {}", path.string()));
    }
    std::string line;
    std::getline(file, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
        line.pop_back();
    }
    if (line.empty()) {
        throw std::runtime_error(std::format("{} holds no secret on its first line", path.string()));
    }
#ifndef _WIN32
    std::error_code error;
    const std::filesystem::perms permissions = std::filesystem::status(path, error).permissions();
    if (!error && (permissions & std::filesystem::perms::others_read) != std::filesystem::perms::none) {
        logger.warning("{} can be read by every user of this machine: make it readable by its owner only "
                       "(chmod 600)",
                       path.string());
    }
#else
    (void)logger;
#endif
    return line;
}

ServerSettings parseSettings(std::string_view text, const std::filesystem::path &file,
                             const rtype::log::Logger &logger)
{
    const Reader reader(file, logger);
    toml::table root;
    try {
        root = toml::parse(text, file.string());
    } catch (const toml::parse_error &error) {
        reader.fail(error.source(), error.description());
    }

    constexpr std::array<std::string_view, 6> Sections = {"server", "web",     "accounts",
                                                          "mail",   "discord", "admin"};
    for (auto &&[key, node] : root) {
        const bool known = std::ranges::find(Sections, key.str()) != Sections.end();
        if (known && !node.is_table()) {
            reader.fail(key.source(), std::format("{} must be a section, [{}]", key.str(), key.str()));
        }
        if (!known) {
            reader.fail(key.source(), node.is_table()
                                          ? std::format("unknown section [{}]", key.str())
                                          : std::format("unknown key '{}' outside of a section", key.str()));
        }
    }

    ServerSettings settings;
    readServer(reader, root, settings);
    readWeb(reader, root, settings);
    readAccounts(reader, root, settings);
    readMail(reader, root, settings);
    readDiscord(reader, root, settings);
    readAdmin(reader, root, settings);
    checkSections(reader, root, settings);
    return settings;
}

ServerSettings loadSettings(const std::filesystem::path &file, const rtype::log::Logger &logger)
{
    std::ifstream stream(file, std::ios::binary);
    std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (!stream && !stream.eof()) {
        throw SettingsError(std::format("Cannot read the settings file {}", file.string()));
    }
    return parseSettings(text, file, logger);
}

} // namespace rtype::server
