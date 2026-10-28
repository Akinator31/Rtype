#pragma once

#include "rtype/database/Address.hpp"

#include <soci/soci.h>

#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rtype::database
{

/// A database that cannot be opened, or whose schema this program cannot use. The message is for the user.
class Error : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/// One change of the schema of a program: the tables it adds or changes, in SQL that SQLite and PostgreSQL
/// both understand (docs/architecture/database.md#portable-sql).
struct Migration {
    /// Its number: the migrations of a program are numbered 1, 2, 3... in the order they apply.
    int version = 0;
    /// What it does, kept in the database: "create the games table".
    std::string_view description;
    /// Its statements, one SQL statement each: SQLite runs only the first statement of a string.
    std::vector<std::string_view> statements;
};

/// A connection to a database, SQLite or PostgreSQL, through one SOCI session. It is not thread-safe: one
/// thread uses it at a time.
///
/// @code
/// using rtype::database::Database;
/// Database database = Database::open(rtype::database::Address::parse("sqlite:rtype.db"));
/// database.migrate(Migrations);
/// database.transaction([&] {
///     database.session() << "INSERT INTO games (id, ended_at) VALUES (:id, :ended)", soci::use(id),
///         soci::use(endedAt);
/// });
/// @endcode
class Database
{
    Address _address;
    std::unique_ptr<soci::session> _session;

    Database(Address address, std::unique_ptr<soci::session> session);

public:
    /// Where the version of the schema is kept: one row per migration applied.
    static constexpr std::string_view MigrationsTable = "schema_migrations";

    /// Connects to the database. A SQLite file is created with its directories if missing.
    /// @throws Error if it cannot be opened, or its engine is not built into this program.
    [[nodiscard]] static Database open(const Address &address);

    [[nodiscard]] const Address &address() const { return _address; }
    [[nodiscard]] Engine engine() const { return _address.engine; }
    /// The SOCI session, to run queries: `database.session() << "SELECT ...", soci::into(value);`
    [[nodiscard]] soci::session &session() { return *_session; }

    /// The version of the schema: the last migration applied, 0 for a new database.
    /// @throws Error if it cannot be read.
    [[nodiscard]] int schemaVersion();

    /// Applies the migrations the database does not have yet, in order, each in a transaction with its
    /// version: a migration that fails leaves the database at the previous one.
    /// @throws std::invalid_argument if the migrations are not numbered 1, 2, 3...: a bug of the program.
    /// @throws Error if a migration fails, or the database has a migration this program does not know, or
    /// knows by another description: a newer program, or another branch, wrote it.
    /// @return The number of migrations applied.
    int migrate(std::span<const Migration> migrations);

    /// Runs the function in a transaction: committed if it returns, rolled back if it throws, and the
    /// exception goes on.
    /// @return What the function returns.
    template <typename Function>
    std::invoke_result_t<Function &> transaction(Function &&function)
    {
        soci::transaction transaction(*_session);
        if constexpr (std::is_void_v<std::invoke_result_t<Function &>>) {
            function();
            transaction.commit();
        } else {
            auto result = function();
            transaction.commit();
            return result;
        }
    }
};

} // namespace rtype::database
