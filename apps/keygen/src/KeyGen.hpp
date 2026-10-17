#pragma once

#include "rtype/network/Tls.hpp"

#include <filesystem>
#include <optional>
#include <ostream>
#include <span>
#include <string_view>

/// The key generation tool, r-type_keygen: makes the key pair the server proves its identity with
/// (docs/guides/generate-the-server-keys.md). The program itself only calls run(): the rest is here for the
/// tests.
namespace rtype::keygen
{

/// Name of the private key file, which stays on the server.
inline constexpr std::string_view PrivateKeyFile = "server.key";
/// Name of the public key file, which is given with the client.
inline constexpr std::string_view PublicKeyFile = "server.pub";

/// Exit codes of the program.
enum ExitCode : int {
    Success = 0,
    /// The keys could not be written or read.
    Failure = 1,
    /// The arguments are invalid.
    InvalidArguments = 2,
};

/// What the command line asks for.
struct Options {
    /// Where to write the keys, created if missing.
    std::filesystem::path directory = ".";
    /// Replace the keys already in the directory.
    bool force = false;
    /// Only show the fingerprint of this key file, without writing anything.
    std::optional<std::filesystem::path> fingerprintOf;
    /// Only show the usage.
    bool help = false;
};

/// The usage shown by --help.
[[nodiscard]] std::string_view usage();

/// Reads the arguments of the command line, without the name of the program.
/// @throws std::invalid_argument if an option is unknown or misses its value, with a message for the user.
[[nodiscard]] Options parseArguments(std::span<const std::string_view> arguments);

/// Where writeKeyFiles() wrote the keys.
struct KeyFiles {
    std::filesystem::path privateKey;
    std::filesystem::path publicKey;
};

/// Writes the keys of an identity in a directory, which is created if missing: the private key in
/// PrivateKeyFile, only readable by its owner on Unix, and the public key in PublicKeyFile.
/// @param force Replace the files already there. Without it, nothing is written if one of them exists.
/// @throws std::filesystem::filesystem_error if a file exists (std::errc::file_exists) or cannot be written:
/// path1() is the file.
KeyFiles writeKeyFiles(const network::ServerIdentity &identity, const std::filesystem::path &directory,
                       bool force);

/// The fingerprint of a key file: a public key, or a private key whose public key is taken.
/// @throws std::system_error if the file cannot be read.
/// @throws std::invalid_argument if it holds no ECDSA P-256 key in PEM.
[[nodiscard]] network::KeyFingerprint fingerprintOfFile(const std::filesystem::path &path);

/// Runs the program: what it shows goes to `out`, the errors to the logs.
/// @return The exit code of the program.
[[nodiscard]] int run(std::span<const std::string_view> arguments, std::ostream &out);

} // namespace rtype::keygen
