#pragma once

#include "rtype/log/Logger.hpp"
#include "rtype/network/Tls.hpp"

#include <filesystem>
#include <optional>
#include <string_view>

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

/// Creates a file that does not exist yet and writes it. On Unix it is created readable by its owner only, so
/// that what it holds is never readable by others, even for a moment; on Windows it gets the access rights of
/// its directory.
/// @throws std::filesystem::filesystem_error if the file exists (std::errc::file_exists) or cannot be
/// written: path1() is the file.
void writeNewSecretFile(const std::filesystem::path &path, std::string_view content);

} // namespace rtype::server
