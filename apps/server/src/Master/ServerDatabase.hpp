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
inline const std::vector<rtype::database::Migration> ServerMigrations{
    {1,
     "create the admin_actions table: the record of the actions of the administrators",
     {"CREATE TABLE admin_actions (id BIGINT PRIMARY KEY, at BIGINT NOT NULL, actor TEXT NOT NULL, "
      "action TEXT NOT NULL, target TEXT NOT NULL, details TEXT NOT NULL)"}},
    {2,
     "create the address_bans table: the IP addresses banned for a time",
     {"CREATE TABLE address_bans (address TEXT PRIMARY KEY, created_at BIGINT NOT NULL, "
      "expires_at BIGINT NOT NULL, reason TEXT NOT NULL, actor TEXT NOT NULL)"}},
    {3,
     "create the accounts table: the accounts of the players",
     {"CREATE TABLE accounts (id BIGINT PRIMARY KEY, username TEXT NOT NULL, "
      "username_key TEXT NOT NULL UNIQUE, email TEXT, email_key TEXT UNIQUE, email_verified_at BIGINT, "
      "password_hash TEXT, role TEXT NOT NULL, banned_at BIGINT, ban_reason TEXT, "
      "code_failures BIGINT NOT NULL, code_failures_day BIGINT NOT NULL, created_at BIGINT NOT NULL, "
      "last_seen_at BIGINT NOT NULL)"}},
    {4,
     "create the account_sessions table: the sessions of the accounts, by the hash of their token",
     {"CREATE TABLE account_sessions (token_hash TEXT PRIMARY KEY, "
      "account_id BIGINT NOT NULL REFERENCES accounts (id) ON DELETE CASCADE, created_at BIGINT NOT NULL, "
      "last_used_at BIGINT NOT NULL, expires_at BIGINT NOT NULL, client TEXT NOT NULL)",
      "CREATE INDEX account_sessions_by_account ON account_sessions (account_id)"}},
    {5,
     "create the account_codes table: the codes sent by email, by their hash",
     {"CREATE TABLE account_codes (account_id BIGINT NOT NULL REFERENCES accounts (id) ON DELETE CASCADE, "
      "purpose TEXT NOT NULL, code_hash TEXT NOT NULL, expires_at BIGINT NOT NULL, attempts BIGINT NOT NULL, "
      "PRIMARY KEY (account_id, purpose))"}},
    {6,
     "create the discord_identities table: the Discord users who sign in to the accounts, one per account",
     {"CREATE TABLE discord_identities (discord_id TEXT PRIMARY KEY, "
      "account_id BIGINT NOT NULL UNIQUE REFERENCES accounts (id) ON DELETE CASCADE, linked_at BIGINT NOT "
      "NULL)"}},
    {7,
     "keep the profile of the Discord identities: their names and their picture, as of their last sign-in",
     {"ALTER TABLE discord_identities ADD COLUMN discord_username TEXT",
      "ALTER TABLE discord_identities ADD COLUMN discord_display_name TEXT",
      "ALTER TABLE discord_identities ADD COLUMN avatar_hash TEXT",
      "ALTER TABLE discord_identities ADD COLUMN avatar_png TEXT",
      "ALTER TABLE discord_identities ADD COLUMN profile_at BIGINT"}},
};

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
