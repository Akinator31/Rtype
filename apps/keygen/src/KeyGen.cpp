#include "KeyGen.hpp"

#include "rtype/log/Logger.hpp"

#include <exception>
#include <format>
#include <stdexcept>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <fstream>
#else
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#endif

namespace rtype::keygen
{

namespace
{

constexpr std::string_view Usage = R"(Usage: r-type_keygen [--out <directory>] [--force]
       r-type_keygen --fingerprint <file>

Generates the key pair the server proves its identity with:
  server.key  the private key, to keep on the server only and never share
  server.pub  the public key, to give with the client

Options:
  -o, --out <directory>     Where to write the keys, created if missing (default: the current directory)
  -f, --force               Replace the keys already there: the clients that pin the old key will refuse
                            the server until they get the new server.pub
      --fingerprint <file>  Show the fingerprint of a key file (server.key or server.pub), write nothing
  -h, --help                Show this help

Exit codes: 0 on success, 1 if the keys cannot be written or read, 2 if the arguments are invalid.
Logs: set RTYPE_LOG_LEVEL to trace, debug, info, warning or error.
)";

/// Creates a file that does not exist yet and writes it. A secret file is created only readable by its owner,
/// so that the private key is never readable by others, even for a moment.
void writeNewFile(const std::filesystem::path &path, std::string_view content, bool secret)
{
#ifdef _WIN32
    // Windows has no permission bits: the file gets the access rights of its directory
    (void)secret;
    if (std::filesystem::exists(path)) {
        throw std::filesystem::filesystem_error("cannot create the file", path,
                                                std::make_error_code(std::errc::file_exists));
    }
    std::ofstream file(path, std::ios::binary);
    file << content;
    file.close();
    if (!file) {
        throw std::filesystem::filesystem_error("cannot write the file", path,
                                                std::make_error_code(std::errc::io_error));
    }
#else
    // O_EXCL: fails if the file exists, instead of writing through it
    const int descriptor =
        ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, secret ? 0600 : 0644);
    if (descriptor < 0) {
        throw std::filesystem::filesystem_error("cannot create the file", path,
                                                std::error_code(errno, std::system_category()));
    }
    std::size_t written = 0;
    while (written < content.size()) {
        const ssize_t result = ::write(descriptor, content.data() + written, content.size() - written);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result < 0) {
            const std::error_code error(errno, std::system_category());
            ::close(descriptor);
            throw std::filesystem::filesystem_error("cannot write the file", path, error);
        }
        written += static_cast<std::size_t>(result);
    }
    if (::close(descriptor) != 0) {
        throw std::filesystem::filesystem_error("cannot write the file", path,
                                                std::error_code(errno, std::system_category()));
    }
#endif
}

} // namespace

std::string_view usage()
{
    return Usage;
}

Options parseArguments(std::span<const std::string_view> arguments)
{
    Options options;
    bool outGiven = false;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string_view argument = arguments[i];
        const auto value = [&]() {
            if (i + 1 >= arguments.size() || arguments[i + 1].empty()) {
                throw std::invalid_argument(std::format("{} needs a value", argument));
            }
            return std::filesystem::path(std::string(arguments[++i]));
        };
        if (argument == "-h" || argument == "--help") {
            options.help = true;
        } else if (argument == "-o" || argument == "--out") {
            options.directory = value();
            outGiven = true;
        } else if (argument == "-f" || argument == "--force") {
            options.force = true;
        } else if (argument == "--fingerprint") {
            options.fingerprintOf = value();
        } else {
            throw std::invalid_argument(std::format("unknown argument '{}'", argument));
        }
    }
    if (options.fingerprintOf && (outGiven || options.force)) {
        throw std::invalid_argument("--fingerprint writes nothing: it cannot be used with --out or --force");
    }
    return options;
}

KeyFiles writeKeyFiles(const network::ServerIdentity &identity, const std::filesystem::path &directory,
                       bool force)
{
    std::filesystem::create_directories(directory);
    const KeyFiles files{directory / PrivateKeyFile, directory / PublicKeyFile};

    // Checks both files before writing any, so that a refusal leaves the directory as it was
    for (const auto &file : {files.privateKey, files.publicKey}) {
        if (force) {
            // Removed rather than written through, so that the new file gets the permissions of a new key
            std::filesystem::remove(file);
        } else if (std::filesystem::exists(file)) {
            throw std::filesystem::filesystem_error("the key already exists", file,
                                                    std::make_error_code(std::errc::file_exists));
        }
    }

    writeNewFile(files.privateKey, identity.privateKeyPem(), true);
    try {
        writeNewFile(files.publicKey, identity.publicKeyPem(), false);
    } catch (...) {
        // A private key without its public key is of no use: leaves nothing rather than half a pair
        std::error_code ignored;
        std::filesystem::remove(files.privateKey, ignored);
        throw;
    }
    return files;
}

network::KeyFingerprint fingerprintOfFile(const std::filesystem::path &path)
{
    try {
        return network::KeyFingerprint::loadPublicKey(path);
    } catch (const std::invalid_argument &) {
        // Not a public key: maybe the private key, which holds the public key too
    }
    try {
        return network::ServerIdentity::load(path).fingerprint();
    } catch (const std::invalid_argument &) {
        throw std::invalid_argument(std::format("{} holds no ECDSA P-256 key in PEM", path.string()));
    }
}

int run(std::span<const std::string_view> arguments, std::ostream &out)
{
    const log::Logger logger("keygen");

    Options options;
    try {
        options = parseArguments(arguments);
    } catch (const std::invalid_argument &error) {
        logger.error("{} (see --help)", error.what());
        return InvalidArguments;
    }
    if (options.help) {
        out << Usage;
        return Success;
    }

    if (options.fingerprintOf) {
        try {
            out << fingerprintOfFile(*options.fingerprintOf).toHex() << '\n';
            return Success;
        } catch (const std::system_error &error) {
            logger.error("Cannot read {}: {}", options.fingerprintOf->string(), error.code().message());
        } catch (const std::exception &error) {
            logger.error("{}", error.what());
        }
        return Failure;
    }

    try {
        const network::ServerIdentity identity = network::ServerIdentity::generate();
        if (options.force) {
            logger.warning(
                "Replacing the keys in {}: the clients that pin the old key will refuse this server "
                "until they get the new {}",
                options.directory.string(), PublicKeyFile);
        }
        const KeyFiles files = writeKeyFiles(identity, options.directory, options.force);
        logger.debug("Keys written in {}", options.directory.string());
        out << "Private key: " << files.privateKey.string()
            << "  (keep it on the server only, never share it)\n"
            << "Public key:  " << files.publicKey.string() << "  (give it with the client)\n"
            << "Fingerprint: " << identity.fingerprint().toHex() << '\n';
        return Success;
    } catch (const std::filesystem::filesystem_error &error) {
        if (error.code() == std::errc::file_exists) {
            logger.error(
                "{} already exists: a new key would make the clients refuse the server. Use --force to "
                "replace it",
                error.path1().string());
        } else {
            logger.error("Cannot write {}: {}", error.path1().string(), error.code().message());
        }
    } catch (const std::exception &error) {
        logger.error("Cannot generate the keys: {}", error.what());
    }
    return Failure;
}

} // namespace rtype::keygen
