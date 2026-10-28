#include "rtype/database/Database.hpp"

#include "rtype/log/Logger.hpp"

#include <soci/sqlite3/soci-sqlite3.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <string>
#include <system_error>

namespace rtype::database
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("database");
    return instance;
}

/// How long SQLite waits for a lock held by another connection before failing, in seconds.
constexpr std::string_view SqliteBusyTimeout = "5";

std::unique_ptr<soci::session> openSqlite(const Address &address)
{
    const std::filesystem::path file(address.target);
    if (file.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(file.parent_path(), error);
        if (error) {
            throw Error(std::format("Cannot create the directory of the database {}: {}", address.toString(),
                                    error.message()));
        }
    }
    // Options rather than a connection string, which would cut a file name at its first space
    soci::connection_parameters parameters(soci::sqlite3, "");
    parameters.set_option("db", address.target);
    parameters.set_option("timeout", std::string(SqliteBusyTimeout));
    // SQLite ignores foreign keys unless asked, PostgreSQL always checks them
    parameters.set_option("foreign_keys", "on");
    auto session = std::make_unique<soci::session>(parameters);
    // SQLite opens a file lazily: reading it now refuses a file that is not a database at once
    int tables = 0;
    *session << "SELECT COUNT(*) FROM sqlite_master", soci::into(tables);
    return session;
}

long long nowMilliseconds()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void createMigrationsTable(soci::session &session)
{
    session << std::format(
        "CREATE TABLE IF NOT EXISTS {} (version BIGINT PRIMARY KEY, description TEXT NOT NULL, "
        "applied_at BIGINT NOT NULL)",
        Database::MigrationsTable);
}

/// Checks that the migrations are numbered 1, 2, 3..., each with a description and statements.
void checkNumbering(std::span<const Migration> migrations)
{
    for (std::size_t i = 0; i < migrations.size(); ++i) {
        const Migration &migration = migrations[i];
        if (migration.version != static_cast<int>(i) + 1) {
            throw std::invalid_argument(std::format(
                "migration {} is numbered {}: they are numbered 1, 2, 3...", i + 1, migration.version));
        }
        if (migration.description.empty() || migration.statements.empty()) {
            throw std::invalid_argument(
                std::format("migration {} needs a description and statements", migration.version));
        }
    }
}

} // namespace

Database::Database(Address address, std::unique_ptr<soci::session> session)
    : _address(std::move(address)), _session(std::move(session))
{
}

Database Database::open(const Address &address)
{
    try {
        switch (address.engine) {
        case Engine::Sqlite:
            return Database(address, openSqlite(address));
        case Engine::Postgresql:
            break;
        }
    } catch (const soci::soci_error &error) {
        throw Error(std::format("Cannot open the database {}: {}", address.toString(), error.what()));
    }
    throw Error(std::format("Cannot open the database {}: this program is built without PostgreSQL",
                            address.toString()));
}

int Database::schemaVersion()
{
    try {
        createMigrationsTable(*_session);
        long long version = 0;
        soci::indicator indicator = soci::i_ok;
        *_session << std::format("SELECT MAX(version) FROM {}", MigrationsTable),
            soci::into(version, indicator);
        return indicator == soci::i_null ? 0 : static_cast<int>(version);
    } catch (const soci::soci_error &error) {
        throw Error(
            std::format("Cannot read the schema of the database {}: {}", _address.toString(), error.what()));
    }
}

int Database::migrate(std::span<const Migration> migrations)
{
    checkNumbering(migrations);

    // What the database has: the same migrations as the program, as far as it goes
    int applied = 0;
    try {
        createMigrationsTable(*_session);
        const soci::rowset<soci::row> rows =
            (_session->prepare << std::format("SELECT version, description FROM {} ORDER BY version",
                                              MigrationsTable));
        for (const soci::row &row : rows) {
            const auto version = row.get<long long>(0);
            const auto description = row.get<std::string>(1);
            if (version != applied + 1) {
                throw Error(std::format("The database {} has migration {} without migration {}",
                                        _address.toString(), version, applied + 1));
            }
            if (version > static_cast<long long>(migrations.size())) {
                throw Error(std::format("The database {} has migration {} ({}), which this program does not "
                                        "know: it was made by a newer one",
                                        _address.toString(), version, description));
            }
            const Migration &known = migrations[static_cast<std::size_t>(version - 1)];
            if (description != known.description) {
                throw Error(
                    std::format("Migration {} of the database {} is \"{}\", this program's is \"{}\": "
                                "they come from different versions of the program",
                                version, _address.toString(), description, known.description));
            }
            ++applied;
        }
    } catch (const soci::soci_error &error) {
        throw Error(
            std::format("Cannot read the schema of the database {}: {}", _address.toString(), error.what()));
    }

    const int already = applied;
    for (const Migration &migration : migrations.subspan(static_cast<std::size_t>(already))) {
        try {
            transaction([&] {
                for (const std::string_view statement : migration.statements) {
                    *_session << std::string(statement);
                }
                const long long version = migration.version;
                const std::string description(migration.description);
                const long long appliedAt = nowMilliseconds();
                *_session << std::format(
                    "INSERT INTO {} (version, description, applied_at) VALUES (:version, "
                    ":description, :applied_at)",
                    MigrationsTable),
                    soci::use(version), soci::use(description), soci::use(appliedAt);
            });
        } catch (const soci::soci_error &error) {
            throw Error(std::format("Migration {} ({}) of the database {} failed, nothing of it was kept: {}",
                                    migration.version, migration.description, _address.toString(),
                                    error.what()));
        }
        logger().info("Database {}: migration {} applied, {}", _address.toString(), migration.version,
                      migration.description);
        ++applied;
    }
    return applied - already;
}

} // namespace rtype::database
