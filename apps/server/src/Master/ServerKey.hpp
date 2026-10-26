#pragma once

#include "rtype/log/Logger.hpp"
#include "rtype/network/Tls.hpp"

#include <filesystem>
#include <optional>

namespace rtype::server
{

/// What loadOrCreateKey() found.
struct ServerKey {
    rtype::network::ServerIdentity identity;
    /// The file did not exist: the key was generated and written there.
    bool generated = false;
};

/// The key of the master (ADR 0021): the one in this file, or a new one written there if the file does not
/// exist. A file that exists is never replaced.
/// @return Nothing, with the reason logged, if the file cannot be read or written, or holds no key.
[[nodiscard]] std::optional<ServerKey> loadOrCreateKey(const std::filesystem::path &path,
                                                       const rtype::log::Logger &logger);

/// Writes a private key in a file that does not exist yet, only readable by its owner on Unix. The
/// directories of the path are created if missing.
/// @throws std::filesystem::filesystem_error if the file exists (std::errc::file_exists) or cannot be
/// written: path1() is the file.
void writePrivateKey(const rtype::network::ServerIdentity &identity, const std::filesystem::path &path);

} // namespace rtype::server
