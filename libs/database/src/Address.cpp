#include "rtype/database/Address.hpp"

#include <format>
#include <stdexcept>

namespace rtype::database
{

namespace
{

constexpr std::string_view SqlitePrefix = "sqlite:";
constexpr std::string_view PostgresqlSchemes[] = {"postgresql://", "postgres://"};

/// The address of a PostgreSQL server without its password: in its user part (`user:password@`), and in its
/// parameters (`password=...`).
std::string withoutPassword(std::string address)
{
    const std::size_t schemeEnd = address.find("://") + 3;
    const std::size_t hostStart = address.find('@', schemeEnd);
    const std::size_t pathStart = address.find_first_of("/?", schemeEnd);
    if (hostStart != std::string::npos && (pathStart == std::string::npos || hostStart < pathStart)) {
        const std::size_t colon = address.find(':', schemeEnd);
        if (colon != std::string::npos && colon < hostStart) {
            address.replace(colon + 1, hostStart - colon - 1, "***");
        }
    }
    for (std::size_t found = address.find("password="); found != std::string::npos;
         found = address.find("password=", found + 1)) {
        const bool parameter = found > 0 && (address[found - 1] == '?' || address[found - 1] == '&');
        if (!parameter) {
            continue;
        }
        const std::size_t valueStart = found + std::string_view("password=").size();
        const std::size_t valueEnd = address.find('&', valueStart);
        address.replace(valueStart, (valueEnd == std::string::npos ? address.size() : valueEnd) - valueStart,
                        "***");
    }
    return address;
}

} // namespace

Address Address::parse(std::string_view text)
{
    if (text.starts_with(SqlitePrefix)) {
        const std::string_view file = text.substr(SqlitePrefix.size());
        if (file.empty()) {
            throw std::invalid_argument("a SQLite database needs a file: sqlite:<file>");
        }
        return {Engine::Sqlite, std::string(file)};
    }
    for (const std::string_view scheme : PostgresqlSchemes) {
        if (text.starts_with(scheme)) {
            if (text.size() == scheme.size()) {
                throw std::invalid_argument(std::format(
                    "a PostgreSQL database needs its server: {}<user>@<host>/<database>", scheme));
            }
            return {Engine::Postgresql, std::string(text)};
        }
    }
    throw std::invalid_argument(std::format(
        "'{}' is not the address of a database: sqlite:<file>, or postgresql://<user>@<host>/<database>",
        text));
}

std::string Address::toString() const
{
    switch (engine) {
    case Engine::Sqlite:
        return std::string(SqlitePrefix).append(target);
    case Engine::Postgresql:
        return withoutPassword(target);
    }
    return target;
}

} // namespace rtype::database
