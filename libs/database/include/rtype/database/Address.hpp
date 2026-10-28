#pragma once

#include <string>
#include <string_view>

/// The data a program keeps from one run to the next, in SQLite or PostgreSQL through SOCI
/// (docs/architecture/database.md, ADR 0023).
namespace rtype::database
{

/// The engines a database can run on.
enum class Engine {
    Sqlite,
    Postgresql,
};

/// Where a database is, as the user gives it:
///
/// - `sqlite:<file>`: a SQLite file, created with its directories if missing (`sqlite:rtype.db`,
///   `sqlite:/data/rtype.db`);
/// - `postgresql://...` or `postgres://...`: a PostgreSQL server, in the format libpq reads
///   (`postgresql://rtype@db.example.org:5432/rtype?sslmode=verify-full`).
///
/// @code
/// const auto address = rtype::database::Address::parse("sqlite:rtype.db");
/// logger.info("Database {}", address.toString());
/// @endcode
struct Address {
    Engine engine = Engine::Sqlite;
    /// The file for SQLite; the whole address for PostgreSQL, which libpq reads as it is.
    std::string target;

    /// @throws std::invalid_argument with a message for the user, if the text is none of the forms above.
    [[nodiscard]] static Address parse(std::string_view text);

    /// The address as the user gives it, without its password if it has one: for the logs.
    [[nodiscard]] std::string toString() const;

    friend bool operator==(const Address &, const Address &) = default;
};

} // namespace rtype::database
