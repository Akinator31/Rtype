#include "KeyGen.hpp"

#include "rtype/network/Tls.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace rtype::keygen;
using rtype::network::KeyFingerprint;
using rtype::network::ServerIdentity;

namespace
{

// A directory of its own for each test, removed at the end
struct TempDirectory {
    std::filesystem::path path;

    explicit TempDirectory(std::string_view name)
        : path(std::filesystem::temp_directory_path() / std::string("rtype_tests_keygen_").append(name))
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

int runWith(std::vector<std::string_view> arguments, std::string &output)
{
    std::ostringstream out;
    const int code = run(arguments, out);
    output = out.str();
    return code;
}

} // namespace

// --- Arguments ---

TEST_CASE("Without arguments, the keys are written in the current directory", "[keygen]")
{
    const Options options = parseArguments({});

    CHECK(options.directory == ".");
    CHECK_FALSE(options.force);
    CHECK_FALSE(options.fingerprintOf);
    CHECK_FALSE(options.help);
}

TEST_CASE("Every option is read, in its short and long form", "[keygen]")
{
    const std::vector<std::string_view> longForm{"--out", "keys", "--force"};
    const std::vector<std::string_view> shortForm{"-f", "-o", "keys"};

    for (const auto &arguments : {longForm, shortForm}) {
        const Options options = parseArguments(arguments);
        CHECK(options.directory == "keys");
        CHECK(options.force);
    }
    CHECK(parseArguments(std::vector<std::string_view>{"-h"}).help);
    CHECK(parseArguments(std::vector<std::string_view>{"--help"}).help);
    CHECK(parseArguments(std::vector<std::string_view>{"--fingerprint", "server.pub"}).fingerprintOf ==
          "server.pub");
}

TEST_CASE("Invalid arguments are refused", "[keygen]")
{
    const std::vector<std::vector<std::string_view>> invalid{
        {"--unknown"},
        {"keys"},
        {"--out"},
        {"--out", ""},
        {"--fingerprint"},
        {"--fingerprint", "server.pub", "--force"},
        {"--fingerprint", "server.pub", "--out", "keys"},
    };

    for (const auto &arguments : invalid) {
        CHECK_THROWS_AS(parseArguments(arguments), std::invalid_argument);
    }
}

// --- Key files ---

TEST_CASE("The written keys are the identity's, and the server and the client can load them", "[keygen]")
{
    const TempDirectory directory("written");
    const ServerIdentity identity = ServerIdentity::generate();

    const KeyFiles files = writeKeyFiles(identity, directory.path / "nested" / "keys", false);

    CHECK(files.privateKey == directory.path / "nested" / "keys" / "server.key");
    CHECK(files.publicKey == directory.path / "nested" / "keys" / "server.pub");
    CHECK(readFile(files.privateKey) == identity.privateKeyPem());
    CHECK(readFile(files.publicKey) == identity.publicKeyPem());
    CHECK(ServerIdentity::load(files.privateKey).fingerprint() == identity.fingerprint());
    CHECK(KeyFingerprint::loadPublicKey(files.publicKey) == identity.fingerprint());
}

#ifndef _WIN32
TEST_CASE("Only its owner can read the private key", "[keygen]")
{
    using std::filesystem::perms;
    const TempDirectory directory("permissions");

    const KeyFiles files = writeKeyFiles(ServerIdentity::generate(), directory.path, false);

    CHECK((std::filesystem::status(files.privateKey).permissions() &
           (perms::group_all | perms::others_all)) == perms::none);
    CHECK((std::filesystem::status(files.publicKey).permissions() & perms::others_read) ==
          perms::others_read);
}

TEST_CASE("A replaced private key is only readable by its owner, whatever the old file allowed", "[keygen]")
{
    using std::filesystem::perms;
    const TempDirectory directory("replaced_permissions");
    const KeyFiles files = writeKeyFiles(ServerIdentity::generate(), directory.path, false);
    std::filesystem::permissions(files.privateKey, perms::all);

    writeKeyFiles(ServerIdentity::generate(), directory.path, true);

    CHECK((std::filesystem::status(files.privateKey).permissions() &
           (perms::group_all | perms::others_all)) == perms::none);
}
#endif

TEST_CASE("Existing keys are kept unless forced", "[keygen]")
{
    const TempDirectory directory("existing");
    const ServerIdentity first = ServerIdentity::generate();
    const ServerIdentity second = ServerIdentity::generate();
    const KeyFiles files = writeKeyFiles(first, directory.path, false);

    try {
        writeKeyFiles(second, directory.path, false);
        FAIL("the existing keys were replaced");
    } catch (const std::filesystem::filesystem_error &error) {
        CHECK(error.code() == std::errc::file_exists);
        CHECK(error.path1() == files.privateKey);
    }
    CHECK(KeyFingerprint::loadPublicKey(files.publicKey) == first.fingerprint());

    writeKeyFiles(second, directory.path, true);
    CHECK(ServerIdentity::load(files.privateKey).fingerprint() == second.fingerprint());
    CHECK(KeyFingerprint::loadPublicKey(files.publicKey) == second.fingerprint());
}

TEST_CASE("A lone public key also stops the writing, which leaves nothing behind", "[keygen]")
{
    const TempDirectory directory("lone_public_key");
    std::filesystem::create_directories(directory.path);
    std::ofstream(directory.path / "server.pub") << "old";

    CHECK_THROWS_AS(writeKeyFiles(ServerIdentity::generate(), directory.path, false),
                    std::filesystem::filesystem_error);
    CHECK_FALSE(std::filesystem::exists(directory.path / "server.key"));
    CHECK(readFile(directory.path / "server.pub") == "old");
}

TEST_CASE("The fingerprint of both key files is the one of the key", "[keygen]")
{
    const TempDirectory directory("fingerprint");
    const ServerIdentity identity = ServerIdentity::generate();
    const KeyFiles files = writeKeyFiles(identity, directory.path, false);
    std::ofstream(directory.path / "other.txt") << "not a key";

    CHECK(fingerprintOfFile(files.privateKey) == identity.fingerprint());
    CHECK(fingerprintOfFile(files.publicKey) == identity.fingerprint());
    CHECK_THROWS_AS(fingerprintOfFile(directory.path / "other.txt"), std::invalid_argument);
    CHECK_THROWS_AS(fingerprintOfFile(directory.path / "missing.pub"), std::system_error);
}

// --- Program ---

TEST_CASE("The program writes the keys and shows their fingerprint", "[keygen]")
{
    const TempDirectory directory("program");
    const std::string out = directory.path.string();
    std::string output;

    REQUIRE(runWith({"--out", out}, output) == Success);

    const std::string fingerprint = KeyFingerprint::loadPublicKey(directory.path / "server.pub").toHex();
    CHECK(output.find("Fingerprint: " + fingerprint) != std::string::npos);

    std::string shown;
    CHECK(runWith({"--fingerprint", (directory.path / "server.key").string()}, shown) == Success);
    CHECK(shown == fingerprint + "\n");
}

TEST_CASE("The program fails without touching existing keys, and on invalid arguments", "[keygen]")
{
    const TempDirectory directory("program_failures");
    const std::string out = directory.path.string();
    std::string output;
    REQUIRE(runWith({"--out", out}, output) == Success);
    const std::string key = readFile(directory.path / "server.key");

    CHECK(runWith({"--out", out}, output) == Failure);
    CHECK(output.empty());
    CHECK(readFile(directory.path / "server.key") == key);

    CHECK(runWith({"--fingerprint", (directory.path / "missing.pub").string()}, output) == Failure);
    CHECK(runWith({"--unknown"}, output) == InvalidArguments);
    CHECK(runWith({"--help"}, output) == Success);
    CHECK(output == usage());
}
