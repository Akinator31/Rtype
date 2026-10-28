// Tests of the database library: addresses, opening, migrations, transactions, and the SQL both engines share
// (docs/architecture/database.md).

#include "rtype/database/Database.hpp"

#include "rtype/database/Address.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using rtype::database::Address;
using rtype::database::Database;
using rtype::database::Engine;
using rtype::database::Error;
using rtype::database::Migration;

namespace
{

/// A directory of its own for each test, removed at the end: CTest runs them at once.
struct TempDirectory {
    std::filesystem::path path;

    explicit TempDirectory(std::string_view name)
        : path(std::filesystem::temp_directory_path() / std::string("rtype_tests_database_").append(name))
    {
        std::filesystem::remove_all(path);
    }
    ~TempDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    TempDirectory(const TempDirectory &) = delete;
    TempDirectory &operator=(const TempDirectory &) = delete;

    [[nodiscard]] Address database() const
    {
        return Address::parse("sqlite:" + (path / "rtype.db").string());
    }
};

const std::vector<Migration> Migrations{
    {1,
     "create the players table",
     {"CREATE TABLE players (id BIGINT PRIMARY KEY, name TEXT NOT NULL UNIQUE, created_at BIGINT NOT NULL)"}},
    {2,
     "create the games table",
     {"CREATE TABLE games (id BIGINT PRIMARY KEY, player_id BIGINT NOT NULL REFERENCES players (id), "
      "score BIGINT NOT NULL)",
      "CREATE INDEX games_by_player ON games (player_id)"}},
};

bool tableExists(Database &database, const std::string &table)
{
    int count = 0;
    database.session() << "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = :name",
        soci::use(table), soci::into(count);
    return count == 1;
}

} // namespace

// --- Addresses ---

TEST_CASE("A database address names its engine and its target", "[database]")
{
    CHECK(Address::parse("sqlite:rtype.db") == Address{Engine::Sqlite, "rtype.db"});
    CHECK(Address::parse("sqlite:/data/my games/rtype.db") ==
          Address{Engine::Sqlite, "/data/my games/rtype.db"});
    CHECK(Address::parse("postgresql://rtype@db.example.org/rtype") ==
          Address{Engine::Postgresql, "postgresql://rtype@db.example.org/rtype"});
    CHECK(Address::parse("postgres://rtype@localhost").engine == Engine::Postgresql);
}

TEST_CASE("Anything else is not a database address", "[database]")
{
    for (const std::string_view text : {"", "rtype.db", "sqlite:", "sqlite", "mysql://rtype@host/rtype",
                                        "postgresql://", "postgres://", "SQLITE:rtype.db"}) {
        CAPTURE(text);
        CHECK_THROWS_AS(Address::parse(text), std::invalid_argument);
    }
}

TEST_CASE("An address is shown without its password", "[database]")
{
    CHECK(Address::parse("sqlite:rtype.db").toString() == "sqlite:rtype.db");
    CHECK(Address::parse("postgresql://rtype@db.example.org:5432/rtype").toString() ==
          "postgresql://rtype@db.example.org:5432/rtype");
    CHECK(Address::parse("postgresql://rtype:secret@db.example.org:5432/rtype").toString() ==
          "postgresql://rtype:***@db.example.org:5432/rtype");
    CHECK(Address::parse("postgresql://db.example.org/rtype?user=rtype&password=secret&sslmode=require")
              .toString() == "postgresql://db.example.org/rtype?user=rtype&password=***&sslmode=require");
    CHECK(Address::parse("postgres://db.example.org/rtype?password=secret").toString() ==
          "postgres://db.example.org/rtype?password=***");
}

// --- Opening ---

TEST_CASE("A SQLite database is created with its directories, empty", "[database]")
{
    const TempDirectory directory("created");
    const Address address = Address::parse("sqlite:" + (directory.path / "nested" / "rtype.db").string());

    Database database = Database::open(address);

    CHECK(std::filesystem::exists(directory.path / "nested" / "rtype.db"));
    CHECK(database.engine() == Engine::Sqlite);
    CHECK(database.address() == address);
    CHECK(database.schemaVersion() == 0);
}

TEST_CASE("A file name with spaces is a file name", "[database]")
{
    const TempDirectory directory("spaces");

    Database database =
        Database::open(Address::parse("sqlite:" + (directory.path / "my games" / "r type.db").string()));

    CHECK(std::filesystem::exists(directory.path / "my games" / "r type.db"));
}

TEST_CASE("A file that is not a database is refused, and left as it is", "[database]")
{
    const TempDirectory directory("not_a_database");
    std::filesystem::create_directories(directory.path);
    const std::string content(4096, 'x');
    std::ofstream(directory.path / "rtype.db", std::ios::binary) << content;

    CHECK_THROWS_AS(Database::open(directory.database()), Error);

    std::ifstream file(directory.path / "rtype.db", std::ios::binary);
    const std::string kept((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    CHECK(kept == content);
}

TEST_CASE("A PostgreSQL database needs a program built with PostgreSQL", "[database]")
{
    try {
        (void)Database::open(Address::parse("postgresql://rtype:secret@db.example.org/rtype"));
        FAIL("opened");
    } catch (const Error &error) {
        const std::string message = error.what();
        CHECK(message.find("built without PostgreSQL") != std::string::npos);
        CHECK(message.find("secret") == std::string::npos);
    }
}

// --- Migrations ---

TEST_CASE("Migrations are applied in order, once", "[database]")
{
    const TempDirectory directory("migrations");

    {
        Database database = Database::open(directory.database());
        CHECK(database.migrate(std::span(Migrations).first(1)) == 1);
        CHECK(database.schemaVersion() == 1);
        CHECK(tableExists(database, "players"));
        CHECK_FALSE(tableExists(database, "games"));
    }
    {
        // Reopened by a newer program: only the new migration
        Database database = Database::open(directory.database());
        CHECK(database.migrate(Migrations) == 1);
        CHECK(database.schemaVersion() == 2);
        CHECK(tableExists(database, "games"));
        // Again: nothing to do
        CHECK(database.migrate(Migrations) == 0);
    }

    Database database = Database::open(directory.database());
    std::vector<long long> versions(10);
    std::vector<std::string> descriptions(10);
    database.session() << "SELECT version, description FROM schema_migrations ORDER BY version",
        soci::into(versions), soci::into(descriptions);
    CHECK(versions == std::vector<long long>{1, 2});
    CHECK(descriptions == std::vector<std::string>{"create the players table", "create the games table"});
}

TEST_CASE("A migration that fails leaves the database at the previous one", "[database]")
{
    const TempDirectory directory("failing_migration");
    std::vector<Migration> migrations = Migrations;
    migrations.push_back(
        {3,
         "a broken migration",
         {"CREATE TABLE ships (id BIGINT PRIMARY KEY)", "CREATE TABLE ships (id BIGINT PRIMARY KEY)"}});
    Database database = Database::open(directory.database());

    CHECK_THROWS_AS(database.migrate(migrations), Error);

    CHECK(database.schemaVersion() == 2);
    CHECK(tableExists(database, "games"));
    // Its first statement was rolled back with it
    CHECK_FALSE(tableExists(database, "ships"));
}

TEST_CASE("A database newer than the program is refused", "[database]")
{
    const TempDirectory directory("newer");
    Database::open(directory.database()).migrate(Migrations);
    Database database = Database::open(directory.database());

    CHECK_THROWS_AS(database.migrate(std::span(Migrations).first(1)), Error);
    CHECK(database.schemaVersion() == 2);
}

TEST_CASE("A migration known by another description is refused", "[database]")
{
    const TempDirectory directory("other_branch");
    Database::open(directory.database()).migrate(Migrations);
    std::vector<Migration> other = Migrations;
    other[1].description = "create the scores table";
    Database database = Database::open(directory.database());

    try {
        database.migrate(other);
        FAIL("migrated");
    } catch (const Error &error) {
        CHECK(std::string(error.what()).find("create the scores table") != std::string::npos);
    }
}

TEST_CASE("Migrations must be numbered 1, 2, 3..., with a description and statements", "[database]")
{
    const TempDirectory directory("numbering");
    Database database = Database::open(directory.database());
    const std::vector<std::vector<Migration>> invalid{
        {{0, "zero", {"SELECT 1"}}},
        {{2, "starts at two", {"SELECT 1"}}},
        {{1, "one", {"SELECT 1"}}, {1, "one again", {"SELECT 1"}}},
        {{1, "one", {"SELECT 1"}}, {3, "a gap", {"SELECT 1"}}},
        {{1, "", {"SELECT 1"}}},
        {{1, "no statement", {}}},
    };

    for (const auto &migrations : invalid) {
        CHECK_THROWS_AS(database.migrate(migrations), std::invalid_argument);
    }
    CHECK(database.schemaVersion() == 0);
}

// --- Transactions ---

TEST_CASE("A transaction is committed if its function returns, rolled back if it throws", "[database]")
{
    const TempDirectory directory("transactions");
    Database database = Database::open(directory.database());
    database.migrate(Migrations);

    const int inserted = database.transaction([&] {
        database.session() << "INSERT INTO players (id, name, created_at) VALUES (1, 'Pavel', 0)";
        return 1;
    });
    CHECK(inserted == 1);
    CHECK_THROWS_AS(database.transaction([&] {
        database.session() << "INSERT INTO players (id, name, created_at) VALUES (2, 'Noah', 0)";
        throw std::runtime_error("the game crashed");
    }),
                    std::runtime_error);

    int players = 0;
    database.session() << "SELECT COUNT(*) FROM players", soci::into(players);
    CHECK(players == 1);
}

// --- Portable SQL ---

TEST_CASE("The SQL both engines share works", "[database]")
{
    const TempDirectory directory("portable_sql");
    Database database = Database::open(directory.database());
    database.migrate(Migrations);
    soci::session &sql = database.session();

    SECTION("An id is created with RETURNING")
    {
        long long id = 0;
        sql << "INSERT INTO players (id, name, created_at) VALUES ((SELECT COALESCE(MAX(id), 0) + 1 FROM "
               "players), "
               "'Pavel', 0) RETURNING id",
            soci::into(id);
        CHECK(id == 1);
    }
    SECTION("A row is inserted or updated with ON CONFLICT")
    {
        const std::string name = "Pavel";
        for (const long long createdAt : {10LL, 20LL}) {
            sql << "INSERT INTO players (id, name, created_at) VALUES (1, :name, :created_at) "
                   "ON CONFLICT (id) DO UPDATE SET created_at = excluded.created_at",
                soci::use(name), soci::use(createdAt);
        }
        long long createdAt = 0;
        sql << "SELECT created_at FROM players WHERE id = 1", soci::into(createdAt);
        CHECK(createdAt == 20);
    }
    SECTION("A date in Unix milliseconds keeps its 64 bits")
    {
        const long long date = 1'798'000'000'123LL;
        sql << "INSERT INTO players (id, name, created_at) VALUES (1, 'Pavel', :date)", soci::use(date);
        long long read = 0;
        sql << "SELECT created_at FROM players WHERE id = 1", soci::into(read);
        CHECK(read == date);
    }
    SECTION("A foreign key is checked")
    {
        CHECK_THROWS_AS(sql << "INSERT INTO games (id, player_id, score) VALUES (1, 42, 100)",
                        soci::soci_error);
    }
    SECTION("A missing value is a null indicator")
    {
        long long best = 0;
        soci::indicator indicator = soci::i_ok;
        sql << "SELECT MAX(score) FROM games", soci::into(best, indicator);
        CHECK(indicator == soci::i_null);
    }
}
