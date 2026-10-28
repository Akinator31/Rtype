#pragma once

#include "Common/Options.hpp"
#include "rtype/database/Database.hpp"
#include "rtype/log/Logger.hpp"

#include <optional>
#include <vector>

namespace rtype::server
{

/// The schema of the data of the master, one migration per change (docs/architecture/database.md#migrations).
/// A change is a new migration at the end: one already merged is never changed.
inline const std::vector<rtype::database::Migration> ServerMigrations{};

/// The address of the database of the master: the one of --database, or the one the file of --database-file
/// holds.
/// @return Nothing, with the reason logged, if the file cannot be read or holds no address. Its content is
/// never logged: it may hold a password.
[[nodiscard]] std::optional<rtype::database::Address> databaseAddress(const MasterOptions &options,
                                                                      const rtype::log::Logger &logger);

/// Opens the database of the master and applies the migrations it does not have yet.
/// @return Nothing, with the reason logged, if it cannot be opened or its schema is not this server's.
[[nodiscard]] std::optional<rtype::database::Database>
openServerDatabase(const rtype::database::Address &address, const rtype::log::Logger &logger);

} // namespace rtype::server
