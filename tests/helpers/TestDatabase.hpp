#pragma once

// A database of its own for each test, on each engine the tests can use: SQLite always, PostgreSQL when
// RTYPE_TEST_POSTGRESQL_URL gives a server (docs/architecture/database.md#tests).

#include "rtype/database/Database.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace rtype::test
{

/// The PostgreSQL server of the tests, from RTYPE_TEST_POSTGRESQL_URL:
/// postgresql://rtype:rtype@127.0.0.1/rtype
inline std::optional<std::string> testPostgresqlUrl()
{
#ifdef _WIN32
    // getenv() is deprecated by MSVC
    char *value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "RTYPE_TEST_POSTGRESQL_URL") != 0 || value == nullptr) {
        return std::nullopt;
    }
    std::string result(value);
    std::free(value);
#else
    const char *value = std::getenv("RTYPE_TEST_POSTGRESQL_URL");
    if (value == nullptr) {
        return std::nullopt;
    }
    std::string result(value);
#endif
    if (result.empty()) {
        return std::nullopt;
    }
    return result;
}

/// The engines the tests of the database run on: SQLite, and PostgreSQL if this program has it and a server
/// is given.
inline std::vector<database::Engine> testEngines()
{
    std::vector<database::Engine> engines{database::Engine::Sqlite};
    if (database::isBuiltWith(database::Engine::Postgresql) && testPostgresqlUrl()) {
        engines.push_back(database::Engine::Postgresql);
    }
    return engines;
}

inline std::string_view engineName(database::Engine engine)
{
    return engine == database::Engine::Sqlite ? "SQLite" : "PostgreSQL";
}

/// An empty database for one test, removed at its end: a SQLite file in a directory of its own, or a schema
/// of its own on the PostgreSQL server of the tests. CTest runs the tests at once: each name must be unique.
class TestDatabase
{
    database::Engine _engine;
    std::string _name;
    std::filesystem::path _directory;
    database::Address _address;

    [[nodiscard]] std::string schema() const { return "rtype_test_" + _name; }

    /// Runs a statement on the server of the tests, outside the schema of the test.
    void onServer(const std::string &statement) const
    {
        database::Database server = database::Database::open(database::Address::parse(*testPostgresqlUrl()));
        server.session() << statement;
    }

public:
    TestDatabase(database::Engine engine, std::string_view name)
        : _engine(engine), _name(name),
          _directory(std::filesystem::temp_directory_path() / ("rtype_tests_" + std::string(name)))
    {
        for (char &character : _name) {
            if (std::isalnum(static_cast<unsigned char>(character)) == 0) {
                character = '_';
            }
        }
        if (_engine == database::Engine::Sqlite) {
            std::filesystem::remove_all(_directory);
            _address = database::Address::parse("sqlite:" + (_directory / "rtype.db").string());
            return;
        }
        onServer("DROP SCHEMA IF EXISTS " + schema() + " CASCADE");
        onServer("CREATE SCHEMA " + schema());
        // Every table of the test goes in its schema: the first of the search path
        const std::string url = *testPostgresqlUrl();
        _address = database::Address::parse(url + (url.find('?') == std::string::npos ? "?" : "&") +
                                            "options=-csearch_path%3D" + schema());
    }
    ~TestDatabase()
    {
        if (_engine == database::Engine::Sqlite) {
            std::error_code ignored;
            std::filesystem::remove_all(_directory, ignored);
            return;
        }
        try {
            onServer("DROP SCHEMA IF EXISTS " + schema() + " CASCADE");
        } catch (...) {
            // Left for the next run of the test, which drops it first
        }
    }
    TestDatabase(const TestDatabase &) = delete;
    TestDatabase &operator=(const TestDatabase &) = delete;

    [[nodiscard]] const database::Address &address() const { return _address; }
    /// The address as a user writes it, with its password: for a command line.
    [[nodiscard]] std::string addressText() const
    {
        return _engine == database::Engine::Sqlite ? "sqlite:" + _address.target : _address.target;
    }
    /// The directory of the SQLite file.
    [[nodiscard]] const std::filesystem::path &directory() const { return _directory; }
    [[nodiscard]] database::Database open() const { return database::Database::open(_address); }
};

/// Tells whether the table exists, on either engine.
inline bool tableExists(database::Database &database, const std::string &table)
{
    try {
        long long rows = 0;
        database.session() << "SELECT COUNT(*) FROM " + table, soci::into(rows);
        return true;
    } catch (const soci::soci_error &) {
        return false;
    }
}

} // namespace rtype::test
