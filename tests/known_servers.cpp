// Tests of the keys the client trusts for each server: the one the player trusted at its first connection
// (docs/decisions/0021-server-generated-key-trust-on-first-use.md).

#include "Online/KnownServers.hpp"
#include "Online/OnlineScenes.hpp"
#include "rtype/network/Tls.hpp"
#include "rtype/save/Save/Save.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

using rtype::client::KnownServers;
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

} // namespace

TEST_CASE("A server is known by its host in lowercase and its port, an IPv6 address in brackets", "[online]")
{
    CHECK(serverAddress("RType.Example.org", 4242) == "rtype.example.org:4242");
    CHECK(serverAddress("127.0.0.1", 5000) == "127.0.0.1:5000");
    CHECK(serverAddress("::1", 4242) == "[::1]:4242");
}

TEST_CASE("A server never met is trusted on first use, then only with that key", "[online]")
{
    TemporarySave file("first_use");
    KnownServers known(file.save);
    const std::string address = serverAddress("10.0.0.7", 4242);
    const KeyFingerprint key = ServerIdentity::generate().fingerprint();

    // Nothing trusted: the connection fails and tells the key
    CHECK(known.trusted(address).empty());
    CHECK(known.judge(address) == Verdict::FirstUse);

    CHECK(known.trust(address, key));
    CHECK(known.remembered(address) == key);
    CHECK(known.trusted(address) == std::vector{key});
    // Another key afterwards: refused, never asked again
    CHECK(known.judge(address) == Verdict::Changed);

    SECTION("The key trusted is kept in the save, for the next runs")
    {
        rtype::save::Save next(file.path);
        next.readFile();
        CHECK(next.getString("servers.known.10.0.0.7:4242") == key.toHex());
        const KnownServers again(next);
        CHECK(again.trusted(address) == std::vector{key});
    }
    SECTION("Forgetting the server makes its next connection a first use")
    {
        CHECK(known.forget(address));
        CHECK_FALSE(known.remembered(address));
        CHECK(known.judge(address) == Verdict::FirstUse);
        CHECK_FALSE(known.forget(address));
        rtype::save::Save next(file.path);
        next.readFile();
        CHECK_FALSE(next.hasKey("servers.known.10.0.0.7:4242"));
    }
}

TEST_CASE("A key in the save that is not a fingerprint trusts nothing", "[online]")
{
    TemporarySave file("garbage");
    file.save.setString("servers.known.10.0.0.7:4242", "garbage");
    const KnownServers known(file.save);

    CHECK(known.trusted("10.0.0.7:4242").empty());
    CHECK_FALSE(known.remembered("10.0.0.7:4242"));
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
