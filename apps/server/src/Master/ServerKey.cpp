#include "Master/ServerKey.hpp"

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

namespace rtype::server
{

void writeNewSecretFile(const std::filesystem::path &path, std::string_view content)
{
#ifdef _WIN32
    // Windows has no permission bits: the file gets the access rights of its directory
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
    const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
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
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            throw std::filesystem::filesystem_error("cannot write the file", path, error);
        }
        written += static_cast<std::size_t>(result);
    }
    if (::close(descriptor) != 0) {
        const std::error_code error(errno, std::system_category());
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        throw std::filesystem::filesystem_error("cannot write the file", path, error);
    }
#endif
}

void writePrivateKey(const rtype::network::ServerIdentity &identity, const std::filesystem::path &path)
{
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    writeNewSecretFile(path, identity.privateKeyPem());
}

std::optional<ServerKey> loadOrCreateKey(const std::filesystem::path &path, const rtype::log::Logger &logger)
{
    std::error_code missing;
    if (!std::filesystem::exists(path, missing) && !missing) {
        try {
            ServerKey key{rtype::network::ServerIdentity::generate(), true};
            writePrivateKey(key.identity, path);
            logger.warning(
                "No key in {}: a new one was generated there. Keep this file: with another key, the "
                "players who trusted this server refuse it",
                path.string());
            return key;
        } catch (const std::filesystem::filesystem_error &error) {
            // Another server made it in the meantime: its key is the one to use
            if (error.code() != std::errc::file_exists) {
                logger.error("Cannot write the key {}: {}", path.string(), error.code().message());
                return std::nullopt;
            }
        }
    }
    try {
        return ServerKey{rtype::network::ServerIdentity::load(path), false};
    } catch (const std::system_error &error) {
        logger.error("Cannot read the key {}: {}", path.string(), error.code().message());
    } catch (const std::invalid_argument &error) {
        logger.error("{} is not a key of the server: {}. Move it away, and the server makes a new key",
                     path.string(), error.what());
    }
    return std::nullopt;
}

} // namespace rtype::server
