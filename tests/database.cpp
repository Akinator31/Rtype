// Tests of the database library: addresses, opening, migrations, transactions, and the SQL both engines share
// (docs/architecture/database.md).

#include "rtype/database/Database.hpp"

#include "TestDatabase.hpp"
#include "rtype/database/Address.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

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
using rtype::test::tableExists;
using rtype::test::TestDatabase;

namespace
{

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
    const TestDatabase test(Engine::Sqlite, "database_created");
    const Address address = Address::parse("sqlite:" + (test.directory() / "nested" / "rtype.db").string());

    Database database = Database::open(address);

    CHECK(std::filesystem::exists(test.directory() / "nested" / "rtype.db"));
    CHECK(database.engine() == Engine::Sqlite);
    CHECK(database.address() == address);
    CHECK(database.schemaVersion() == 0);
}

TEST_CASE("A file name with spaces is a file name", "[database]")
{
    const TestDatabase test(Engine::Sqlite, "database_spaces");

    Database database =
        Database::open(Address::parse("sqlite:" + (test.directory() / "my games" / "r type.db").string()));

    CHECK(std::filesystem::exists(test.directory() / "my games" / "r type.db"));
}

TEST_CASE("A file that is not a database is refused, and left as it is", "[database]")
{
    const TestDatabase test(Engine::Sqlite, "database_not_a_database");
    std::filesystem::create_directories(test.directory());
    const std::string content(4096, 'x');
    std::ofstream(test.directory() / "rtype.db", std::ios::binary) << content;

    CHECK_THROWS_AS(Database::open(test.address()), Error);

    std::ifstream file(test.directory() / "rtype.db", std::ios::binary);
    const std::string kept((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    CHECK(kept == content);
}

TEST_CASE("A PostgreSQL database that cannot be opened says why, without its password", "[database]")
{
    // Nothing listens on port 1: refused at once
    const Address address = Address::parse("postgresql://rtype:secret@127.0.0.1:1/rtype?connect_timeout=2");
    try {
        (void)Database::open(address);
        FAIL("opened");
    } catch (const Error &error) {
        const std::string message = error.what();
        CAPTURE(message);
        CHECK(message.find("secret") == std::string::npos);
        CHECK((message.find("built without PostgreSQL") != std::string::npos) ==
              !rtype::database::isBuiltWith(Engine::Postgresql));
    }
}

// --- Migrations ---

TEST_CASE("Migrations are applied in order, once", "[database]")
{
    const Engine engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const TestDatabase test(engine, "database_migrations");

    {
        Database database = Database::open(test.address());
        CHECK(database.migrate(std::span(Migrations).first(1)) == 1);
        CHECK(database.schemaVersion() == 1);
        CHECK(tableExists(database, "players"));
        CHECK_FALSE(tableExists(database, "games"));
    }
    {
        // Reopened by a newer program: only the new migration
        Database database = Database::open(test.address());
        CHECK(database.migrate(Migrations) == 1);
        CHECK(database.schemaVersion() == 2);
        CHECK(tableExists(database, "games"));
        // Again: nothing to do
        CHECK(database.migrate(Migrations) == 0);
    }

    Database database = Database::open(test.address());
    std::vector<long long> versions(10);
    std::vector<std::string> descriptions(10);
    database.session() << "SELECT version, description FROM schema_migrations ORDER BY version",
        soci::into(versions), soci::into(descriptions);
    CHECK(versions == std::vector<long long>{1, 2});
    CHECK(descriptions == std::vector<std::string>{"create the players table", "create the games table"});
}

TEST_CASE("A migration that fails leaves the database at the previous one", "[database]")
{
    const Engine engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const TestDatabase test(engine, "database_failing_migration");
    std::vector<Migration> migrations = Migrations;
    migrations.push_back(
        {3,
         "a broken migration",
         {"CREATE TABLE ships (id BIGINT PRIMARY KEY)", "CREATE TABLE ships (id BIGINT PRIMARY KEY)"}});
    Database database = Database::open(test.address());

    CHECK_THROWS_AS(database.migrate(migrations), Error);

    CHECK(database.schemaVersion() == 2);
    CHECK(tableExists(database, "games"));
    // Its first statement was rolled back with it
    CHECK_FALSE(tableExists(database, "ships"));
}

TEST_CASE("A database newer than the program is refused", "[database]")
{
    const Engine engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const TestDatabase test(engine, "database_newer");
    Database::open(test.address()).migrate(Migrations);
    Database database = Database::open(test.address());

    CHECK_THROWS_AS(database.migrate(std::span(Migrations).first(1)), Error);
    CHECK(database.schemaVersion() == 2);
}

TEST_CASE("A migration known by another description is refused", "[database]")
{
    const Engine engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const TestDatabase test(engine, "database_other_branch");
    Database::open(test.address()).migrate(Migrations);
    std::vector<Migration> other = Migrations;
    other[1].description = "create the scores table";
    Database database = Database::open(test.address());

    try {
        database.migrate(other);
        FAIL("migrated");
    } catch (const Error &error) {
        CHECK(std::string(error.what()).find("create the scores table") != std::string::npos);
    }
}

TEST_CASE("Migrations must be numbered 1, 2, 3..., with a description and statements", "[database]")
{
    const Engine engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const TestDatabase test(engine, "database_numbering");
    Database database = Database::open(test.address());
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
    const Engine engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const TestDatabase test(engine, "database_transactions");
    Database database = Database::open(test.address());
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
    const Engine engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const TestDatabase test(engine, "database_portable_sql");
    Database database = Database::open(test.address());
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
