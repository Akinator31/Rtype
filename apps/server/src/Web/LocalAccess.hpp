#pragma once

#include "Common/Secret.hpp"
#include "rtype/network/Address.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace rtype::server
{

/// The name of the file the master writes the address of its panel to, with the local access token, beside
/// its key (docs/architecture/web.md#local-access).
inline constexpr std::string_view AdminUrlFileName = "admin-url.txt";

/// A new local access token: 256 bits from the random generator of the cryptography, new at every start.
[[nodiscard]] Secret newAccessToken();

/// The address a browser opens the panel at, without a trailing slash: `public_url` when the settings give
/// one, else the address the panel listens on, `127.0.0.1` for an address that listens everywhere.
[[nodiscard]] std::string panelAddress(const rtype::network::Address &listening, std::string_view publicUrl);

/// The address that opens the panel with the token: `http://127.0.0.1:8080/?token=<64 hex digits>`.
[[nodiscard]] std::string accessUrl(std::string_view panel, const Secret &token);

/// Writes the address of accessUrl() to `file`, replacing the one of the previous start. On Unix the file is
/// readable by its owner only (0600): whoever reads it opens the panel.
/// @throws std::filesystem::filesystem_error if it cannot be written.
void writeAccessUrl(const std::filesystem::path &file, std::string_view url);

} // namespace rtype::server
