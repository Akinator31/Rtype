#include "Master/ServerDatabase.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace rtype::server
{

std::optional<rtype::database::Address> databaseAddress(const MasterOptions &options,
                                                        const rtype::log::Logger &logger)
{
    if (options.databaseFile.empty()) {
        if (options.database.toString() != options.database.target &&
            options.database.engine == rtype::database::Engine::Postgresql) {
            logger.warning(
                "The address of the database holds a password, which every user of this machine sees "
                "on the command line: give it with --database-file, or in ~/.pgpass");
        }
        return options.database;
    }
    std::ifstream file(options.databaseFile);
    std::string line;
    if (!file || !std::getline(file, line)) {
        logger.error("Cannot read the address of the database in {}", options.databaseFile.string());
        return std::nullopt;
    }
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
        line.pop_back();
    }
    try {
        return rtype::database::Address::parse(line);
    } catch (const std::invalid_argument &) {
        logger.error("{} does not hold the address of a database on its first line: sqlite:<file>, or "
                     "postgresql://<user>:<password>@<host>:<port>/<database>",
                     options.databaseFile.string());
        return std::nullopt;
    }
}

std::optional<rtype::database::Database> openServerDatabase(const rtype::database::Address &address,
                                                            const rtype::log::Logger &logger)
{
    try {
        rtype::database::Database database = rtype::database::Database::open(address);
        database.migrate(ServerMigrations);
        logger.info("Database {}, schema {}", address.toString(), database.schemaVersion());
        return database;
    } catch (const rtype::database::Error &error) {
        logger.error("{}", error.what());
        return std::nullopt;
    }
}

} // namespace rtype::server
