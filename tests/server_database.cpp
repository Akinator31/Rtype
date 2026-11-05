// Tests of the database of the master: its address, from --database or --database-file, opened and brought up
// to date at start (docs/architecture/server.md#the-database, ADR 0023).

#include "Common/Options.hpp"
#include "Http.hpp"
#include "Master/ServerDatabase.hpp"
#include "Server.hpp"
#include "TestDatabase.hpp"
#include "rtype/database/Database.hpp"
#include "rtype/log/Log.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/Tls.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace rtype::server;
using rtype::database::Address;
using rtype::database::Database;
using rtype::database::Engine;
using rtype::test::TestDatabase;

namespace
{

/// A directory of its own for each test, with a key for the master, removed at the end.
struct MasterDirectory {
    std::filesystem::path path;

    explicit MasterDirectory(std::string_view name)
        : path(std::filesystem::temp_directory_path() /
               std::string("rtype_tests_server_database_").append(name))
    {
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
        std::ofstream(path / "server.key") << rtype::network::ServerIdentity::generate().privateKeyPem();
    }
    ~MasterDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    MasterDirectory(const MasterDirectory &) = delete;
    MasterDirectory &operator=(const MasterDirectory &) = delete;
};

/// Runs `r-type_server --check` with these database options.
int check(const MasterDirectory &directory, std::vector<std::string> databaseOptions)
{
    std::vector<std::string> arguments{"--check",
                                       "--port",
                                       "0",
                                       "--config",
                                       RTYPE_TEST_CONFIG,
                                       "--key",
                                       (directory.path / "server.key").string(),
                                       "--settings",
                                       testing::webOnAnyPort(directory.path / "web.toml").string()};
    arguments.insert(arguments.end(), databaseOptions.begin(), databaseOptions.end());
    const std::vector<std::string_view> views(arguments.begin(), arguments.end());
    std::ostringstream out;
    std::istringstream in;
    return run(views, out, in);
}

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

} // namespace

// --- Options ---

TEST_CASE("A master keeps its data in rtype.db by default, or where --database says", "[server]")
{
    const Options defaults = parseOptions(std::vector<std::string_view>{});
    CHECK(defaults.master.database == Address{Engine::Sqlite, "rtype.db"});
    CHECK(defaults.master.databaseFile.empty());

    CHECK(
        parseOptions(std::vector<std::string_view>{"--database", "sqlite:/data/rtype.db"}).master.database ==
        Address{Engine::Sqlite, "/data/rtype.db"});
    CHECK(parseOptions(std::vector<std::string_view>{"--database", "postgresql://rtype@db/rtype"})
              .master.database == Address{Engine::Postgresql, "postgresql://rtype@db/rtype"});
    CHECK(parseOptions(std::vector<std::string_view>{"--database-file", "/run/secrets/database"})
              .master.databaseFile == "/run/secrets/database");
}

TEST_CASE("Invalid database options are refused", "[server]")
{
    const std::vector<std::vector<std::string_view>> invalid{
        {"--database"},
        {"--database", "rtype.db"},
        {"--database-file"},
        {"--database", "sqlite:rtype.db", "--database-file", "database.txt"},
        {"--instance", "--id", "7", "--udp-port", "4251", "--master-port", "5000", "--database",
         "sqlite:x.db"},
        {"--instance", "--id", "7", "--udp-port", "4251", "--master-port", "5000", "--database-file", "x"},
    };

    for (const auto &arguments : invalid) {
        CAPTURE(arguments);
        CHECK_THROWS_AS(parseOptions(arguments), std::invalid_argument);
    }
    try {
        (void)parseOptions(std::vector<std::string_view>{"--database", "rtype.db"});
    } catch (const std::invalid_argument &error) {
        CHECK(std::string(error.what()).starts_with("--database: "));
    }
}

TEST_CASE("A password on the command line is warned about, never logged", "[server]")
{
    const Captured captured;
    const rtype::log::Logger logger("tests");
    MasterOptions options;
    options.database = Address::parse("postgresql://rtype:secret@db.example.org/rtype");

    CHECK(databaseAddress(options, logger) == options.database);

    CHECK(captured.sink->text.find("--database-file") != std::string::npos);
    CHECK(captured.sink->text.find("secret") == std::string::npos);
}

// --- Starting ---

TEST_CASE("A master opens its database at start, created the first time", "[server]")
{
    const MasterDirectory directory("created");
    const std::filesystem::path file = directory.path / "data" / "rtype.db";

    REQUIRE(check(directory, {"--database", "sqlite:" + file.string()}) == Success);
    // Again, on the database it made
    REQUIRE(check(directory, {"--database", "sqlite:" + file.string()}) == Success);

    Database database = Database::open(Address::parse("sqlite:" + file.string()));
    CHECK(database.schemaVersion() == static_cast<int>(ServerMigrations.size()));
}

TEST_CASE("A master reads the address of its database from --database-file", "[server]")
{
    const MasterDirectory directory("file");
    const std::filesystem::path file = directory.path / "rtype.db";
    // As an editor or a Docker secret writes it: a line end, perhaps Windows'
    std::ofstream(directory.path / "database.txt", std::ios::binary) << "sqlite:" << file.string() << "\r\n";

    CHECK(check(directory, {"--database-file", (directory.path / "database.txt").string()}) == Success);
    CHECK(std::filesystem::exists(file));
}

TEST_CASE("A database file without an address stops the master, without showing what it holds", "[server]")
{
    const MasterDirectory directory("bad_file");
    std::ofstream(directory.path / "database.txt") << "mysql://rtype:secret@db.example.org/rtype\n";
    std::ofstream(directory.path / "empty.txt") << "";
    const Captured captured;

    CHECK(check(directory, {"--database-file", (directory.path / "database.txt").string()}) == Failure);
    CHECK(check(directory, {"--database-file", (directory.path / "empty.txt").string()}) == Failure);
    CHECK(check(directory, {"--database-file", (directory.path / "missing.txt").string()}) == Failure);

    CHECK(captured.sink->text.find("secret") == std::string::npos);
}

TEST_CASE("A database the master cannot use stops it", "[server]")
{
    const MasterDirectory directory("unusable");

    SECTION("Not a database")
    {
        std::ofstream(directory.path / "rtype.db") << std::string(4096, 'x');
        CHECK(check(directory, {"--database", "sqlite:" + (directory.path / "rtype.db").string()}) ==
              Failure);
    }
    SECTION("A database used by a newer server")
    {
        const Address address = Address::parse("sqlite:" + (directory.path / "rtype.db").string());
        Database::open(address).migrate(std::vector<rtype::database::Migration>{
            {1, "a change of a newer server", {"CREATE TABLE future (id BIGINT PRIMARY KEY)"}}});
        CHECK(check(directory, {"--database", address.toString()}) == Failure);
    }
}

TEST_CASE("A master opens its database on every engine", "[server]")
{
    const Engine engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const MasterDirectory directory("engines");
    const TestDatabase database(engine, "server_database_engines");

    CHECK(check(directory, {"--database", database.addressText()}) == Success);
}
