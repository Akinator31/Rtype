// Tests of the key of the master: generated at its first start, then kept
// (docs/decisions/0021-server-generated-key-trust-on-first-use.md).

#include "Common/Options.hpp"
#include "Master/ServerKey.hpp"
#include "Server.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/Tls.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace rtype::server;
using rtype::network::ServerIdentity;

namespace
{

// A directory of its own for each test, removed at the end: CTest runs them at once
struct TempDirectory {
    std::filesystem::path path;

    explicit TempDirectory(std::string_view name)
        : path(std::filesystem::temp_directory_path() / std::string("rtype_tests_server_key_").append(name))
    {
        std::filesystem::remove_all(path);
    }
    ~TempDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    TempDirectory(const TempDirectory &) = delete;
    TempDirectory &operator=(const TempDirectory &) = delete;
};

std::string readFile(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("tests");
    return instance;
}

int check(const std::filesystem::path &key)
{
    std::ostringstream out;
    std::istringstream in;
    const std::string keyArgument = key.string();
    // In the directory of the key, rather than rtype.db in the working directory of the tests
    const std::string databaseArgument = "sqlite:" + (key.parent_path() / "rtype.db").string();
    return run(std::vector<std::string_view>{"--check", "--port", "0", "--config", RTYPE_TEST_CONFIG, "--key",
                                             keyArgument, "--database", databaseArgument},
               out, in);
}

int fingerprint(const std::filesystem::path &key, std::string &output)
{
    std::ostringstream out;
    std::istringstream in;
    const std::string keyArgument = key.string();
    const int code = run(std::vector<std::string_view>{"--fingerprint", "--key", keyArgument}, out, in);
    output = out.str();
    return code;
}

} // namespace

TEST_CASE("Without a key file, the key is generated there, then the same key is loaded", "[server]")
{
    const TempDirectory directory("generated");
    const std::filesystem::path path = directory.path / "nested" / "server.key";

    const auto first = loadOrCreateKey(path, logger());
    REQUIRE(first);
    CHECK(first->generated);
    CHECK(readFile(path) == first->identity.privateKeyPem());

    const auto second = loadOrCreateKey(path, logger());
    REQUIRE(second);
    CHECK_FALSE(second->generated);
    CHECK(second->identity.fingerprint() == first->identity.fingerprint());
}

#ifndef _WIN32
TEST_CASE("Only its owner can read the generated key", "[server]")
{
    using std::filesystem::perms;
    const TempDirectory directory("permissions");
    const std::filesystem::path path = directory.path / "server.key";

    REQUIRE(loadOrCreateKey(path, logger()));

    CHECK((std::filesystem::status(path).permissions() & (perms::group_all | perms::others_all)) ==
          perms::none);
}

TEST_CASE("A key that cannot be written stops the master", "[server]")
{
    const TempDirectory directory("read_only");
    std::filesystem::create_directories(directory.path);
    std::filesystem::permissions(directory.path,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
    if (std::ofstream(directory.path / "probe")) {
        SKIP("The directory stays writable: the tests run as root");
    }

    CHECK_FALSE(loadOrCreateKey(directory.path / "server.key", logger()));
    std::filesystem::permissions(directory.path, std::filesystem::perms::owner_all);
}
#endif

TEST_CASE("A file that is not a key stops the master, and is never replaced", "[server]")
{
    const TempDirectory directory("not_a_key");
    std::filesystem::create_directories(directory.path);
    const std::filesystem::path path = directory.path / "server.key";
    std::ofstream(path) << "not a key";

    CHECK_FALSE(loadOrCreateKey(path, logger()));
    CHECK(check(path) == Failure);
    CHECK(readFile(path) == "not a key");
}

TEST_CASE("A private key is never written over an existing file", "[server]")
{
    const TempDirectory directory("existing");
    std::filesystem::create_directories(directory.path);
    const std::filesystem::path path = directory.path / "server.key";
    std::ofstream(path) << "old";

    try {
        writePrivateKey(ServerIdentity::generate(), path);
        FAIL("the existing file was replaced");
    } catch (const std::filesystem::filesystem_error &error) {
        CHECK(error.code() == std::errc::file_exists);
        CHECK(error.path1() == path);
    }
    CHECK(readFile(path) == "old");
}

TEST_CASE("A master makes its key at its first start and shows its fingerprint", "[server]")
{
    const TempDirectory directory("program");
    const std::filesystem::path path = directory.path / "server.key";
    std::string shown;

    // --fingerprint never generates a key
    CHECK(fingerprint(path, shown) == Failure);
    CHECK_FALSE(std::filesystem::exists(path));

    REQUIRE(check(path) == Success);
    const std::string key = readFile(path);
    REQUIRE(check(path) == Success);
    CHECK(readFile(path) == key);

    CHECK(fingerprint(path, shown) == Success);
    CHECK(shown == ServerIdentity::load(path).fingerprint().toHex() + "\n");
}
