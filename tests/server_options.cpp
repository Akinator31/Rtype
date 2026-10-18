// Tests of the command line of r-type_server and of the secret of its instances
// (docs/architecture/server.md).

#include "Common/Options.hpp"
#include "Common/Secret.hpp"
#include "Server.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace rtype::server;
using rtype::network::Address;

namespace
{

Options parse(std::vector<std::string_view> arguments)
{
    return parseOptions(arguments);
}

} // namespace

// --- Master ---

TEST_CASE("Without arguments, the server is a master with the default settings", "[server]")
{
    const Options options = parse({});

    CHECK(options.mode == Options::Mode::Master);
    CHECK(options.master.listen == Address::anyIpv4(4242));
    CHECK(options.master.firstGamePort == 4243);
    CHECK(options.master.lastGamePort == 4299);
    CHECK(options.master.key == "server.key");
    CHECK_FALSE(options.master.check);
    CHECK_FALSE(options.logLevel);
}

TEST_CASE("Every option of the master is read", "[server]")
{
    const Options options = parse({"--port", "5000", "--bind", "127.0.0.1", "--game-ports", "6000-6009",
                                   "--key", "/etc/rtype/server.key", "--check", "--log-level", "debug"});

    CHECK(options.mode == Options::Mode::Master);
    CHECK(options.master.listen == Address::loopbackIpv4(5000));
    CHECK(options.master.firstGamePort == 6000);
    CHECK(options.master.lastGamePort == 6009);
    CHECK(options.master.key == "/etc/rtype/server.key");
    CHECK(options.master.check);
    CHECK(options.logLevel == rtype::log::Level::Debug);
    CHECK(parse({"-p", "0", "-k", "a.key"}).master.listen == Address::anyIpv4(0));
}

TEST_CASE("Invalid options of the master are refused", "[server]")
{
    const std::vector<std::vector<std::string_view>> invalid{
        {"--port"},
        {"--port", "65536"},
        {"--port", "-1"},
        {"--port", "42a"},
        {"--bind", "localhost"},
        {"--game-ports", "4243"},
        {"--game-ports", "4300-4243"},
        {"--game-ports", "0-10"},
        {"--log-level", "loud"},
        {"--unknown"},
        {"4242"},
        {"--id", "3"},
        {"--standalone"},
    };

    for (const auto &arguments : invalid) {
        CAPTURE(arguments);
        CHECK_THROWS_AS(parse(arguments), std::invalid_argument);
    }
}

// --- Instance ---

TEST_CASE("An instance reads its id and ports", "[server]")
{
    const Options options = parse({"--instance", "--id", "7", "--udp-port", "4251", "--master-port", "5000"});

    CHECK(options.mode == Options::Mode::Instance);
    CHECK(options.instance.id == 7);
    CHECK(options.instance.udpPort == 4251);
    CHECK(options.instance.masterPort == 5000);
    CHECK_FALSE(options.instance.standalone);
}

TEST_CASE("An instance needs its id and ports, and refuses the options of the master", "[server]")
{
    const std::vector<std::vector<std::string_view>> invalid{
        {"--instance"},
        {"--instance", "--udp-port", "4251", "--master-port", "5000"},
        {"--instance", "--id", "7", "--master-port", "5000"},
        {"--instance", "--id", "7", "--udp-port", "4251"},
        {"--instance", "--id", "0", "--udp-port", "4251", "--master-port", "5000"},
        {"--instance", "--id", "7", "--udp-port", "4251", "--master-port", "5000", "--key", "server.key"},
        {"--instance", "--id", "7", "--udp-port", "4251", "--master-port", "5000", "--check"},
    };

    for (const auto &arguments : invalid) {
        CAPTURE(arguments);
        CHECK_THROWS_AS(parse(arguments), std::invalid_argument);
    }
}

TEST_CASE("The standalone mode only exists in debug builds, without a master", "[server]")
{
    if (!standaloneAvailable()) {
        CHECK_THROWS_AS(parse({"--instance", "--standalone", "--udp-port", "4242"}), std::invalid_argument);
        return;
    }
    const Options options = parse({"--instance", "--standalone", "--udp-port", "4242"});
    CHECK(options.instance.standalone);
    CHECK(options.instance.id == 1);
    CHECK_THROWS_AS(parse({"--instance", "--standalone", "--udp-port", "4242", "--master-port", "5000"}),
                    std::invalid_argument);
}

// --- Help, version, exit codes ---

TEST_CASE("--help and --version win over everything else", "[server]")
{
    CHECK(parse({"--unknown", "--help"}).mode == Options::Mode::Help);
    CHECK(parse({"-h"}).mode == Options::Mode::Help);
    CHECK(parse({"--instance", "--version"}).mode == Options::Mode::Version);
}

TEST_CASE("The program shows its help and version, and refuses invalid arguments", "[server]")
{
    std::ostringstream out;
    std::istringstream in;

    CHECK(run(std::vector<std::string_view>{"--help"}, out, in) == Success);
    CHECK(out.str() == usage());
    out.str("");
    CHECK(run(std::vector<std::string_view>{"--version"}, out, in) == Success);
    CHECK(out.str() == "r-type_server " + std::string(version()) + "\n");
    out.str("");
    CHECK(run(std::vector<std::string_view>{"--port", "nope"}, out, in) == InvalidArguments);
    CHECK(out.str().empty());
}

TEST_CASE("A master without its key does not start", "[server]")
{
    std::ostringstream out;
    std::istringstream in;

    CHECK(run(std::vector<std::string_view>{"--check", "--port", "0", "--key", "missing-rtype-test.key"}, out,
              in) == Failure);
}

TEST_CASE("An instance without a secret on its standard input does not start", "[server]")
{
    std::ostringstream out;
    std::istringstream nothing;
    std::istringstream wrong("not a secret\n");

    const std::vector<std::string_view> arguments{"--instance", "--id",          "7",   "--udp-port",
                                                  "4251",       "--master-port", "5000"};
    CHECK(run(arguments, out, nothing) == Failure);
    CHECK(run(arguments, out, wrong) == Failure);
}

// --- Secret ---

TEST_CASE("A secret survives a round trip through its text", "[server]")
{
    Secret secret{};
    for (std::size_t i = 0; i < secret.size(); ++i) {
        secret[i] = static_cast<std::byte>(i * 9);
    }

    const std::string text = formatSecret(secret);

    CHECK(text.size() == 64);
    CHECK(text.starts_with("0009121b"));
    CHECK(parseSecret(text) == secret);
    CHECK(parseSecret(text + "\r") == secret);
    std::istringstream input(text + "\n");
    CHECK(readSecret(input) == secret);
}

TEST_CASE("Anything but 64 lowercase hexadecimal digits is not a secret", "[server]")
{
    const std::string valid(64, 'a');
    std::string upper = valid;
    upper[10] = 'A';
    std::string other = valid;
    other[63] = 'g';

    CHECK(parseSecret(valid));
    CHECK_FALSE(parseSecret(upper));
    CHECK_FALSE(parseSecret(other));
    CHECK_FALSE(parseSecret(valid.substr(1)));
    CHECK_FALSE(parseSecret(valid + "a"));
    CHECK_FALSE(parseSecret(""));
    std::istringstream empty;
    CHECK_FALSE(readSecret(empty));
}
