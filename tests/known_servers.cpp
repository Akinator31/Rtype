// Tests of the keys the client trusts for each server: the official ones, those the player trusted at their
// first connection, and the development key (docs/decisions/0020-server-key-distribution.md).

#include "Online/KnownServers.hpp"
#include "Online/OfficialServers.hpp"
#include "Online/OnlineScenes.hpp"
#include "rtype/keys/DevelopmentKey.hpp"
#include "rtype/network/Tls.hpp"
#include "rtype/save/Save/Save.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

using rtype::client::KnownServers;
using rtype::client::OfficialServer;
using rtype::client::serverAddress;
using rtype::network::KeyFingerprint;
using rtype::network::ServerIdentity;
using Verdict = KnownServers::Verdict;

namespace
{

/// A save in a file of its own, removed after the test: a directory per test, since CTest runs them at once.
struct TemporarySave {
    std::filesystem::path path;
    rtype::save::Save save;

    explicit TemporarySave(const std::string &test)
        : path(std::filesystem::temp_directory_path() / ("rtype_tests_known_servers_" + test) / "save.txt"),
          save(path)
    {
        std::filesystem::remove_all(path.parent_path());
    }
    ~TemporarySave() { std::filesystem::remove_all(path.parent_path()); }
};

const KeyFingerprint MainKey = ServerIdentity::generate().fingerprint();
const KeyFingerprint BackupKey = ServerIdentity::generate().fingerprint();
const std::string MainHex = MainKey.toHex();
const std::string BackupHex = BackupKey.toHex();
const std::array<OfficialServer, 1> Officials{{
    {.name = "Official", .host = "RType.Example.org", .port = 4242, .keys = {MainHex, BackupHex}},
}};

} // namespace

TEST_CASE("A server is known by its host in lowercase and its port, an IPv6 address in brackets", "[online]")
{
    CHECK(serverAddress("RType.Example.org", 4242) == "rtype.example.org:4242");
    CHECK(serverAddress("127.0.0.1", 5000) == "127.0.0.1:5000");
    CHECK(serverAddress("::1", 4242) == "[::1]:4242");
}

TEST_CASE("The official servers are trusted with their main and backup keys, never asked", "[online]")
{
    TemporarySave file("official");
    const KnownServers known(file.save, Officials, false);
    const std::string official = serverAddress("rtype.example.org", 4242);

    CHECK(known.isOfficial(official));
    CHECK(known.trusted(official) == std::vector{MainKey, BackupKey});
    // Another key on the official address is never a first use
    CHECK(known.judge(official, ServerIdentity::generate().fingerprint()) == Verdict::Changed);
    // The same host on another port is another server
    CHECK_FALSE(known.isOfficial(serverAddress("rtype.example.org", 4243)));
}

TEST_CASE("The fingerprints of the official servers built into the client are valid", "[online]")
{
    for (const OfficialServer &server : rtype::client::OfficialServers) {
        CAPTURE(server.name);
        CHECK_FALSE(server.host.empty());
        for (const std::string_view key : server.keys) {
            CHECK(KeyFingerprint::parse(key));
        }
    }
}

TEST_CASE("An invalid fingerprint of an official server is skipped", "[online]")
{
    TemporarySave file("invalid_official");
    const std::array<OfficialServer, 1> broken{{
        {.name = "Broken", .host = "rtype.example.org", .port = 4242, .keys = {MainHex, "not a fingerprint"}},
    }};
    const KnownServers known(file.save, broken, false);

    CHECK(known.trusted("rtype.example.org:4242") == std::vector{MainKey});
}

TEST_CASE("A server never met is trusted on first use, then only with that key", "[online]")
{
    TemporarySave file("first_use");
    KnownServers known(file.save, Officials, false);
    const std::string address = serverAddress("10.0.0.7", 4242);
    const KeyFingerprint key = ServerIdentity::generate().fingerprint();

    // Nothing trusted: the connection fails and tells the key
    CHECK(known.trusted(address).empty());
    CHECK(known.judge(address, key) == Verdict::FirstUse);

    CHECK(known.trust(address, key));
    CHECK(known.remembered(address) == key);
    CHECK(known.trusted(address) == std::vector{key});
    // Another key afterwards: refused, never asked again
    CHECK(known.judge(address, ServerIdentity::generate().fingerprint()) == Verdict::Changed);

    SECTION("The key trusted is kept in the save, for the next runs")
    {
        rtype::save::Save next(file.path);
        next.readFile();
        CHECK(next.getString("servers.known.10.0.0.7:4242") == key.toHex());
        const KnownServers again(next, Officials, false);
        CHECK(again.trusted(address) == std::vector{key});
    }
    SECTION("Forgetting the server makes its next connection a first use")
    {
        CHECK(known.forget(address));
        CHECK_FALSE(known.remembered(address));
        CHECK(known.judge(address, ServerIdentity::generate().fingerprint()) == Verdict::FirstUse);
        CHECK_FALSE(known.forget(address));
        rtype::save::Save next(file.path);
        next.readFile();
        CHECK_FALSE(next.hasKey("servers.known.10.0.0.7:4242"));
    }
}

TEST_CASE("An official server cannot be forgotten, nor trusted with another key", "[online]")
{
    TemporarySave file("official_forget");
    KnownServers known(file.save, Officials, false);
    const std::string official = serverAddress("rtype.example.org", 4242);
    const KeyFingerprint impostor = ServerIdentity::generate().fingerprint();

    (void)known.trust(official, impostor);

    CHECK(known.trusted(official) == std::vector{MainKey, BackupKey});
    CHECK_FALSE(known.remembered(official));
    CHECK_FALSE(known.forget(official));
}

TEST_CASE("A key in the save that is not a fingerprint trusts nothing", "[online]")
{
    TemporarySave file("garbage");
    file.save.setString("servers.known.10.0.0.7:4242", "garbage");
    const KnownServers known(file.save, Officials, false);

    CHECK(known.trusted("10.0.0.7:4242").empty());
    CHECK_FALSE(known.remembered("10.0.0.7:4242"));
}

TEST_CASE("Debug builds trust the development key for every server, Release builds refuse it", "[online]")
{
    TemporarySave file("development");
    const KeyFingerprint development = rtype::keys::developmentFingerprint();
    const std::string address = serverAddress("10.0.0.7", 4242);

    SECTION("Trusted")
    {
        const KnownServers known(file.save, Officials, true);
        CHECK(known.trusted(address) == std::vector{development});
        CHECK(known.trusted("rtype.example.org:4242") == std::vector{MainKey, BackupKey, development});
    }
    SECTION("Refused, even on first use")
    {
        const KnownServers known(file.save, Officials, false);
        CHECK(known.trusted(address).empty());
        CHECK(known.judge(address, development) == Verdict::DevelopmentKey);
        CHECK(known.judge("rtype.example.org:4242", development) == Verdict::DevelopmentKey);
    }
    SECTION("This build")
    {
        const KnownServers known(file.save, Officials);
        CHECK(known.trusted(address).empty() == !rtype::keys::DevelopmentKeyTrusted);
    }
}

TEST_CASE("The development key is the one committed in keys/development", "[online]")
{
    CHECK(rtype::keys::developmentFingerprint() ==
          KeyFingerprint::loadPublicKey(RTYPE_TEST_DEVELOPMENT_KEYS "/server.pub"));
}

TEST_CASE("A fingerprint is shown in groups of 4 digits", "[online]")
{
    const auto fingerprint =
        *KeyFingerprint::parse("72a6bbda0123456789abcdef0123456789abcdef0123456789abcdef01239e87");

    CHECK(rtype::client::fingerprintLines(fingerprint) ==
          std::vector<std::string>{"72a6 bbda 0123 4567 89ab cdef 0123 4567",
                                   "89ab cdef 0123 4567 89ab cdef 0123 9e87"});
    CHECK(rtype::client::fingerprintLines(fingerprint, 16).size() == 1);
    CHECK(rtype::client::fingerprintLines(fingerprint, 4).size() == 4);
}
